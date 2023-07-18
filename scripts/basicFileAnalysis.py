
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import numpy as np
import pandas as pd
import csv 
import os
from operator import itemgetter
from basicAnalysis import average, tail, getThroughput_MakeSpan, diffTimestamp_s 

SLO={}
skip_cnt=200

def gatherClientData(client_file, client_benchwise_info):
    with open (client_file) as fp:
        dirnames = client_file.split("/")
        filename = dirnames[-1]
        benchmark = filename.split("-")[0]
        lines = fp.readlines()
        if len(lines) == 1: # something is wrong....
            item=(0,0)
            return item
        for line in lines:
            words=line.split(' ')
            if words[0] != "Respond":
                continue
            if float(words[2]) > 100000000 : # we have a serious problem of huge numbers, skip it if so
                continue
            client_benchwise_info[benchmark].append(float(words[2]))


def analyzeThroughput2(serv_file, result_file, schedule_mode, benchmarks, metric):
    with open (serv_file) as fp:
        benchwise_info = {}
        for bench in benchmarks:
            benchwise_info[bench] = []       
        lines = fp.readlines()
        cnt = 1
        for line in lines:
            if cnt !=0 :
                cnt = cnt -1
                continue
            words  = line.split(',')
            task_id = int(words[2])
            if task_id == -1:
                continue
            benchname=words[1]
            timestamp=words[0]
            benchwise_info[benchname].append(timestamp)
        perf_data=[]
    for bench in benchmarks:
        item = (schedule_mode,bench)
        item = item + (getThroughput_MakeSpan(benchwise_info[bench]),diffTimestamp_s(benchwise_info[bench][0], benchwise_info[bench][-1]))
        perf_data.append(item)
    with open(result_file,"a") as fp:
        if metric == "model":
            fp.write("====SERVER MODEL Total Throughput(#tasks / makespan)====\n")
        elif metric == "app":
            fp.write("====SERVER APP Total Throughput(#tasks / makespan)====\n")  
        fp.write("scheduler-name,Throughput,Makespan(s)\n")
        sorted_data = sorted(perf_data,key=itemgetter(0,1))
        for item in sorted_data:
            item_name=str(item[0])+"-"+str(item[1])
            fp.write(item_name+",")
            for x in range(2,len(item)):
                element = item[x]
                fp.write(str(element)+",")
            fp.write("\n");

def analyze2PhaseThrougput(serv_file ,result_file, schedule_mode, benchmarks):
    with open (serv_file) as fp:
        print ("processing 2phase Througput" + serv_file)
        benchwise_info = {}
        for bench in benchmarks:
            benchwise_info[bench] = []       
        lines = fp.readlines()
        cnt = 1
        for line in lines:
            if cnt !=0 :
                cnt = cnt -1
                continue
            words  = line.split(',')
            benchname=words[1]
            timestamp=words[0]
            benchwise_info[benchname].append(timestamp)
        #chose the most earliest finishing benchmark and its timestamp
        earliest="23:59:59.999" # not earliest
        for bench in benchmarks:
            if isEarlierTimestamp(benchwise_info[bench][-1], earliest):
                earliest = benchwise_info[bench][-1]
        print ("earliest : " + earliest)
        perf_data=[]
        for bench in benchmarks:
            item = (schedule_mode,bench)
            print(bench)
            item = item + getThroughput_MakeSpan2Phase(benchwise_info[bench], earliest)
            perf_data.append(item)
        with open(result_file,"a") as fp:
            fp.write("====SERVER Conteding Throughput(#tasks / makespan)====\n")  
            fp.write("scheduler-name,Throughput Phase 1,makespan \n")
            sorted_data = sorted(perf_data,key=itemgetter(0,1))
            for item in sorted_data:
                item_name=str(item[0])+"-"+str(item[1])
                fp.write(item_name+",")
                for x in range(2,len(item)):
                    element = item[x]
                    fp.write(str(element)+",")
                fp.write("\n");


def analyzeQueue(serv_file, result_file, schedule_mode,benchmarks):
    with open (serv_file) as fp:
        benchwise_info = {}
        lines = fp.readlines()
        cnt = 1
        for line in lines:
            if cnt !=0 :
                cnt = cnt -1
                continue
            words  = line.split(',')
            benchname=words[0]
            if benchname not in benchwise_info:
                benchwise_info[benchname]=[]
            throughput=float(words[1])
            benchwise_info[benchname].append(throughput)
        perf_data=[]
    for bench in benchmarks:
        item = (schedule_mode,bench)
        item = item + (nonzeroAverage(benchwise_info[bench]),max(benchwise_info[bench]))
        perf_data.append(item)
    with open(result_file,"a") as fp:
        fp.write("====SERVER Queue====\n")
        fp.write("scheduler-name,AVG,MAX\n")
        sorted_data = sorted(perf_data,key=itemgetter(0,1))
        for item in sorted_data:
            item_name=str(item[0])+"-"+str(item[1])
            fp.write(item_name+",")
            for x in range(2,len(item)):
                element = item[x]
                fp.write(str(element)+",")
            fp.write("\n");

def analyzeServerBreakdownAVG(serv_breakdown_file, result_file, schedule_mode, benchmarks, stages, metric):
    server_data = []
    with open (serv_breakdown_file) as fp:
        benchwise_info = {}
        for bench in benchmarks:
            benchwise_info[bench] = {}
        for bench in benchmarks:
            for stage in stages:
                benchwise_info[bench][stage]=[]
        lines = fp.readlines()
        cnt=1 # skip the first
        stage_len = len(stages)
        start_idx = 0
        for line in lines:
            if cnt != 0:
                words=line.split(',')
                i=0
                for word in words:
                    if(word == stages[0]):
                        start_idx=i
                        break
                    else:
                        i=i+1
                cnt = cnt - 1
                continue
            words=line.split(',')
            bench = words[1]
            task_id = int(words[2])
            if task_id == -1:
                continue
            i=0
            total=0
            for x in range(start_idx,start_idx+stage_len):
                benchwise_info[bench][stages[i]].append(float(words[x]))
                total = total + float(words[x])
                i = i +1
        for bench in benchmarks:
            item = (schedule_mode,bench)
            for stage in stages:
                item = item + (average(benchwise_info[bench][stage]),)
            server_data.append(item)
    with open(result_file,"a") as fp:
        if metric == "model":
            fp.write("====SERVER MODEL Breakdown(AVG)====\n")
        elif metric == "app":
            fp.write("====SERVER APP Breakdown(AVG)====\n")
        sorted_data = sorted(server_data,key=itemgetter(0,1))
        fp.write("desc")                                                                            
        for stage in stages:
            fp.write(","+stage)
        fp.write("\n")
        for item in sorted_data:
            item_name=str(item[0])+"-"+str(item[1])
            fp.write(item_name+",")
            for x in range(2,len(item)):
                element = item[x]
                fp.write(str(element)+",")
            fp.write("\n");

def analyzeServerBreakdownTAIL(serv_breakdown_file, result_file, schedule_mode, benchmarks, stages, metric):
    server_data = []
    with open (serv_breakdown_file) as fp:
        benchwise_info = {}
        for bench in benchmarks:
            benchwise_info[bench] = {}
        for bench in benchmarks:
            for stage in stages:
                benchwise_info[bench][stage]=[]
                benchwise_info[bench]['latency']=[]
        lines = fp.readlines()
        cnt=1 # skip the first
        stage_len = len(stages)
        start_idx = 0
        for line in lines:
            if cnt != 0:
                words=line.split(',')
                i=0
                for word in words:
                    if(word == stages[0]):
                        start_idx=i
                        break
                    else:
                        i=i+1
                cnt = cnt - 1
                continue
            words=line.split(',')
            bench = words[1]
            task_id = int(words[2])
            if task_id == -1:
                continue
            i=0
            total=0
            for x in range(start_idx,start_idx+stage_len):
                benchwise_info[bench][stages[i]].append(float(words[x]))
                total = total + float(words[x])
                i = i +1
            benchwise_info[bench]['latency'].append(total)
        for bench in benchmarks:
            benchwise_info[bench]['latency'] = benchwise_info[bench]['latency'][skip_cnt:]
            item = (schedule_mode,bench)
            # get the index of 99%
            tail_latency = tail(benchwise_info[bench]['latency'])
            tail_index=benchwise_info[bench]['latency'].index(tail_latency)
            for stage in stages:
                item = item + (benchwise_info[bench][stage][tail_index],)
            server_data.append(item)
    with open(result_file,"a") as fp:
        if metric == "model":
            fp.write("====SERVER MODEL Breakdown(TAIL)====\n")
        elif metric == "app":
            fp.write("====SERVER APP Breakdown(TAIL)====\n")
        sorted_data = sorted(server_data,key=itemgetter(0,1))
        fp.write("desc")
        for stage in stages:
            fp.write(","+stage)
        fp.write("\n")
        for item in sorted_data:
            item_name=str(item[0])+"-"+str(item[1])
            fp.write(item_name+",")
            for x in range(2,len(item)):
                element = item[x]
                fp.write(str(element)+",")
            fp.write("\n");
def analyzeServerLatency(serv_breakdown_file, result_file, schedule_mode, benchmarks,stages, metric):
    server_data = []
    with open (serv_breakdown_file) as fp:
        benchwise_info = {}
        for bench in benchmarks:
            benchwise_info[bench] = []
        lines = fp.readlines()
        cnt=1 # skip the first
        stage_len = len(stages)
        if metric=="model":
            start_idx=5
            end_idx=start_idx+stage_len
        elif metric == "app":
            start_idx=4
            end_idx=5
        for line in lines:
            if cnt != 0:
                cnt = cnt - 1
                continue
            words=line.split(',')
            bench = words[1]
            task_id = int(words[2])
            if task_id == -1: # skip dropped tasks
                continue
            i=0
            total=0
            for x in range(start_idx,end_idx):
                total = total + float(words[x])
                i = i +1
            benchwise_info[bench].append(total)
        for bench in benchmarks:
            benchwise_info[bench] = benchwise_info[bench][skip_cnt:]
            item = (schedule_mode,bench,average(benchwise_info[bench]),tail(benchwise_info[bench]))
            server_data.append(item)
    with open(result_file,"a") as fp:
        if metric == "model":
            fp.write("====SERVER MODEL Latency====\n")
        elif metric == "app":
            fp.write("====SERVER APP Latency====\n")
        sorted_data = sorted(server_data,key=itemgetter(0,1))
        fp.write("schedule-name,AVG latency,Tail latency(99%)\n");
        for item in sorted_data:
            item_name=str(item[0])+"-"+str(item[1])
            fp.write(item_name+",")
            for x in range(2,len(item)):
                element = item[x]
                fp.write(str(element)+",")
            fp.write("\n");

def analyzeSLO(serv_breakdown_file, result_file, schedule_mode, benchmarks,stages, metric):
    server_data = []
    SLO['ssd-mobilenetv1']=202
    SLO['lenet1']=5
    SLO['resnet50']=108
    SLO['googlenet']=66
    SLO['vgg16']=142
    SLO['traffic']=202
    SLO['game']=108
    SLO['mnasnet1_0']=62
    SLO['mobilenet_v2']=64
    SLO['densenet161']=202
    SLO['bert']=22

    first_timestamp=0
    with open (serv_breakdown_file) as fp:
        benchwise_info_SLO_violate = {}
        benchwise_info_total={}
        benchwise_info_timestamp={}
        benchwise_info_latency={}
        benchwise_info_task_id={}
        for bench in benchmarks:
            benchwise_info_SLO_violate[bench] = 0
            benchwise_info_total[bench]=0
            benchwise_info_timestamp[bench]=[]
            benchwise_info_latency[bench]=[]
            benchwise_info_task_id[bench]=[]

        lines = fp.readlines()
        cnt=1 # skip the first
        stage_len = len(stages)
        if metric == "model":
            start_idx=5
            end_idx=start_idx+stage_len
        elif metric == "app":
            start_idx=4
            end_idx=5
        for line in lines:
            if cnt != 0:
               cnt = cnt - 1
               continue
            words=line.split(',')
            if first_timestamp == 0:
                first_timestamp=words[0]
            if diffTimestamp_s(first_timestamp, words[0]) <1:
                continue
            bench = words[1]
            task_id = int(words[2])
            i=0
            total=0
            for x in range(start_idx,end_idx):
                total = total + float(words[x])
            benchwise_info_latency[bench].append(total)
            benchwise_info_task_id[bench].append(task_id)

        for bench in benchmarks:
            local_skip_cnt=skip_cnt
            if len(benchwise_info_latency[bench]) <= skip_cnt:
                local_skip_cnt = 100
            for i in range(len(benchwise_info_latency[bench])):
                if local_skip_cnt != 0:
                    local_skip_cnt = local_skip_cnt -1
                    continue
                if benchwise_info_task_id[bench][i] == -1:
                    benchwise_info_SLO_violate[bench] = benchwise_info_SLO_violate[bench] + 1

                if benchwise_info_latency[bench][i] > SLO[bench] and benchwise_info_task_id[bench][i] != -1:
                    benchwise_info_SLO_violate[bench] = benchwise_info_SLO_violate[bench] + 1
                benchwise_info_total[bench] = benchwise_info_total[bench]+1
            print("processed: "+serv_breakdown_file)
            print(bench+",total: "+str(benchwise_info_total[bench])+",violated: "+str(benchwise_info_SLO_violate[bench]))
            item = (schedule_mode, bench, 100*float(benchwise_info_SLO_violate[bench])/benchwise_info_total[bench],SLO[bench])
            server_data.append(item)
    with open(result_file,"a") as fp:
        if metric == "model":
            fp.write("====SERVER MODEL SLO====\n")
        elif metric == "app":
            fp.write("====SERVER APP SLO====\n")
        sorted_data = sorted(server_data,key=itemgetter(0,1))
        fp.write("schedule-name,SLO Violation(%),Tail SLO(ms)\n");
        for item in sorted_data:
            item_name=str(item[0])+"-"+str(item[1])
            fp.write(item_name+",")
            for x in range(2,len(item)):
                element = item[x]
                fp.write(str(element)+",")
            fp.write("\n");