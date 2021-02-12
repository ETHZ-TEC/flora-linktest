#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Oct  6 23:07:26 2019

@author: romantrueb
"""

import pickle
import numpy as np

with open('linktest_data_75056.pkl', 'rb' ) as f:
    d = pickle.load(f)
    

nodeList = d['nodeList']
prrMatrix = d['prrMatrix']
testInfo = d['testInfo']

with open('connectivity_output.txt', 'w') as f:
    f.write('<?xml version="1.0" encoding="UTF-8" ?>\n<network platform="DPP2LoRa">\n')
    for txNode in nodeList:
        for rxNode in nodeList:
            rxIdx = nodeList.index(rxNode)
            txIdx = nodeList.index(txNode)
            if not np.isnan(prrMatrix[rxIdx][txIdx]):
                f.write('<link src="{txNode}" dest="{rxNode}" prr="{prr:.3f}" numpackets="{numPackets}" />\n'.format(
                        txNode=txNode, 
                        rxNode=rxNode, 
                        prr=prrMatrix[rxIdx][txIdx],
                        numPackets=testInfo['numTx'],
                ))
    f.write('</network>')