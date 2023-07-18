from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv
import os
from operator import itemgetter
from basicAnalysis import med


metrics=('avg_latency', 'tail_latency', 'total_throughput', 'slo')

def parse_args():
    parser = ArgumentParser(description=__doc__,
            formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('log_root_dir',
            help='directory which holds log files')
    parser.add_argument('result_file',
            help='file which is going to hold analyzed results')
    parser.add_argument('iternum',
            help='number of iterations')


    args = parser.parse_args()
    return args


def readandfillTable(metric_file, benchwise_info, benchmarks, benchwise_client_info):
        with open (metric_file) as fp:
                lines = fp.readlines()
                numofbench=len(benchmarks)
                cnt=0
                foundSign=False
                foundTotalThroughput=False
                foundServerLatency=False
                foundClientLatency=False
                foundServerSLO=False
                for line in lines:
                        if not foundSign:
                                if "SERVER" in line and "Total" in line and "APP" in line:
                                        foundSign=True
                                        foundTotalThroughput=True
                                        cnt=0
                                elif "SERVER" in line and "Latency" in line and "APP" in line:
                                        foundSign=True
                                        foundServerLatency=True
                                        cnt=0
                                elif "SERVER" in line and "SLO" in line and "APP" in line:
                                        foundSign=True
                                        foundServerSLO=True
                                        cnt=0
                                continue

                        if foundTotalThroughput:
                                if cnt == 0:
                                        cnt = cnt + 1
                                        continue
                                key=line.split(",")[0]
                                value=float(line.split(",")[1])
                                benchwise_info[key]["total_throughput"].append(value)
                                cnt = cnt +1
                                if cnt == numofbench + 1:
                                        foundSign=False
                                        foundTotalThroughput=False
                                        cnt=0
                                continue
                        elif foundServerLatency:
                                if cnt == 0:
                                        cnt = cnt + 1
                                        continue
                                key=line.split(",")[0]
                                value1=float(line.split(",")[1])
                                value2=float(line.split(",")[2])
                                benchwise_info[key]["avg_latency"].append(value1)
                                benchwise_info[key]["tail_latency"].append(value2)
                                cnt = cnt +1
                                if cnt == numofbench + 1:
                                        foundSign=False
                                        foundServerLatency=False
                                        cnt=0
                                continue
                        elif foundServerSLO:
                                if cnt == 0:
                                        cnt = cnt + 1
                                        continue
                                key=line.split(",")[0]
                                value1=float(line.split(",")[1])
                                benchwise_info[key]["slo"].append(value1)
                                cnt = cnt +1
                                if cnt == numofbench + 1:
                                        foundSign=False
                                        foundServerSLO=False
                                        cnt=0
                                continue
                        elif foundClientLatency:
                                if cnt == 0:
                                        cnt = cnt + 1
                                        continue
                                key=line.split(",")[0]
                                value1=float(line.split(",")[1])
                                value2=float(line.split(",")[2])
                                benchwise_client_info[key]["avg_latency"].append(value1)
                                benchwise_client_info[key]["tail_latency"].append(value2)
                                cnt = cnt +1
                                if cnt == numofbench + 1:
                                        foundSign=False
                                        foundClientLatency=False
                                        cnt=0
                                continue

def writeRecords(result_file, benchwise_info, schedulers, benchmarks, benchwise_client_info):
        with open(result_file,"w") as fp:
            for metric in metrics:
                    for schedule in schedulers:
                        fp.write("[Server-"+metric+"-"+schedule+"],")
                        fp.write("max,average,min")
                        fp.write("\n")
                        for bench in benchmarks:
                            fp.write(bench+",")
                            key=schedule+"-"+bench
                            print(metric)
                            fp.write(str(max(benchwise_info[key][metric])) +",")
                            fp.write(str(med(benchwise_info[key][metric])) +",")
                            fp.write(str(min(benchwise_info[key][metric])) +",")
                            fp.write("\n")
def fillSchedulersandBenchmarks(metric_file, schedulers, benchmarks):
    with open (metric_file) as fp:
                lines = fp.readlines()
                found=False
                cnt=1
                for line in lines:
                    if "SERVER" in line and "APP" in line and "Latency" in line:
                        found=True
                        continue
                    if not found:
                        continue
                    if cnt != 0:
                        cnt = cnt -1
                        continue
                    words=line.split(",")
                    if len(words) == 1 or len(words) == 0:
                        found=False
                        cnt = cnt +1
                        continue
                    schedule=words[0].split("-")[0]
                    benchmark=words[0].split("-")[1]
                    if benchmark == 'ssd' or benchmark == 'mobilenetv1':
                        benchmark = 'ssd-mobilenetv1'
                    if schedule not in schedulers:
                        schedulers.append(schedule)
                    if benchmark not in benchmarks:
                        benchmarks.append(benchmark)

                                   
def main():
    args = parse_args()
    #setup schedule-benchmark wise data
    schedulers=[]
    benchmarks=[]
    fillSchedulersandBenchmarks(args.log_root_dir+"/"+"1-schd_metrics.csv",schedulers, benchmarks) 
    print (benchmarks)
    print(schedulers)
    benchwise_info={}
    benchwise_client_info={}
    for schedule in schedulers:
        for bench in benchmarks:
            key=schedule+"-"+bench
            benchwise_info[key]={}
            benchwise_client_info[key]={}
            for metric in metrics:
                benchwise_info[key][metric]=[]
                benchwise_client_info[key][metric]=[]
    for i in range(1,int(args.iternum)+1):
            metric_file_name=str(i)+"-schd_metrics.csv"
            readandfillTable(args.log_root_dir+"/"+metric_file_name, benchwise_info,benchmarks ,benchwise_client_info)
    writeRecords(args.result_file, benchwise_info,schedulers, benchmarks, benchwise_client_info)

if __name__ == '__main__':
    main()

