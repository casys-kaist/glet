from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv
import os
#from itertools import izip_longest
from operator import itemgetter
from basicAnalysis import average, tail, getThroughput_MakeSpan, diffTimestamp_s
from basicFileAnalysis import analyzeServerLatency,analyzeServerBreakdownAVG,analyzeServerBreakdownTAIL,analyzeSLO,analyzeThroughput2

def parse_args():
    parser = ArgumentParser(description=__doc__,
            formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('log_root_dir',
            help='directory which holds log files')
    parser.add_argument('result_file',
            help='file which is going to hold analyzed results')
    args = parser.parse_args()
    return args

def readBenchmarksandStages(server_log_file):
    benchmarks=[]
    stages=[]
    with open (server_log_file) as fp:
        lines = fp.readlines()
        cnt=1
        for line in lines:
            if cnt != 0:
                # read and store stages
                line=line[:-1]
                words = line.split(',')
                for x in range(2, len(words)):
                    if "ID" not in words[x]:
                        stages.append(words[x])
                cnt = cnt -1
                continue
            # store benchmarks
            bench = line.split(',')[1]
            if bench not in benchmarks:
                benchmarks.append(bench)
    return benchmarks,stages


def gatherClientLatency(client_file, client_benchwise_info):
        with open (client_file) as fp:
                print("processing " + client_file)
                dirnames = client_file.split("/")
                filename = dirnames[-1]
                benchmark = filename.split("-")[0]
                if benchmark == 'ssd': # this happens due to '-' seperated naming
                    benchmark='ssd-mobilenetv1'
                lines = fp.readlines()
                if len(lines) == 1: # something is wrong....
                        item=(0,0)
                        return item
                for line in lines:
                        words=line.split(' ')
#                       print line
                        if words[0] != "Respond":
                                continue
                        if float(words[2]) > 10000000 : # we have a serious problem of huge numbers, skip it if so
                                continue
                        #perf.append(float(words[2]))
                        client_benchwise_info[benchmark].append(float(words[2]))

def writeClientLatency(result_file, schedule, client_benchwise_info):
    with open(result_file,"a") as fp:
        fp.write("====CLIENT Latency====\n")
        fp.write("schedule-name,AVG latency, TAIL latency\n")
        for bench in client_benchwise_info:
            fp.write(schedule+"-"+bench+",")
            fp.write(str(average(client_benchwise_info[bench]))+",")
            fp.write(str(tail(client_benchwise_info[bench]))+"\n")
	
def parsetoFile( result_file, log_root_dir):
    for subdir in os.listdir(log_root_dir):
        client_benchwise_info={}
        model_benchmarks,model_stages=readBenchmarksandStages(log_root_dir+"/"+subdir+"/server-model.csv")
        app_benchmarks,app_stages=readBenchmarksandStages(log_root_dir+"/"+subdir+"/server-app.csv")
          
        for bench in app_benchmarks:
            client_benchwise_info[bench] = []
            client_data=[]
        print(model_stages)
        print(model_benchmarks)
        print(app_stages)
        print(app_benchmarks)
        schedule=subdir
        for f in os.listdir(log_root_dir+"/"+subdir):
            name=f.split(".")[0]
            bench=name.split("-")[0]
            metric=name.split("-")[-1]
            item=(schedule,bench)
            if metric == "app":
                analyzeServerLatency(log_root_dir+"/"+subdir+"/"+f, result_file,schedule, app_benchmarks, app_stages, metric)
                analyzeServerBreakdownAVG(log_root_dir+"/"+subdir+"/"+f, result_file,schedule, app_benchmarks, app_stages, metric)
                analyzeServerBreakdownTAIL(log_root_dir+"/"+subdir+"/"+f, result_file,schedule, app_benchmarks,app_stages, metric)
                analyzeThroughput2(log_root_dir+"/"+subdir+"/"+f, result_file, schedule, app_benchmarks, metric)
                analyzeSLO(log_root_dir+"/"+subdir+"/"+f, result_file, schedule, app_benchmarks,app_stages ,metric)
            elif metric == "model":
                analyzeServerLatency(log_root_dir+"/"+subdir+"/"+f, result_file,schedule, model_benchmarks, model_stages, metric)
                analyzeServerBreakdownAVG(log_root_dir+"/"+subdir+"/"+f, result_file,schedule, model_benchmarks, model_stages, metric)
                analyzeServerBreakdownTAIL(log_root_dir+"/"+subdir+"/"+f, result_file,schedule, model_benchmarks,model_stages, metric)
                analyzeThroughput2(log_root_dir+"/"+subdir+"/"+f, result_file, schedule, model_benchmarks, metric)
            elif metric == "client":
                gatherClientLatency(log_root_dir+"/"+subdir+"/"+f, client_benchwise_info)
        writeClientLatency(result_file,schedule,client_benchwise_info)
            
def main():
    args = parse_args()
    parsetoFile(args.result_file, args.log_root_dir)
        
if __name__ == '__main__':
    main()

