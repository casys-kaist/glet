#!/usr/bin/env python
import sys
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
from os import listdir
from operator import itemgetter


def average(numbers):
        total = sum(numbers)
        total = float(total)
        return total / len(numbers)

def nzaverage(numbers):
    cnt=0
    summation=0
    for num in numbers:
        if num > 0:
            summation = summation + num
            cnt =cnt +1
    if cnt == 0:
        return 0
    return summation / cnt

def waverage(numbers, weights): # get weighted average
    if len(numbers) != len(weights):
        print("length does not match")
        return 0
    ret=0
    for index in range(0,len(numbers)):
        ret = ret + weights[index] * numbers[index]
    return ret

def parseFile(input_file):
    with open(input_file) as fp:
        L2_vec=[]
        duration_vec=[]
        SM_vec=[]
        mem_vec=[]
        theo_oc=[]
        achieved_oc=[]
        lines = fp.readlines()
        active_cnt=0 # used for calculating active percentage
        total_cnt=0 # "
        startStats=False
        inStat=False
        for line in lines:
            if len(line)==0:
                continue
            words=line.split()
            if not words:
                continue
            if words[0] == "Memory" and words[1] == "[%]":
                mem_vec.append(float(words[-1]))
            elif words[0] == "L2" and words[1] == "Cache":
                L2_vec.append(float(words[-1]))
            elif words[0] == "Compute" and words[1] == "(SM)":
                SM_vec.append(float(words[-1]))
            elif words[0] == "Duration":
                if words[-2] == "usecond":
                    duration_vec.append(float(words[-1]))
                elif words[-2] == "nsecond":
                    duration_vec.append(float(words[-1])/1000)
                elif words[-2] == "msecond":
                    duration_vec.append(float(words[-1])*1000)
                else:
                    print ("undealt metric: "+words[-2])
        print(mem_vec)
        print(duration_vec)
        total_duration = sum(duration_vec)
        avg_duration = average(duration_vec)# change to ms
        # get weighted average of metric
        weight_vec=[]
        for dur in duration_vec:
            weight_vec.append(dur/total_duration)
        w_l2 = waverage(L2_vec, weight_vec)
        w_SM = waverage(SM_vec, weight_vec)
        w_mem = waverage(mem_vec, weight_vec)
        return(avg_duration,w_SM, w_l2, w_mem)



def parseLogs(log_dir,output_file):
        data = []
        for f in listdir(log_dir):
            print(f)
            filename=f.split(".")[0]
            tokens=filename.split("-")
            batch = int(tokens[0])
            benchmark= tokens[1]
            item = (benchmark, batch)
            ret_item = parseFile(log_dir+"/"+f)
            item = item + ret_item
            data.append(item)
        sorted_data = sorted(data,key=itemgetter(0))
        with open(output_file,"w") as fout:
            fout.write("bench,batch,avg_duration(ms),SM_util(%),l2_util(%),mem_util(%)\n")
            for item in sorted_data:
                for x in range(0,len(item)):
                    element = item[x]
                    fout.write(str(element)+",")
                fout.write("\n")                        
# leaving the argument function and main function for debugging
def parse_args():
        parser = ArgumentParser(description=__doc__,
                        formatter_class=ArgumentDefaultsHelpFormatter)
        parser.add_argument('log_dir',
                        help='dir which holds nsight log')
        parser.add_argument('output_file',
                        help='file to store output')
        args = parser.parse_args()
        return args

def main():
        args = parse_args()
        parseLogs(args.log_dir,args.output_file)
if __name__ == '__main__':
        main()


