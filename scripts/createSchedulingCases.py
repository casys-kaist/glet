#!/usr/bin/env python
import sys
from os import listdir
from numpy import median
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv
import os
import itertools


rates=[0,100,200]

model_id_slo={
0:5,
6:66,
7:108,
8:202,
9:142,
10:62,
11:64,
12:202,
13:22
}

# Just for reference
model_id={
'lenet1': 0,
'lenet2': 1,
'lenet3': 2,
'lenet4': 3,
'lenet5': 4,
'lenet6': 5,
'googlenet': 6,
'resnet50':7,
'ssd-mobilenetv1':8,
'vgg16':9,
'mnasnet1_0':10,
'mobilenet_v2':11,
'densenet161':12,
'bert':13
}

def createFile(log_dir,list_of_models,file_number):
    filename=str(file_number)+"-config.csv"
    with open(log_dir+"/"+filename,"w") as fp:
         num_of_models=len(list_of_models)
         fp.write(str(num_of_models)+"\n")
         for item in list_of_models:
             fp.write(str(item[0])+","+str(item[1])+","+str(model_id_slo[item[0]])+",\n")

def parse_args():
    parser = ArgumentParser(description=__doc__, formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('root_dir', help='directory which hold the configuration')
    args = parser.parse_args()
    return args

def createListfromcombo(combo):
    index_to_id={0:0,
            1:6,
            2:7,
            3:8,
            4:9,
            5:10,
            6:11,
            7:12,
            8:13 }
    list_of_items=[]
    for idx in range(0,len(combo)):
        if combo[idx]==0:
            continue
        new_item=(index_to_id[idx],combo[idx])
        list_of_items.append(new_item)
    return list_of_items

def createCombo(height, temp_list, ans_list):
    if height==0:
        ans_list.append(temp_list)
        return
    for rate in rates:
        temp_list.append(rate)
        createCombo2(height-1,temp_list,ans_list)
        temp_list.pop()

    
def main():
        args = parse_args()
        combos=[]
        temp=[]
        createCombo(9,temp,combos)
        num=0
        for combo in combos:
            list_of_models=createListfromcombo(combo)
            if len(list_of_models)==0:
                continue
            createFile(args.root_dir,list_of_models,num)
            num=num+1
        print("crated "+str(num)+" files")
if __name__ == '__main__':
    main()
