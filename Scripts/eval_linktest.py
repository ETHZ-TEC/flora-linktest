#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sat Oct  5 13:10:39 2019

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

from flocklab import Flocklab
from flocklab import *

fl = Flocklab()

assertionOverride = False


################################################################################

# check arguments
if len(sys.argv) < 2:
    print("no test number specified!")
    sys.exit(1)
testNoList = map(int, sys.argv[1:])


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

def getRows(roundNo, gDf):
    '''Extract rows for requested round from gDf
    '''
    inRange = False
    ret = []
    for d in gDf.data.to_list():
        if d['type'] == 'StartOfRound':
            if d['node'] == roundNo:
                inRange = True
        elif d['type'] == 'EndOfRound':
            if d['node'] == roundNo:
                break
        elif inRange:
            ret.append(d)
    return ret

################################################################################

for testNo in testNoList:
    print('testNo: {}'.format(testNo))
    testdir = os.getcwd() + "/{}/serial.csv".format(testNo)

    # download test results if directory does not exist
    if not os.path.isfile(testdir):
        fl.getResults(testNo)

    df = fl.serial2Df(testdir, error='ignore')
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

    # figure our list of nodes available in the serial trace
    nodeList = list(set(dfd.observer_id))
    numNodes = len(nodeList)

    # prepare
    groups = dfd.groupby('observer_id')
    prrMatrix = np.empty( (numNodes, numNodes,) ) * np.nan       # packet reception ratio (PRR)
    crcErrorMatrix = np.empty( (numNodes, numNodes,) ) * np.nan  # ratio of packets with CRC error
    pathlossMatrix = np.empty( (numNodes, numNodes,) ) * np.nan  # path loss

    # Get TestConfig and RadioConfig & check for consistency
    testConfigDict = OrderedDict()
    radioConfigDict = OrderedDict()
    for node in nodeList:
        testConfigFound = False
        radioConfigFound = False
        testConfigDict[node] = None
        radioConfigDict[node] = None
        gDf = groups.get_group(node)
        for d in gDf.data.to_list():
            if d['type'] == 'TestConfig':
                testConfigDict[node] = d
                testConfigFound = True
            if d['type'] == 'RadioConfig':
                radioConfigDict[node] = d
                radioConfigFound = True
            if testConfigFound and radioConfigFound:
                break

    for node in nodeList:
        assert testConfigDict[nodeList[0]] == testConfigDict[node]
        assert radioConfigDict[nodeList[0]] == radioConfigDict[node]

    testConfig = testConfigDict[nodeList[0]]
    radioConfig = radioConfigDict[nodeList[0]]

    # Make sure that round boundaries do not overlap
    if not assertionOverride:
        currentSlot = -1
        for d in dfd.data.to_list():
            if d['type'] == 'StartOfRound':
                node = d['node']
                # print('Start: {}'.format(node))
                assert node >= currentSlot
                if node > currentSlot:
                    currentSlot = node
            elif d['type'] == 'EndOfRound':
                node = d['node']
                # print('End: {}'.format(node))
                assert node >= currentSlot

    # extract statistics (PRR, path loss, ...)
    # iterate over rounds
    for roundIdx, roundNo in enumerate(nodeList):
    # for roundNo in [nodeList[0]]:
        # print('Round: {}'.format(roundNo))
        txNode = roundNo
        txNodeIdx = roundIdx
        numTx = 0
        numRxDict = OrderedDict()
        numCrcErrorDict = OrderedDict()
        rssiAvgDict = OrderedDict()
        # iterate over nodes
        for nodeIdx, node in enumerate(nodeList):
            rows = getRows(roundNo, groups.get_group(node))
            if node == txNode:
    #            print(node)
                txDoneList = [elem for elem in rows if (elem['type']=='TxDone')]
                numTx = len(txDoneList)
    #            print(numTx, testConfig['numTx'])
                assert numTx == testConfig['numTx']
            else:
                rxDoneList = [elem for elem in rows if (elem['type']=='RxDone' and elem['key']==testConfig['key'] and elem['crc_error']==0)]
                crcErrorList = [elem for elem in rows if (elem['type']=='RxDone' and elem['crc_error']==1)]
                numRxDict[node] = len(rxDoneList)
                numCrcErrorDict[node] = len(crcErrorList)
                rssiAvgDict[node] = np.mean([elem['rssi'] for elem in rxDoneList]) if len(rxDoneList) else np.nan

        # fill PRR matrix
        for rxNode, numRx in numRxDict.items():
            rxNodeIdx = nodeList.index(rxNode)
            prrMatrix[txNodeIdx][rxNodeIdx] = numRx/numTx

        # fill CRC error matrix
        for rxNode, numCrcError in numCrcErrorDict.items():
            rxNodeIdx = nodeList.index(rxNode)
            crcErrorMatrix[txNodeIdx][rxNodeIdx] = numCrcError/numTx
        # NOTE: some CRC error cases are ignored while getting the rows (getRows()) because the json parser cannot parse the RxDone output

        # fill path loss matrix
        for rxNode, rssi in rssiAvgDict.items():
            rxNodeIdx = nodeList.index(rxNode)
            pathlossMatrix[txNodeIdx][rxNodeIdx] = -(rssi - radioConfig['txPower'])

    prrMatrixDf = pd.DataFrame(data=prrMatrix, index=nodeList, columns=nodeList)
    crcErrorMatrixDf = pd.DataFrame(data=crcErrorMatrix, index=nodeList, columns=nodeList)
    pathlossMatrixDf = pd.DataFrame(data=pathlossMatrix, index=nodeList, columns=nodeList)



    # save obtained data to file (including nodeList to resolve idx <-> node ID relations)
    pklPath = './data/linktest_data_{}.pkl'.format(testNo)
    os.makedirs(os.path.split(pklPath)[0], exist_ok=True)
    with open(pklPath, 'wb' ) as f:
        d = {
            'testConfig': testConfig,
            'radioConfig': radioConfig,
            'nodeList': nodeList,
            'prrMatrix': prrMatrix,
            'crcErrorMatrix': crcErrorMatrix,
            'pathlossMatrix': pathlossMatrix,
        }
        pickle.dump(d, f)


#    # print table for resolving nodeIdx to flocklab node ID
#    print('{:>8} {:>5}'.format('nodeIdx', 'node'))
#    for nodeIdx, node in enumerate(nodeList):
#        print('{:>8} {:>5}'.format(nodeIdx, node))


    # save colored tables to html
    html_template = '''
    <html>
    <head>
    <style>
      table, th, td {{font-size:10pt; border:1px solid lightgrey; border-collapse:collapse; text-align:left; font-family:arial;}}
      th, td {{padding: 5px; text-align:center; width:22px;}}
      table.outer, th.outer, td.outer {{font-size:10pt; border:0px solid lightgrey; border-collapse:collapse; text-align:left; font-family:arial;}}
      th.outer, td.outer {{padding: 5px; text-align:center;}}
    </style>
    </head>
    <body>
        <table class="outer">
        <thead>
          <tr class="outer">
            <th class="outer">Pathloss Matrix [dB]</th>
            <th class="outer"></th>
            <th class="outer">PRR Matrix</th>
          </tr>
        </thead>
        <tbody>
          <tr class="outer">
            <td class="outer">{pathloss_html}</td>
            <td class="outer"></td>
            <td class="outer">{prr_html}</td>
          </tr>
          <tr>
            <td class="outer"><br /></td>
            <td class="outer"></td>
            <td class="outer"></td>
          </tr>
        </tbody>
        <thead>
          <tr class="outer">
            <th class="outer">CRC Error Matrix</th>
            <th class="outer"></th>
            <th class="outer"></th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td class="outer">{crc_error_html}</td>
            <td class="outer"></td>
            <td class="outer">{config}</td>
          </tr>
        </tbody>
        </table>
    </body>
    </html>
    '''

    pathlossMatrixDf_styled = (pathlossMatrixDf.style
      .background_gradient(cmap='summer', axis=None)
      .applymap(lambda x: 'background: white' if pd.isnull(x) else '')
      .format("{:.0f}")
    )
    pathloss_html = pathlossMatrixDf_styled.render().replace('nan','')

    prrMatrixDf_styled = (prrMatrixDf.style
      .background_gradient(cmap='inferno', axis=None)
      .format("{:.1f}")
    )
    prr_html = prrMatrixDf_styled.render()

    crcErrorMatrixDf_styled = (crcErrorMatrixDf.style
      .background_gradient(cmap='YlGnBu', axis=None)
      .format("{:.1f}")
    )
    crc_error_html = crcErrorMatrixDf_styled.render()

    htmlPath = './data/{}.html'.format(testNo)
    os.makedirs(os.path.split(htmlPath)[0], exist_ok=True)
    with open(htmlPath,"w") as fp:
       fp.write(html_template.format(
               pathloss_html=pathloss_html,
               prr_html=prr_html,
               crc_error_html=crc_error_html,
               config='testConfig:<br />{}<br /><br />radioConfig:<br />{}'.format(testConfig, radioConfig)
           )
       )
