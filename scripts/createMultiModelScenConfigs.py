#!/usr/bin/env python
import sys
from os import listdir
from numpy import median
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv
import os


group_defs={
    'scen2':['densenet161', 'vgg16'],
    'scen1':['googlenet', 'bert', 'mnasnet1_0', 'mobilenet_v2'],
    'scen3':['resnet50', 'vgg16', 'mobilenet_v2'],
    'scen4':['ssd-mobilenetv1', 'mnasnet1_0', 'densenet161'],
    'scen5':['lenet1', 'ssd-mobilenetv1', 'vgg16', 'mnasnet1_0']
}


model_slo={
'lenet1': 5,
'lenet2': 5,
'lenet3': 5,
'lenet4': 5,
'lenet5': 5,
'lenet6': 5,
'googlenet': 66,
'resnet50':108,
'ssd-mobilenetv1':202,
'vgg16':142,
'mnasnet1_0':62,
'mobilenet_v2':64,
'densenet161':202,
'bert':22
}

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

# number of files per group
NFILES=300

def createFile(log_dir,group,models,base_rate,num_of_files):
    for i in range(1,num_of_files+1):
        filename=group+"-"+str(i)+"-config.csv"
        with open(log_dir+"/"+filename,"w") as fp:
            num_of_models=len(models)
            fp.write(str(num_of_models)+"\n")
            for model in models:
                fp.write(str(model_id[model])+","+str(base_rate *i)+","+str(model_slo[model])+",\n")

def createFileShort(log_dir,models,num_of_files):
    short_models=['googlenet', 'mnasnet', 'bert','lenet1']
    for i in range(1,num_of_files+1):
        filename="scen1-"+str(i)+"-config.csv"
        with open(log_dir+"/"+filename,"w") as fp:
            num_of_models=len(models)
            fp.write(str(num_of_models)+"\n")
            for model in models:
                if model in short_models:
                    fp.write(str(model_id[model])+","+str(50*2*i)+","+str(model_slo[model])+",\n")
                else:
                    fp.write(str(model_id[model])+","+str(50*i)+","+str(model_slo[model])+",\n")


def parse_args():
    parser = ArgumentParser(description=__doc__, formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('root_dir', help='directory which hold the configuration')
    args = parser.parse_args()
    return args

def main():
        args = parse_args()
        #path = os.path(args.root_dir);
        os.mkdir(args.root_dir)
        for item in group_defs:
            if item == "scen1":
                createFileShort(args.root_dir,group_defs[item],NFILES)
            else:
                createFile(args.root_dir,item,group_defs[item],10,NFILES)

if __name__ == '__main__':
    main()
