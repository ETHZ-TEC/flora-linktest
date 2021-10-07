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


@author: rtrueb
@brief: Creates and executes flocklab tests
"""

import re
import os
import datetime
import sys
import json
import git  # GitPython package
from collections import OrderedDict

from flocklab import Flocklab
from flocklab import *
fl = Flocklab()

# Requires sx1262 library (https://gitlab.ethz.ch/tec/research/dpp/software/communication_platforms/sx126x_lora/sx1262)
from sx1262.sx1262 import LoraConfig, FskConfig, getGloriaFloodDuration

###############################################################################
obsNormal = []   # will be read from config if empty
obsHg     = []   # (not used)

cwd = os.path.dirname(os.path.realpath(__file__))
xmlPath = os.path.join(cwd, 'flocklab_linktest.xml')
print(xmlPath)
imageNormalId = 'imageNormal'
imageHgId = 'imageHg'
imagePath = os.path.join(cwd, '../Debug/comboard_linktest.elf')
obsList = obsNormal + obsHg

###############################################################################
# CONFIGURATION (for calculation of linktest duration)

FREERTOS_STARTUP = 1.3
SYNC_DELAY       = 10.0
SLACK            = 10.0

################################################################################

def readConfig(symbol, configFile='../Inc/app_config.h', idx=0):
    with open(os.path.join(cwd, configFile), 'r') as f:
        text = f.read()

    # ret = re.search(r'#define\s+({})\s+(.+?)\s'.format(symbol), text)
    ret = re.findall(r'^\s*#define\s+({})\s+([^/\n\r]+)\s*'.format(symbol), text, re.MULTILINE)
    if len(ret) == 0:
        raise Exception('ERROR: readConfig: element "{}" not found'.format(symbol))
    if idx >= len(ret):
        raise Exception('ERROR: readConfig: idx ({}) out of range'.format(idx))
    if idx is None:
        if len(ret) >= 2:
            raise Exception('ERROR: symbol "{}" defined multiple times!'.format(symbol))
        else:
            idx = 0
    ret = ret[idx][1].strip()
    if len(ret) >= 2 and ret[0] == '"' and ret[-1] == '"':
        ret = ret[1:-1]
    if ret.lstrip('-').isdigit():
        ret = int(ret)
    return ret


def readAllConfig():
    config = OrderedDict()
    config['TESTCONFIG_P2P_MODE'] = readConfig('TESTCONFIG_P2P_MODE')
    config['TESTCONFIG_FLOOD_MODE'] = readConfig('TESTCONFIG_FLOOD_MODE')
    if not bool(config['TESTCONFIG_P2P_MODE']) != bool(config['TESTCONFIG_FLOOD_MODE']):
        raise Exception('Only one linktest mode can be selected at a time!')
    config['TESTCONFIG_KEY'] = readConfig('TESTCONFIG_KEY').replace('"', '').replace("'", "")
    config['TESTCONFIG_NUM_SLOTS'] = readConfig('TESTCONFIG_NUM_SLOTS')
    config['TESTCONFIG_NUM_NODES'] = readConfig('TESTCONFIG_NUM_NODES')
    config['TESTCONFIG_SETUP_TIME'] = readConfig('TESTCONFIG_SETUP_TIME')         # SetupTime [ms]
    config['TESTCONFIG_START_DELAY'] = readConfig('TESTCONFIG_START_DELAY')       # StartDelay [ms]
    config['TESTCONFIG_STOP_DELAY'] = readConfig('TESTCONFIG_STOP_DELAY')         # StopDelay [ms]
    config['TESTCONFIG_SLOT_GAP'] = readConfig('TESTCONFIG_SLOT_GAP')             # TxSlack [ms]

    if config['TESTCONFIG_P2P_MODE']:
        config['RADIOCONFIG_TX_POWER'] = readConfig('RADIOCONFIG_TX_POWER')
        config['RADIOCONFIG_FREQUENCY'] = readConfig('RADIOCONFIG_FREQUENCY')
        config['RADIOCONFIG_MODULATION'] = readConfig('RADIOCONFIG_MODULATION').lower()
        config['RADIOCONFIG_BANDWIDTH'] = readConfig('RADIOCONFIG_BANDWIDTH')
        if 'lora' in config['RADIOCONFIG_MODULATION']:
            if config['RADIOCONFIG_BANDWIDTH'] == 0:
                config['RADIOCONFIG_BANDWIDTH'] = 125000
            elif config['RADIOCONFIG_BANDWIDTH'] == 1:
                config['RADIOCONFIG_BANDWIDTH'] = 250000
            elif config['RADIOCONFIG_BANDWIDTH'] == 2:
                config['RADIOCONFIG_BANDWIDTH'] = 500000
            else:
                raise Exception('LoRa Bandwidth other than 125k, 250k, or 500k are not (yet) implemented!')
        config['RADIOCONFIG_DATARATE'] = readConfig('RADIOCONFIG_DATARATE')
        config['RADIOCONFIG_CODERATE'] = readConfig('RADIOCONFIG_CODERATE')
        config['RADIOCONFIG_IMPLICIT_HEADER'] = readConfig('RADIOCONFIG_IMPLICIT_HEADER')
        config['RADIOCONFIG_CRC_ON'] = readConfig('RADIOCONFIG_CRC_ON')
        config['RADIOCONFIG_PREAMBLE_LEN'] = readConfig('RADIOCONFIG_PREAMBLE_LEN')
    elif config['TESTCONFIG_FLOOD_MODE']:
        config['FLOODCONFIG_RF_BAND'] = readConfig('FLOODCONFIG_RF_BAND')
        config['FLOODCONFIG_TX_POWER'] = readConfig('FLOODCONFIG_TX_POWER')
        config['FLOODCONFIG_MODULATION'] = readConfig('FLOODCONFIG_MODULATION')
        config['FLOODCONFIG_N_TX'] = readConfig('FLOODCONFIG_N_TX')
        config['FLOODCONFIG_NUM_HOPS'] = readConfig('FLOODCONFIG_NUM_HOPS')
        config['FLOODCONFIG_FLOOD_GAP'] = readConfig('FLOODCONFIG_FLOOD_GAP')
        config['FLOODCONFIG_DELAY_TX'] = readConfig('FLOODCONFIG_DELAY_TX')
        config['FLOODCONFIG_INITIATOR'] = readConfig('FLOODCONFIG_INITIATOR')
    else:
        raise Exception('No valid linktest mode selected!')

    # DEBUG
    for k, v in config.items():
        print('{}={}'.format(k, repr(v)))

    return config


def calculateLinktestDuration(config):
    testDuration = None
    payloadLen = len(config['TESTCONFIG_KEY']) + 2   # +2 for uint16_t counter
    if config['TESTCONFIG_P2P_MODE']:
        if 'lora' in config['RADIOCONFIG_MODULATION']:
            loraconfig = LoraConfig()
            loraconfig.bw = config['RADIOCONFIG_BANDWIDTH']
            loraconfig.sf = config['RADIOCONFIG_DATARATE']
            loraconfig.phyPl = payloadLen
            loraconfig.cr = config['RADIOCONFIG_CODERATE']
            loraconfig.ih = config['RADIOCONFIG_IMPLICIT_HEADER']
            loraconfig.lowDataRate = True if (config['RADIOCONFIG_DATARATE'] in (11,12)) else False  # sx126x radio driver in flora code automatically enables low data rate optimization for SF11 and SF12 if bandwidth is 125kHz
            loraconfig.crc = config['RADIOCONFIG_CRC_ON']
            loraconfig.nPreambleSyms = config['RADIOCONFIG_PREAMBLE_LEN']
            timeOnAir = loraconfig.timeOnAir
        elif 'fsk' in config['RADIOCONFIG_MODULATION']:
            fskconfig = FskConfig()
            fskconfig.bitrate = config['RADIOCONFIG_DATARATE']
            fskconfig.nPreambleBits = config['RADIOCONFIG_PREAMBLE_LEN']*8 # fsk preamble config is in bytes, sx126x lib expects bits
            fskconfig.nSyncwordBytes = 2
            fskconfig.nLengthBytes = 1
            fskconfig.nAddressBytes = 1
            fskconfig.phyPl = payloadLen
            fskconfig.nCrcBytes = 1
            timeOnAir = fskconfig.timeOnAir
        else:
            raise Exception('Unknown modulation!')

        slotTime = timeOnAir
        print('Time-on-air single Tx: {:.6f} s'.format(timeOnAir))
        print('Tx time per node: {:.6f} s'.format(timeOnAir * config['TESTCONFIG_NUM_SLOTS']))
    elif config['TESTCONFIG_FLOOD_MODE']:
        floodTime = getGloriaFloodDuration(
            modIdx=config['FLOODCONFIG_MODULATION'],
            phyPlLen=payloadLen,
            nTx=config['FLOODCONFIG_N_TX'],
            numHops=config['FLOODCONFIG_NUM_HOPS'],
        )
        slotTime = 2*config['FLOODCONFIG_FLOOD_GAP']/1e3 + floodTime
        print('Time for a single flood: {:.6f} s'.format(floodTime))
        print('slotTime: {:.6f} s'.format(slotTime))
    else:
        raise Exception('No valid linktest mode selected!')

    slotPeriod = slotTime + config['TESTCONFIG_SLOT_GAP']/1e3
    roundPeriod = config['TESTCONFIG_SETUP_TIME']/1e3 + config['TESTCONFIG_START_DELAY']/1e3 + (config['TESTCONFIG_NUM_SLOTS']-1)*slotPeriod + slotTime + config['TESTCONFIG_STOP_DELAY']/1e3
    numRounds = config['TESTCONFIG_NUM_NODES']
    testDuration = FREERTOS_STARTUP + SYNC_DELAY + numRounds*roundPeriod + SLACK
    print('numRounds: {}'.format(numRounds))
    print('RoundPeriod: {:.6f} s'.format(roundPeriod))
    print('TestDuration: {:.6f} s'.format(testDuration))

    return testDuration


def getDescription(config):
    ret = ''
    if config['TESTCONFIG_P2P_MODE']:
        modulation = ""
        if "lora" in config['RADIOCONFIG_MODULATION']:
            modulation = "LoRa SF{}".format(config['RADIOCONFIG_DATARATE'])
        else:
            modulation = "FSK {:.0f}kbps".format(config['RADIOCONFIG_DATARATE'] / 1e3)
        ret = "P2P: {}  {:.3f}MHz  {}dBm".format(modulation, config['RADIOCONFIG_FREQUENCY'] / 1e6, config['RADIOCONFIG_TX_POWER'])
    elif config['TESTCONFIG_FLOOD_MODE']:
        ret = "FLOOD: modIdx={}, delayTx={}".format(config['FLOODCONFIG_MODULATION'], config['FLOODCONFIG_DELAY_TX'])
    else:
        raise Exception('No valid linktest mode selected!')

    return ret


def create_test():
    global obsNormal, obsList

    # read info from config/header files
    custom = dict()
    for var in ['FLOCKLAB', 'FLOCKLAB_SWD', 'SWO_ENABLE', 'LOG_USE_DMA', 'TESTCONFIG_P2P_MODE', 'TESTCONFIG_FLOOD_MODE']:
        custom[var] = readConfig(var)
    custom['HOST_ID'] = readConfig('HOST_ID')
    custom['git_hashes'] = {
        'comboard_linktest': git.Repo(os.path.join(cwd, '../.')).head.object.hexsha,
        'flora-lib': git.Repo(os.path.join(cwd, '../Lib')).head.object.hexsha,
        'dpp': git.Repo(os.path.join(cwd, '../Lib/dpp')).head.object.hexsha,
    }
    custom['image_last_modified'] = '{}LT'.format(datetime.datetime.fromtimestamp(os.path.getmtime(imagePath)))

    imageConfig = readAllConfig()
    custom['imageConfig'] = imageConfig

    if not obsList:
        # read observer IDs from config
        obsNormal = list(map(int, readConfig('TESTCONFIG_NODE_IDS').split(',')))
        obsList   = obsNormal

    # sanity checks
    if custom['FLOCKLAB'] != 1:
        raise Exception('FLOCKLAB not set to 1 in "app_config.h"!')
    if imageConfig['TESTCONFIG_NUM_NODES'] != len(obsList):
        raise Exception('TESTCONFIG_NUM_NODES != len(obsList); ({}!={})'.format(imageConfig['TESTCONFIG_NUM_NODES'], len(obsList)))

    duration = max(int(calculateLinktestDuration(imageConfig))+1, 40) # min FlockLab test duration is 40s

    fc = FlocklabXmlConfig()
    fc.generalConf.name = 'DPP2LoRa Linktest'
    fc.generalConf.description = getDescription(imageConfig)
    fc.generalConf.custom = json.dumps(custom)
    fc.generalConf.duration = duration

    if obsNormal:
        targetNormal = TargetConf()
        targetNormal.obsIds = obsNormal
        targetNormal.embeddedImageId = imageNormalId
        targetNormal.voltage = 3.3
        fc.configList.append(targetNormal)

    if obsHg:
        targetHg = TargetConf()
        targetHg.obsIds = obsHg
        targetHg.embeddedImageId = imageHgId
        targetHg.voltage = 3.3
        fc.configList.append(targetHg)

    serial = SerialConf()
    serial.obsIds = obsNormal + obsHg
    serial.baudrate = '115200'
    # serial.remoteIp = '0.0.0.0' # activates Python serial logger (instead of C serial logger) if uncommented
    fc.configList.append(serial)

    gpioTracingConf = GpioTracingConf()
    gpioTracingConf.obsIds = obsList
    pinList = set(['INT1', 'INT2', 'LED1', 'LED2', 'LED3'])
    if custom['FLOCKLAB_SWD']:
        pinList = pinList.difference(set(['INT2', 'LED3']))
    if custom['SWO_ENABLE']:
        pinList = pinList.difference(set(['LED2']))
    gpioTracingConf.pinList = ['INT1', 'INT2', 'LED1', 'LED2', 'LED3']
    fc.configList.append(gpioTracingConf)

    gpioActuation = GpioActuationConf()
    gpioActuation.obsIds = obsNormal + obsHg
    pinConfList = []
    pinConfList += [{'pin': 'SIG1', 'level': 1, 'offset': 10.0}]
    pinConfList += [{'pin': 'SIG1', 'level': 0, 'offset': 11.0}]
    gpioActuation.pinConfList = pinConfList
    fc.configList.append(gpioActuation)

    # debugConf = DebugConf()
    # debugConf.obsIds = obsList
    # debugConf.cpuSpeed = 48000000
    # debugConf.dataTraceConfList = [
    #     ('IrqFired', 'W', 1),
    # ]
    # fc.configList.append(debugConf)

    # powerProfiling = PowerProfilingConf()
    # powerProfiling.obsIds = obsNormal + obsHg
    # powerProfiling.offset = 0 # seconds
    # powerProfiling.duration = duration
    # powerProfiling.samplingRate = 1e3
    # powerProfiling.fileFormat = 'rld'
    # fc.configList.append(powerProfiling)

    if obsNormal:
        imageNormal = EmbeddedImageConf()
        imageNormal.embeddedImageId = imageNormalId
        imageNormal.name = 'dummy_image_name'
        imageNormal.description = 'dummy image description'
        imageNormal.platform = 'dpp2lora'
        imageNormal.imagePath = imagePath
        fc.configList.append(imageNormal)

    if obsHg:
        imageHg = EmbeddedImageConf()
        imageHg.embeddedImageId = imageHgId
        imageHg.name = 'dummy_image_name'
        imageHg.description = 'dummy image description'
        imageHg.platform = 'dpp2lorahg'
        imageHg.imagePath = ''
        fc.configList.append(imageHg)

    fc.generalConf.custom = json.dumps(custom, separators=(',', ':'))

    fc.generateXml(xmlPath=xmlPath)


def run_test():
    # Prompt for scheduling the flocklab test
    inp = input("Submit FlockLab test to FlockLab Server? [y/N]: ")
    if inp == 'y':
        print(fl.createTestWithInfo(xmlPath))
    else:
        print('Test NOT submitted!')


if __name__ == "__main__":
    create_test()
    run_test()
