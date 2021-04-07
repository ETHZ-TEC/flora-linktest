#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on 2019-10-21

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
obsNormal = []   # will be read from config if empty  # [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32]
obsHg     = []   # [15, 17, 25, 30]

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
    if ret.isnumeric():
        ret = int(ret)
    return ret


def readAllConfig():
    ret = OrderedDict()
    ret['LINKTEST_P2P'] = readConfig('LINKTEST_P2P')
    ret['LINKTEST_FLOOD'] = readConfig('LINKTEST_FLOOD')

    ret['TESTCONFIG_KEY'] = readConfig('TESTCONFIG_KEY').replace('"', '').replace("'", "")
    ret['TESTCONFIG_NUM_SLOTS'] = readConfig('TESTCONFIG_NUM_SLOTS')
    ret['TESTCONFIG_NUM_NODES'] = readConfig('TESTCONFIG_NUM_NODES')
    ret['TESTCONFIG_SETUP_TIME'] = readConfig('TESTCONFIG_SETUP_TIME')         # SetupTime [ms]
    ret['TESTCONFIG_START_DELAY'] = readConfig('TESTCONFIG_START_DELAY')       # StartDelay [ms]
    ret['TESTCONFIG_STOP_DELAY'] = readConfig('TESTCONFIG_STOP_DELAY')         # StopDelay [ms]
    ret['TESTCONFIG_SLOT_GAP'] = readConfig('TESTCONFIG_SLOT_GAP')             # TxSlack [ms]

    ret['RADIOCONFIG_TX_POWER'] = readConfig('RADIOCONFIG_TX_POWER')
    ret['RADIOCONFIG_FREQUENCY'] = readConfig('RADIOCONFIG_FREQUENCY')
    ret['RADIOCONFIG_MODULATION'] = readConfig('RADIOCONFIG_MODULATION').lower()
    ret['RADIOCONFIG_PAYLOAD_LEN'] = len(ret['TESTCONFIG_KEY']) + 1 + 2   # +1 for string termination char, +2 for uint16_t counter
    ret['RADIOCONFIG_BANDWIDTH'] = readConfig('RADIOCONFIG_BANDWIDTH')
    if 'lora' in ret['RADIOCONFIG_MODULATION']:
        if ret['RADIOCONFIG_BANDWIDTH'] == 0:
            ret['RADIOCONFIG_BANDWIDTH'] = 125000
        elif ret['RADIOCONFIG_BANDWIDTH'] == 1:
            ret['RADIOCONFIG_BANDWIDTH'] = 250000
        elif ret['RADIOCONFIG_BANDWIDTH'] == 2:
            ret['RADIOCONFIG_BANDWIDTH'] = 500000
        else:
            raise Exception('LoRa Bandwidth other than 125k, 250k, or 500k are not (yet) implemented!')
    ret['RADIOCONFIG_DATARATE'] = readConfig('RADIOCONFIG_DATARATE')
    ret['RADIOCONFIG_CODERATE'] = readConfig('RADIOCONFIG_CODERATE')
    ret['RADIOCONFIG_IMPLICIT_HEADER'] = readConfig('RADIOCONFIG_IMPLICIT_HEADER')
    ret['RADIOCONFIG_CRC_ON'] = readConfig('RADIOCONFIG_CRC_ON')
    ret['RADIOCONFIG_PREAMBLE_LEN'] = readConfig('RADIOCONFIG_PREAMBLE_LEN')

    ret['FLOODCONFIG_RF_BAND'] = readConfig('FLOODCONFIG_RF_BAND')
    ret['FLOODCONFIG_TX_POWER'] = readConfig('FLOODCONFIG_TX_POWER')
    ret['FLOODCONFIG_MODULATION'] = readConfig('FLOODCONFIG_MODULATION')
    ret['FLOODCONFIG_N_TX'] = readConfig('FLOODCONFIG_N_TX')
    ret['FLOODCONFIG_NUM_HOPS'] = readConfig('FLOODCONFIG_NUM_HOPS')
    ret['FLOODCONFIG_FLOOD_GAP'] = readConfig('FLOODCONFIG_FLOOD_GAP')
    ret['FLOODCONFIG_DELAY_TX'] = readConfig('FLOODCONFIG_DELAY_TX')

    # DEBUG
    for k, v in ret.items():
        print('{}={}'.format(k, repr(v)))

    return ret


def calculateLinktestDuration(config):
    testDuration = None
    if config['LINKTEST_P2P']:
        if 'lora' in config['RADIOCONFIG_MODULATION']:
            loraconfig = LoraConfig()
            loraconfig.bw = config['RADIOCONFIG_BANDWIDTH']
            loraconfig.sf = config['RADIOCONFIG_DATARATE']
            loraconfig.phyPl = config['RADIOCONFIG_PAYLOAD_LEN']
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
            fskconfig.phyPl = config['RADIOCONFIG_PAYLOAD_LEN']
            fskconfig.nCrcBytes = 1
            timeOnAir = fskconfig.timeOnAir
        else:
            raise Exception('Unknown modulation!')

        slotTime = timeOnAir
        print('Time-on-air single Tx: {:.6f} s'.format(timeOnAir))
        print('Tx time per node: {:.6f} s'.format(timeOnAir * config['TESTCONFIG_NUM_SLOTS']))
    elif config['LINKTEST_FLOOD']:
        floodTime = getGloriaFloodDuration(
            modIdx=config['FLOODCONFIG_MODULATION'],
            phyPlLen=config['RADIOCONFIG_PAYLOAD_LEN'],
            nTx=config['FLOODCONFIG_N_TX'],
            numHops=config['FLOODCONFIG_NUM_HOPS'],
        )
        slotTime = 2*config['FLOODCONFIG_FLOOD_GAP']/1e3 + floodTime
        print('Time for a single flood: {:.6f} s'.format(floodTime))
        print('slotTime: {:.6f} s'.format(slotTime))
    else:
        raise Exception('No valid linktest type selected!')

    slotPeriod = slotTime + config['TESTCONFIG_SLOT_GAP']/1e3
    roundPeriod = config['TESTCONFIG_SETUP_TIME']/1e3 + config['TESTCONFIG_START_DELAY']/1e3 + (config['TESTCONFIG_NUM_SLOTS']-1)*slotPeriod + slotTime + config['TESTCONFIG_STOP_DELAY']/1e3
    testDuration = FREERTOS_STARTUP + SYNC_DELAY + config['TESTCONFIG_NUM_NODES'] * roundPeriod + SLACK
    print('RoundPeriod: {:.6f} s'.format(roundPeriod))
    print('TestDuration: {:.6f} s'.format(testDuration))

    return testDuration


def getDescription(config):
    modulation = ""
    if "lora" in config['RADIOCONFIG_MODULATION']:
        modulation = "LoRa SF{}".format(config['RADIOCONFIG_DATARATE'])
    else:
        modulation = "FSK {:.0f}kbps".format(config['RADIOCONFIG_DATARATE'] / 1e3)
    return "{}  {:.3f}MHz  {}dBm".format(modulation, config['RADIOCONFIG_FREQUENCY'] / 1e6, config['RADIOCONFIG_TX_POWER'])


def create_test():
    global obsNormal, obsList

    # read info from config/header files
    custom = dict()
    for var in ['FLOCKLAB', 'FLOCKLAB_SWD', 'SWO_ENABLE']:
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
