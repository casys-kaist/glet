from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv
import os
#from itertools import izip_longest
from operator import itemgetter

WORKLOADS=['game', 'traffic', 'scen1', 'scen2', 'scen3', 'scen4', 'scen5']

NUM_OF_FILES_PER_WORKLOAD=300
NUM_OF_FILES_TO_PRINT=3

def parse_args():
    parser = ArgumentParser(description=__doc__,
            formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('log_root_dir',
            help='directory which holds log files')
    args = parser.parse_args()
    return args

def getSetup(filename):
    tokens=filename.split("-")
    setup_str = "\'"+str(tokens[0])+"-"+str(tokens[1])+"\'"
    return setup_str

def printFiles(vec_files,isFirst):
    if len(vec_files) < NUM_OF_FILES_TO_PRINT:
        print("expected more files!! there are only "+str(len(vec_files))+" file(s)")
        return 
    str_to_print=''
    if isFirst is True:
        str_to_print="CASE=("
    else:
        str_to_print="CASE+=("
    cnt=0
    for filename in vec_files:
        setup_name = getSetup(filename)
        str_to_print = str_to_print + setup_name
        cnt=cnt+1
        if cnt < NUM_OF_FILES_TO_PRINT:
            str_to_print = str_to_print + " "
        else:
            str_to_print = str_to_print +  ")"
    print(str_to_print)

def checkEmpty(filename):
    empty=False
    with open(filename,"r") as fp:
        lines = fp.readlines()
        for line in lines:
            if "EMPTY" in line:
                empty=True
                break
    return empty
# 
def readnPrintSchedFiles(result_dir):
    # will print top 3 rates per workload
    first=True
    for work in WORKLOADS:
        success_files=[]
        reversed_range=reversed(range(1,NUM_OF_FILES_PER_WORKLOAD))
        for i in reversed_range:
            if len(success_files) >= NUM_OF_FILES_TO_PRINT:
                break;
            filename=str(work)+"-"+str(i)+"-result.txt"
            if checkEmpty(result_dir+"/"+filename):
                continue
            else:
                success_files.append(filename)
        printFiles(success_files,first)
        if first:
            first=False


def ReadnPrint(log_root_dir):
    print("reading following sub dirs under: " + log_root_dir)
    scens=os.listdir(log_root_dir)
    print(scens)
    for scen_dir in scens:
        print("----"+str(scen_dir)+"----")
        readnPrintSchedFiles(log_root_dir+"/"+scen_dir)
        print("------------")


def main():
    args = parse_args()
    ReadnPrint(args.log_root_dir)

if __name__ == '__main__':
    main()


