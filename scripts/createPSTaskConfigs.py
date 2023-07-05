import sys
from os import listdir
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv 
import os
import statistics
from operator import itemgetter
import json


#workloads=["traffic", "game", "short", "long", "group1", "group2", "group3"]

workloads=["traffic", "game", "scen1", "scen2", "scen3", "scen4", "scen5"]
SLO=[]
model_ids=[]

def parse_args():
    parser = ArgumentParser(description=__doc__,
            formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('output_dir',
            help='directory which will holds task configs ')
    args = parser.parse_args()
    return args

def setup(workload):
    global SLO
    global workloadl_ids
    if workload == "traffic":
        SLO=[(6,112),(8,90),(9,112)]
        model_ids=[6,8,9]
    elif workload == "game":
        SLO=[(0,108),(1,108),(2,108),(3,108),(4,108),(5,108),(7,108)]
        model_ids=[0,1,2,3,4,5,7]
    else:
        SLO=[(0,5),(6,66),(7,108),(8,202),(9, 142),(10,62),(11,64),(12,202),(13,22)]
        model_ids=[0,6,7,8,9,10,11,12,13]
       
def generateCombinations(workload):
    comb_list=[]
    rates=range(10,3000,10)
    if workload == "scen1":
        for rate in rates:
            item1=(0,rate*2)
            item2=(6,rate*2)
            item3=(7,rate*2)
            item4=(8,rate)
            item5=(9,rate)
            comb_list.append([item1,item2,item3,item4,item5])
    elif workload == "scen2":
        for rate in rates:
            item3=(7,rate)
            item4=(8,rate)
            item5=(9,rate)
            comb_list.append([item3,item4,item5])
    elif workload == "game":
        for rate in rates:
            item1=(0,rate)
            item2=(1,rate)
            item3=(2,rate)
            item4=(3,rate)
            item5=(4,rate)
            item6=(5,rate)
            item7=(7,rate)
            comb_list.append([item1,item2,item3,item4,item5,item6,item7])
    elif workload == "traffic":
        for rate in rates:
            item1=(6,rate)
            item2=(8,rate)
            item3=(9,rate)
            comb_list.append([item1,item2,item3])
    elif workload == "scen3":
        for rate in rates:
            item1=(7,rate)
            item2=(9,rate)
            item3=(11,rate)
            comb_list.append([item1,item2,item3])
    elif workload == "scen4":
        for rate in rates:
            item1=(8,rate)
            item2=(10,rate)
            item3=(12,rate)
            comb_list.append([item1,item2,item3])
    elif workload == "scen5":
        for rate in rates:
            item1=(0,rate)
            item2=(8,rate)
            item3=(9,rate)
            item4=(10,rate)
            comb_list.append([item1,item2,item3,item4])
    else:
        print("unrecognized workload: "+workload)
        exit(1)
    return comb_list
   

   
def getSLO(id):
    slo=0
    for item in SLO:
        if id in item:
            slo=item[1]
    return slo

def writeContent(comb, fp, idx):
    num_of_models =0
    for item in comb:
        if item[1] != 0:
            num_of_models = num_of_models +1
    if num_of_models == 0:
        return
    fp.write(str(num_of_models)+"\n")
    for item in comb:
        if(item[1] == 0): 
            continue
        slo=getSLO(item[0])
        fp.write(str(item[0])+","+str(item[1])+","+str(slo)+",")
        fp.write("\n")

def generateLists(list_dir, possible_combinations, workload):
    save_path=list_dir
    idx=0
    for idx in range(0,len(possible_combinations)):
        filename=workload+"-"+str(idx+1)+"-config.csv"
        complete_path=os.path.join(save_path,filename)
        fp=open(complete_path,"w")
        writeContent(possible_combinations[idx],fp, idx)
        fp.close()


def main():
    args=parse_args()
    for workload in workloads:
        setup(workload)
        combs=generateCombinations(workload)
        generateLists(args.output_dir, combs, workload)
    print("Finished writing all combinations")

if __name__=='__main__':
    main()
