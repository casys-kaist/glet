#!/bin/bash

## Script for peak search(ps)
exp_flag=1
update_flag=1

ROOT_DIR=..
RES_DIR=$ROOT_DIR/resource
SCRIPT_DIR=$ROOT_DIR/scripts
SIM_DIR=$RES_DIR/SchedConfigs
BIN=../bin/standalone_scheduler
CONFIG_DIR=$RES_DIR/PeakSearchConfigs
ROOT_RESULT_DIR=$RES_DIR/PeakSearchResults
mkdir -p $ROOT_RESULT_DIR

# If you want to maintain another directory other than the result directory, specify the following directory
UPDATE_DIR=$RES_DIR/update_dir_example

# Configurations to experiment
SCENS=('gpulet-int' 'gpulet-noint' 'greedy_part' 'timesharing_only')

nums=`seq 1 299`

workloads=('traffic' 'game')
workloads+=('scen1' 'scen2' 'scen3' 'scen4' 'scen5')


if [ $exp_flag -eq 1 ];
then


for scen in ${SCENS[@]}
do   
    RESULT_DIR=$ROOT_RESULT_DIR/$scen
    mkdir -p $RESULT_DIR
    for workload in ${workloads[@]}; do
        for num in ${nums[@]}; do
        echo $workload
        echo $scen
        echo $num

        taskfile="$workload"-"$num"-config.csv
        configfile=config-$scen.json

  		$ROOT_DIR/bin/standalone_scheduler --resource_dir $RES_DIR \
        	--task_config $CONFIG_DIR/$taskfile --sched_config $SIM_DIR/$configfile \
        	--mem_config $RES_DIR/mem-config.json \
        	--device_config $RES_DIR/device-config.json\
        	--output $RESULT_DIR/"$workload"-"$num"-result.txt > $RESULT_DIR/"$workload"-"$num"-log

        done # num
    done #workload
done #scen 
fi # exp_flag

if [ $update_flag -eq 1 ];then
    echo "updating files to $UPDATE_DIR"
    for scen in ${SCENS[@]}
    do  
        RESULT_DIR=$ROOT_RESULT_DIR/$scen/
	UPDATE_SUB_DIR=$UPDATE_DIR/$scen
	mkdir -p $UPDATE_SUB_DIR
        cp $RESULT_DIR/* $UPDATE_SUB_DIR/
    done
      
fi #update_flag                     

