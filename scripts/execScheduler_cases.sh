#!/bin/bash

## Script for peak search

exp_flag=1

ROOT_DIR=..
RES_DIR=$ROOT_DIR/resource/
SIM_DIR=$RES_DIR/SchedConfigs
SCRIPT_DIR=$ROOT_DIR/scripts
BIN=$ROOT_DIR/bin/standalone_scheduler
# Configurations to experiment
SCENS=('ours' 'ideal')


# 3^9 -1 = (exclude case where all cases are 0)
# there are 19682 files to check 
nums=`seq 1  19682`


CONFIG_DIR=$RES_DIR/SchedCases
ROOT_RESULT_DIR=$RES_DIR/SchedulingResults

mkdir -p $ROOT_RESULT_DIR

if [ $exp_flag -eq 1 ];
then


for scen in ${SCENS[@]}
do  
	echo $scen
    RESULT_DIR=$ROOT_RESULT_DIR/$scen
    mkdir -p $RESULT_DIR
    for num in ${nums[@]}; do

        echo $num
        taskfile="$num"-config.csv
        configfile=config-gpulet-int.json
	latencyfile=latency.csv
	full_search_flag=0
	if [ $scen == "ours" ];
	then
		full_search_flag=0

	elif [ $scen == "ideal" ];
	then
		full_search_flag=1
	else 
		echo "unrecognized scenario $scen" 
	fi
	$BIN --resource_dir $RES_DIR --task_config $CONFIG_DIR/$taskfile \
		--sched_config $SIM_DIR/$configfile --output $RESULT_DIR/"$num"-result.txt \
		--mem_config $RES_DIR/mem-config.json  --device_config $RES_DIR/device-config.json  \
		--full_search $full_search_flag > "$RESULT_DIR"/"$num"-log


    done # num
done #scen 
fi # exp_flag


