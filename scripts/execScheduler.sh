#!/bin/bash


RES_DIR=$PWD/../resource/
SIM_DIR=$RES_DIR
BIN=$PWD/../bin/standalone_scheduler
configfile=sched-config.json
taskfile=rates.csv

$BIN --resource_dir $RES_DIR --task_config $RES_DIR/$taskfile \
        --sched_config $SIM_DIR/$configfile --output $RES_DIR/ModelList.txt  \
        --mem_config $RES_DIR/mem-config.json \
	--device_config $RES_DIR/device-config.json

