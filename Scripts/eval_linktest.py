#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (c) 2019 - 2021, ETH Zurich, Computer Engineering Group (TEC)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


@author: romantrueb
@brief:  Anaylse dpp2linktest measurements
"""

import sys
import os
import numpy as np
import pandas as pd
import json
from collections import OrderedDict
import pickle

# construct html with python
import dominate
from dominate.tags import *
from dominate.util import raw

from flocklab import Flocklab
from flocklab import *

fl = Flocklab()

################################################################################

assertionOverride = False
outputDir = './data'


################################################################################
# Helper Functions
################################################################################

def getJson(text):
    '''Find an convert json in a single line from serial output. Returns None if no valid json could be found.
    '''
    ret = None
    # find index
    idx = 0
    if not '{' in text:
        return ret
    for i in range(len(text)):
        if text[i] == '{':
            idx = i
            break

    try:
        ret =  json.loads(text[idx:], strict=False)
    except json.JSONDecodeError:
        print('WARNING: json could not be parsed: {}'.format(text[idx:]))
    return ret


def getRows(nodeOfRound, df):
    '''Extract rows for requested round from df
    Args:
        nodeOfRound: nodeID which identifies the round
        df: dataframe containing all rows to search through
    '''
    inRange = False
    ret = []
    for d in df.data.to_list():
        if d['type'] == 'StartOfRound':
            if d['node'] == nodeOfRound:
                inRange = True
        elif d['type'] == 'EndOfRound':
            if d['node'] == nodeOfRound:
                break
        elif inRange:
            ret.append(d)
    return ret


def styleDf(df, cmap='inferno', format='{:.1f}', replaceNan=True, applymap=None):
    ret = ( df.style
            .background_gradient(cmap=cmap, axis=None)
            .format(format) )
    if applymap is not None:
        ret = ret.applymap(applymap)

    ret = ret.render()

    if replaceNan:
        ret = ret.replace('nan','')

    return ret


htmlStyleBlock = '''
    table, th, td {font-size:10pt; border:1px solid lightgrey; border-collapse:collapse; text-align:left; font-family:arial;}
    th, td {padding: 5px; text-align:center; width:22px;}
    table.outer, th.outer, td.outer {font-size:10pt; border:0px solid lightgrey; border-collapse:collapse; text-align:left; font-family:arial;}
    th.outer, td.outer {padding: 5px; text-align:center;}
'''

################################################################################

def extractData(testNo, testDir):
    serialPath = os.path.join(testDir, "{}/serial.csv".format(testNo))

    # download test results if directory does not exist
    if not os.path.isfile(serialPath):
        fl.getResults(testNo)

    df = fl.serial2Df(serialPath, error='ignore')
    df.sort_values(by=['timestamp', 'observer_id'], inplace=True, ignore_index=True)

    # convert output with valid json to dict and remove other rows
    keepMask = []
    resList = []
    for idx, row in df.iterrows():
        jsonDict = getJson(row['output'])
        keepMask.append(1 if jsonDict else 0)
        if jsonDict:
            resList.append(jsonDict)
    dfd = df[np.asarray(keepMask).astype(bool)].copy()
    dfd['data'] = resList

    nodeList = sorted(dfd.observer_id.unique())

    groups = dfd.groupby('observer_id')

    # Get TestConfig and RadioConfig & check for consistency
    testConfigDict = OrderedDict()
    radioConfigDict = OrderedDict()
    floodConfigDict = OrderedDict()
    for node in nodeList:
        testConfigFound = False
        radioConfigFound = False
        floodConfigFound = False
        testConfigDict[node] = None
        radioConfigDict[node] = None
        floodConfigDict[node] = None
        gDf = groups.get_group(node)
        for d in gDf.data.to_list():
            if d['type'] == 'TestConfig':
                testConfigDict[node] = d
                testConfigFound = True
            if d['type'] == 'RadioConfig':
                radioConfigDict[node] = d
                radioConfigFound = True
            if d['type'] == 'FloodConfig':
                floodConfigDict[node] = d
                floodConfigFound = True
            if testConfigFound and (radioConfigFound or floodConfigFound):
                break



    testConfig = testConfigDict[nodeList[0]]
    if not('p2pMode' in testConfig) and (len(radioConfigDict) == len(nodeList)):
        # backwards compatibility for test results without linktest mode indication
        testConfig['p2pMode'] = 1
        testConfig['floodMode'] = 0

    if testConfig['p2pMode'] and not testConfig['floodMode']:
        radioConfig = radioConfigDict[nodeList[0]]
        floodConfig = None
    elif testConfig['floodMode'] and not testConfig['p2pMode']:
        radioConfig = None
        floodConfig = floodConfigDict[nodeList[0]]
    else:
        raise Exception('TestConfig seems invalid!')

    # check configs for consistency
    for node in nodeList:
        assert testConfigDict[nodeList[0]] == testConfigDict[node]
        if len(radioConfigDict) == len(nodeList):
            assert radioConfigDict[nodeList[0]] == radioConfigDict[node]
        if len(floodConfigDict) == len(nodeList):
            assert floodConfigDict[nodeList[0]] == floodConfigDict[node]

    # Make sure that round boundaries do not overlap
    if not assertionOverride:
        stack = []
        currentNode = -1
        for d in dfd.data.to_list():
            if d['type'] == 'StartOfRound':
                if len(stack)==0:
                    currentNode = d['node']
                else:
                    assert d['node'] == stack[-1]
                stack.append(d['node'])
            elif d['type'] == 'EndOfRound':
                assert d['node'] == stack.pop()

    # collect extracted data (including nodeList to resolve idx <-> node ID relations)
    d = {
        'testConfig': testConfig,
        'nodeList': nodeList,
    }
    if testConfig['p2pMode'] and (not testConfig['floodMode']):
        pathlossMatrix, prrMatrix, crcErrorMatrix = extractP2pStats(dfd, testConfig, radioConfig)
        d['radioConfig'] = radioConfig
        d['prrMatrix'] = prrMatrix
        d['crcErrorMatrix'] = crcErrorMatrix
        d['pathlossMatrix'] = pathlossMatrix
    elif testConfig['floodMode'] and (not testConfig['p2pMode']):
        if floodConfig['delayTx'] == 0:
            numFloodsRxMatrix, hopDistanceMatrix, hopDistanceStdMatrix = extractFloodNormal(dfd, testConfig, floodConfig)
        else: # floods with delayed Tx
            numFloodsRxMatrix, hopDistanceMatrix, hopDistanceStdMatrix = extractFloodDelayedTx(dfd, testConfig, floodConfig)
        d['floodConfig'] = floodConfig
        d['numFloodsRxMatrix'] = numFloodsRxMatrix
        d['hopDistanceMatrix'] = hopDistanceMatrix
        d['hopDistanceStdMatrix'] = hopDistanceStdMatrix
    # save obtained data to file
    pklPath = os.path.join(outputDir, 'linktest_data_{}.pkl'.format(testNo))
    os.makedirs(os.path.split(pklPath)[0], exist_ok=True)
    with open(pklPath, 'wb' ) as f:
        pickle.dump(d, f)

    return d


def extractP2pStats(dfd, testConfig, radioConfig):
    groups = dfd.groupby('observer_id')
    nodeList = sorted(dfd.observer_id.unique())
    numNodes = len(nodeList)

    # prepare
    pathlossMatrix = np.empty( (numNodes, numNodes,) ) * np.nan  # path loss
    prrMatrix = np.empty( (numNodes, numNodes,) ) * np.nan       # packet reception ratio (PRR)
    crcErrorMatrix = np.empty( (numNodes, numNodes,) ) * np.nan  # ratio of packets with CRC error

    # iterate over rounds
    for nodeOfRound in nodeList:
        txNode = nodeOfRound
        txNodeIdx = nodeList.index(txNode)

        numTx = 0
        numRxDict = OrderedDict()
        numCrcErrorDict = OrderedDict()
        rssiAvgDict = OrderedDict()
        # iterate over nodes
        for node in nodeList:
            rows = getRows(nodeOfRound, groups.get_group(node))
            if node == txNode:
                txDoneList = [elem for elem in rows if (elem['type']=='TxDone')]
                numTx = len(txDoneList)
                assert numTx == testConfig['numTx']
            else:
                rxDoneList = [elem for elem in rows if (elem['type']=='RxDone' and elem['key']==testConfig['key'] and elem['crc_error']==0)]
                crcErrorList = [elem for elem in rows if (elem['type']=='RxDone' and elem['crc_error']==1)]
                numRxDict[node] = len(rxDoneList)
                numCrcErrorDict[node] = len(crcErrorList)
                rssiAvgDict[node] = np.mean([elem['rssi'] for elem in rxDoneList]) if len(rxDoneList) else np.nan

        # fill path loss matrix
        for rxNode, rssi in rssiAvgDict.items():
            rxNodeIdx = nodeList.index(rxNode)
            pathlossMatrix[txNodeIdx][rxNodeIdx] = -(rssi - radioConfig['txPower'])

        # fill PRR matrix
        for rxNode, numRx in numRxDict.items():
            rxNodeIdx = nodeList.index(rxNode)
            prrMatrix[txNodeIdx][rxNodeIdx] = numRx/numTx

        # fill CRC error matrix
        for rxNode, numCrcError in numCrcErrorDict.items():
            rxNodeIdx = nodeList.index(rxNode)
            crcErrorMatrix[txNodeIdx][rxNodeIdx] = numCrcError/numTx
        # NOTE: some CRC error cases are ignored while getting the rows (getRows()) because the json parser cannot parse the RxDone output

    return pathlossMatrix, prrMatrix, crcErrorMatrix


def extractFloodNormal(dfd, testConfig, floodConfig):
    groups = dfd.groupby('observer_id')
    nodeList = sorted(dfd.observer_id.unique())
    numNodes = len(nodeList)
    numFloodsRxMatrix = np.empty( (numNodes, numNodes,) ) * np.nan
    hopDistanceMatrix = np.empty( (numNodes, numNodes,) ) * np.nan
    hopDistanceStdMatrix = np.empty( (numNodes, numNodes,) ) * np.nan

    # iterate over rounds
    for nodeOfRound in nodeList:
        txNode = nodeOfRound
        txNodeIdx = nodeList.index(txNode)
        # iterate over nodes
        for rxNode in nodeList:
            rxNodeIdx = nodeList.index(rxNode)
            rows = getRows(txNode, groups.get_group(rxNode))
            floodRxList = [elem for elem in rows if (elem['type']=='FloodDone' and elem['rx_cnt']>0 and elem['is_initiator']==0)]
            # fill matrix
            numFloodsRxMatrix[txNodeIdx][rxNodeIdx] = len(floodRxList)
            hopDistanceMatrix[txNodeIdx][rxNodeIdx] = np.mean([elem['rx_idx']+1 for elem in floodRxList]) if len(floodRxList) else np.nan
            hopDistanceStdMatrix[txNodeIdx][rxNodeIdx] = np.std([elem['rx_idx']+1 for elem in floodRxList]) if len(floodRxList) else np.nan

    return numFloodsRxMatrix, hopDistanceMatrix, hopDistanceStdMatrix


def extractFloodDelayedTx(dfd, testConfig, floodConfig):
    groups = dfd.groupby('observer_id')
    nodeList = sorted(dfd.observer_id.unique())
    numNodes = len(nodeList)
    numFloodsRxMatrix = np.empty( (numNodes, numNodes,) ) * np.nan
    hopDistanceMatrix = np.empty( (numNodes, numNodes,) ) * np.nan
    hopDistanceStdMatrix = np.empty( (numNodes, numNodes,) ) * np.nan

    # iterate over rounds
    for nodeOfRound in nodeList:
        delayedNode = nodeOfRound
        delayedNodeIdx = nodeList.index(delayedNode)
        # iterate over nodes
        for rxNode in nodeList:
            rxNodeIdx = nodeList.index(rxNode)
            rows = getRows(delayedNode, groups.get_group(rxNode))
            floodRxList = [elem for elem in rows if (elem['type']=='FloodDone' and elem['rx_cnt']>0 and elem['is_initiator']==0)]
            # fill matrix
            # hop distance = rx_idx + 1
            numFloodsRxMatrix[delayedNodeIdx][rxNodeIdx] = len(floodRxList)
            hopDistanceMatrix[delayedNodeIdx][rxNodeIdx] = np.mean([elem['rx_idx']+1 for elem in floodRxList]) if len(floodRxList) else np.nan
            hopDistanceStdMatrix[delayedNodeIdx][rxNodeIdx] = np.std([elem['rx_idx']+1 for elem in floodRxList]) if len(floodRxList) else np.nan

    return numFloodsRxMatrix, hopDistanceMatrix, hopDistanceStdMatrix


def saveP2pMatricesToHtml(extractionDict):
    nodeList = extractionDict['nodeList']

    prrMatrixDf = pd.DataFrame(data=extractionDict['prrMatrix'], index=nodeList, columns=nodeList)
    crcErrorMatrixDf = pd.DataFrame(data=extractionDict['crcErrorMatrix'], index=nodeList, columns=nodeList)
    pathlossMatrixDf = pd.DataFrame(data=extractionDict['pathlossMatrix'], index=nodeList, columns=nodeList)

    # save colored tables to html
    pathloss_html = styleDf(
        pathlossMatrixDf,
        cmap='summer',
        format='{:.0f}',
        replaceNan=True,
        applymap=lambda x: 'background: white' if pd.isnull(x) else ''
    )

    prr_html = styleDf(
        prrMatrixDf,
        cmap='inferno',
        format='{:.1f}',
        replaceNan=False,
        applymap=None
    )

    crc_error_html = styleDf(
        crcErrorMatrixDf,
        cmap='YlGnBu',
        format='{:.1f}',
        replaceNan=False,
        applymap=None
    )

    configString = 'testConfig:<br />{}<br /><br />radioConfig:<br />{}'.format(extractionDict['testConfig'], extractionDict['radioConfig'])

    h = html()
    with h.add(head()):
        meta(charset='UTF-8')
        style(raw(htmlStyleBlock))
    with h.add(body()).add(div(id='content')):
        with table(cls="outer").add(tbody()):
            with tr(cls="outer"):
                th('Pathloss Matrix [dB]', cls='outer')
                th(cls="outer")
                th('PRR Matrix', cls='outer')
            with tr(cls='outer'):
                td(raw(pathloss_html), cls='outer')
                td(cls="outer")
                td(raw(prr_html), cls='outer')
            with tr(cls='outer'):
                td(raw('<br />'), cls='outer')
                td(cls="outer")
                td(raw(''), cls='outer')
            with tr(cls="outer"):
                th('CRC Error Matrix', cls='outer')
                th(cls="outer")
                th('Config', cls='outer')
            with tr(cls='outer'):
                td(raw(crc_error_html), cls='outer')
                td(cls="outer")
                td(raw(configString), cls='outer')

    htmlPath = os.path.join(outputDir, '{}.html'.format(testNo))
    os.makedirs(os.path.split(htmlPath)[0], exist_ok=True)
    with open(htmlPath,"w") as fp:
       fp.write(h.render())


def saveFloodNormalMatricesToHtml(extractionDict):
    nodeList = extractionDict['nodeList']
    initiator = extractionDict['floodConfig']['initiator']

    numFloodsRxMatrixDf = pd.DataFrame(data=extractionDict['numFloodsRxMatrix'], index=nodeList, columns=nodeList)
    hopDistanceMatrixDf = pd.DataFrame(data=extractionDict['hopDistanceMatrix'], index=nodeList, columns=nodeList)
    hopDistanceStdMatrixDf = pd.DataFrame(data=extractionDict['hopDistanceStdMatrix'], index=nodeList, columns=nodeList)
    configHtml = 'testConfig:<br />{}<br /><br />floodConfig:<br />{}'.format(extractionDict['testConfig'], extractionDict['floodConfig'])

    saveMatricesToHtml(
        matrixDfList=(
            hopDistanceMatrixDf,
            numFloodsRxMatrixDf,
            hopDistanceStdMatrixDf,
            configHtml,
        ),
        titles=(
            'Hop distance (Tx node -> Rx node)'.format(initiator),
            'Number of received floods (Tx node -> Rx node)',
            'Stddev of hop distance',
            'Config',
        ),
        cmaps=('inferno_r', 'YlGnBu', 'YlGnBu', None),
        formats=('{:.1f}', '{:.0f}', '{:.1f}', None),
        applymaps=[lambda x: 'background: white' if pd.isnull(x) else '']*3 + [None],
        outputDir=outputDir,
        filename='{}.html'.format(testNo)
    )


def saveFloodDelayedTxMatricesToHtml(extractionDict):
    nodeList = extractionDict['nodeList']
    initiator = extractionDict['floodConfig']['initiator']

    numFloodsRxMatrixDf = pd.DataFrame(data=extractionDict['numFloodsRxMatrix'], index=nodeList, columns=nodeList)
    hopDistanceMatrixDf = pd.DataFrame(data=extractionDict['hopDistanceMatrix'], index=nodeList, columns=nodeList)
    hopDistanceStdMatrixDf = pd.DataFrame(data=extractionDict['hopDistanceStdMatrix'], index=nodeList, columns=nodeList)
    hopDistanceDiffMatrixDf = hopDistanceMatrixDf.apply(
        func=lambda col: col - col[initiator],
        axis=0
    )
    configHtml = 'testConfig:<br />{}<br /><br />floodConfig:<br />{}'.format(extractionDict['testConfig'], extractionDict['floodConfig'])

    saveMatricesToHtml(
        matrixDfList=(
            hopDistanceMatrixDf,
            numFloodsRxMatrixDf,
            hopDistanceDiffMatrixDf,
            hopDistanceStdMatrixDf,
            configHtml,
            '',
        ),
        titles=(
            'Hop distance (delayed node -> Rx node, initiator={})'.format(initiator),
            'Number of received floods',
            'Hop distance diff (delayed node -> Rx node, initiator={})'.format(initiator),
            'Stddev of hop distance',
            'Config',
            '',
        ),
        cmaps=('inferno_r', 'YlGnBu', 'inferno_r', 'YlGnBu', None, None),
        formats=('{:.1f}', '{:.0f}', '{:.1f}', '{:.1f}', None, None),
        applymaps=[lambda x: 'background: white' if pd.isnull(x) else '']*4 + [None, None],
        outputDir=outputDir,
        filename='{}.html'.format(testNo)
    )


def saveMatricesToHtml(matrixDfList, filename, titles, cmaps, formats, applymaps=None, outputDir='data'):
    numMatrices = len(matrixDfList)
    assert numMatrices == len(titles)
    assert numMatrices == len(cmaps)
    assert numMatrices == len(titles)
    assert numMatrices == len(formats)
    assert len(titles) % 2 == 0
    if applymaps is None:
        applymaps = [lambda x: '']*numMatrices
    h = html()
    with h.add(head()):
        meta(charset='UTF-8')
        style(raw(htmlStyleBlock))
    with h.add(body()).add(div(id='content')):
        with table(cls="outer").add(tbody()):
            for i in range(int(numMatrices/2)):
                htmlTile = {}
                for idx in [2*i, 2*i+1]:
                    if isinstance(matrixDfList[idx], pd.DataFrame):
                        htmlTile[idx] = styleDf(
                            df=matrixDfList[idx],
                            cmap=cmaps[idx],
                            format=formats[idx],
                            applymap=applymaps[idx],
                        )
                    elif type(matrixDfList[idx]) == str:
                        htmlTile[idx] = matrixDfList[idx]
                    else:
                        raise Exception('Unknown data type \'{}\' for matrix tile with title \'{}\''.format(type(matrixDfList[idx])), titles[idx])
                with tr(cls="outer"):
                    th(titles[2*i], cls='outer')
                    th(cls="outer")
                    th(titles[2*i+1], cls='outer')
                with tr(cls='outer'):
                    td(raw(htmlTile[2*i]), cls='outer')
                    td(cls="outer")
                    td(raw(htmlTile[2*i+1]), cls='outer')

    htmlPath = os.path.join(outputDir, filename)
    os.makedirs(os.path.split(htmlPath)[0], exist_ok=True)
    with open(htmlPath,"w") as fp:
       fp.write(h.render())

################################################################################
# Main
################################################################################

if __name__ == "__main__":
    # check arguments
    if len(sys.argv) < 2:
        print("no test number specified!")
        sys.exit(1)
    testNoList = map(int, sys.argv[1:])
    testDir = os.getcwd()

    for testNo in testNoList:
        print('testNo: {}'.format(testNo))

        d = extractData(testNo, testDir)
        if 'radioConfig' in d:
            saveP2pMatricesToHtml(d)
        elif 'floodConfig' in d:
            if d['floodConfig']['delayTx'] == 0:
                saveFloodNormalMatricesToHtml(d)
            else:
                saveFloodDelayedTxMatricesToHtml(d)
