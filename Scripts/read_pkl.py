#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Oct  6 23:07:26 2019

@author: romantrueb
"""

import pickle
import numpy as np

with open('linktest_data_75108.pkl', 'rb' ) as f:
    d = pickle.load(f)
    
print(d)

nodeList = d['nodeList']
print('{:>6} {:>6}'.format('id', 'name'))
for id, name in enumerate(nodeList):
    print('{:6} {:6}'.format(id, name))