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

#model_ids=[0,6,7,8,9] ## model ID SBP will use

## #1
#rates=[0,100,200,400] ## rates SBP will use

## #2
#rates=[0,100,300,500] ## rates SBP will use

## #3




#SLO=[(0,3),(6,44),(7,95),(8,136),(9, 130)]

SLO=[]
model_ids=[]

def parse_args():
    parser = ArgumentParser(description=__doc__,
            formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('list_dir',
            help='directory which will holds task configs ')
    parser.add_argument("app",
            help='specifappp: traffic, game')
    args = parser.parse_args()
    return args

def setup(app):
    global SLO
    global model_ids

    if app == "traffic":
        SLO=[(6,112),(8,90),(9,112)]
        model_ids=[6,8,9]
    elif app == "game":
        SLO=[(0,108),(1,108),(2,108),(3,108),(4,108),(5,108),(7,108)]
        model_ids=[0,1,2,3,4,5,7]
    else:
        print("unrecognized app:"+ app)
        return
        
def generateCombinations(app):
    comb_list=[]
    #rates=range(10,3000,10)
    if app == "game":
        rates=range(100,2500,100)
        for rate in rates:
            item1=(0,rate)
            item2=(1,rate)
            item3=(2,rate)
            item4=(3,rate)
            item5=(4,rate)
            item6=(5,rate)
            item7=(7,rate)
            comb_list.append([item1,item2,item3,item4,item5,item6,item7])
    elif app == "traffic":
        rates=range(100,900,50)
        for rate in rates:
            item1=(6,rate)
            item2=(8,rate)
            item3=(9,rate)
            comb_list.append([item1,item2,item3])
    else:
        print("unrecognized app: "+app)
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
    #printIfFound(comb, idx)

    

def generateLists(list_dir, possible_combinations, mode):
    save_path=list_dir
    rates=[]
    if mode == "game":
        rates=range(100,2500,100)
    elif mode == "traffic":
        rates=range(100,900,50)
    idx=0
    for rate in rates:
        filename="tasks_"+mode+"_"+str(rate)+".csv"
        complete_path=os.path.join(save_path,filename)
        fp=open(complete_path,"w")
        writeContent(possible_combinations[idx],fp, idx)
        idx=idx+1
        fp.close()


def main():
    args=parse_args()
    setup(args.app)
    combs=generateCombinations(args.app)
    generateLists(args.list_dir, combs, args.app)
    print("Finished writing all combinations")

if __name__=='__main__':
    main()
