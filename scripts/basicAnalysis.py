import numpy as np
import pandas as pd
import csv 
import os
from operator import itemgetter

def isEarlierTimestamp(org, dest):
    org_seconds=float(org.split(':')[-1]) / 1000 + float(org.split(':')[2]) + float(org.split(':')[1]) *60 + float(org.split(':')[0])*3600
    dest_seconds=float(dest.split(':')[-1]) / 1000 + float(dest.split(':')[2]) + float(dest.split(':')[1]) *60 + float(dest.split(':')[0])*3600
    if (org_seconds<=dest_seconds ):
        return True
    else: 
        return False

def diffTimestamp_s(start, end): # returns how much time has elapsed between two timestamp in ms
        start_seconds=float(start.split(':')[-1]) / 1000 + float(start.split(':')[2]) + float(start.split(':')[1]) *60 
        end_seconds=float(end.split(':')[-1]) / 1000 + float(end.split(':')[2]) + float(end.split(':')[1]) *60 
        if start_seconds > end_seconds:
                end_seconds = 3600 + end_seconds
        return (end_seconds - start_seconds)

def getThroughput_MakeSpan(vec): # accepts vector of timestamps
        if len(vec) == 0:
            return 0
        elif len(vec) == 1:
            return 0
        makespan = diffTimestamp_s(vec[0], vec[-1])
        if makespan==0:
            return 0
        return (len(vec)-1) / diffTimestamp_s(vec[0], vec[-1])

def getThroughput_MakeSpan2Phase(vec,timestamp): # accepts vector of timestamps & the timestamp to split the vector
    firstVec=[]
    secondVec=[]
    for i in range(len(vec)):
        if isEarlierTimestamp(vec[i], timestamp):
            firstVec.append(vec[i])
        else:
            secondVec.append(vec[i])
    return getThroughput_MakeSpan(firstVec), diffTimestamp_s(firstVec[0],timestamp)

def nonzeroAverage(vec):
    pvec = np.array([num for num in vec if num > 0]) 
    nvec=sorted(pvec)
    if len(nvec) ==0:
        return 0
    if len(nvec) ==1:
        return vec[0]
    return sum(nvec) / len(nvec)

def tail(vec):
    if len(vec) == 0:
        return 0
    elif len(vec) == 1:
        return vec[0]
    pvec = np.array([num for num in vec if num >= 0])
    nvec=sorted(pvec)
    return np.percentile(nvec, 99, interpolation='nearest')

def average(vec):
    if len(vec) ==0:
        return 0
    if len(vec) ==1:
        return vec[0]
    pvec = np.array([num for num in vec if num >= 0])
    nvec=sorted(pvec)
    del nvec[-1] # exclude the outliers
    return sum(nvec) / len(nvec)

def med(vec):
    #pvec = vec[vec>=0]
    nvec=sorted(vec)
    nlen=len(vec)
    if nlen ==0:
        return 0
    else:
        return np.percentile(nvec,50)