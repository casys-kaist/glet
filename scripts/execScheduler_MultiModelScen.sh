#!/bin/bash

exp_flag=1
update_flag=1

RES_DIR=../resource/
SIM_DIR=$RES_DIR/SchedConfigs
BIN=../bin/standalone_scheduler
CONFIG_DIR=$RES_DIR/MultiModelScenConfigs
ROOT_RESULT_DIR=$RES_DIR/SimulationResults

# the directory to update (update_flag must be set to true, for this to happen)
UPDATE_DIR=$RES_DIR/example_output_dir

# Configurations to experiment
SCENS=('gpulet-int' 'gpulet-noint' 'greedy_part' 'timesharing_only')  


# adding to workload list
workloads=()
for i in `seq 1 20`
do
	name=scen1-$i
	workloads+=("$name")
done
for i in `seq 1 15`
do
	name=scen2-$i
	workloads+=("$name")
done
for i in `seq 1 15`
do
	name=scen3-$i
	workloads+=("$name")
done
for i in `seq 1 15`
do
	name=scen4-$i
	workloads+=("$name")
done
for i in `seq 1 15`
do
	name=scen5-$i
	workloads+=("$name")
done

for name in ${workloads[@]}
do
	echo $name
done




mkdir -p $ROOT_RESULT_DIR

if [ $exp_flag -eq 1 ];
then


for scen in ${SCENS[@]}
do   
    RESULT_DIR=$ROOT_RESULT_DIR/$scen
    mkdir -p $RESULT_DIR
    for workload in ${workloads[@]}; do
        echo $workload
        echo $scen
        taskfile="$workload"-config.csv
        configfile=config-$scen.json
	logfile="$workload"-"$interval"-log
	$BIN --resource_dir $RES_DIR --task_config $CONFIG_DIR/$taskfile \
		--sched_config $SIM_DIR/$configfile --output $RESULT_DIR/"$workload"-result.txt \
		--mem_config $RES_DIR/mem-config.json \
		--device_config $RES_DIR/device-config.json > "$RESULT_DIR"/"$workload"-log


    done #workload
done #scen 
fi 


## Optional ##
# Use the following script when you want to copy/update the results right away to another experimental directory

if [ $update_flag -eq 1 ];
then
    for partition in ${SCENS[@]}
    do  
        RESULT_DIR=$ROOT_RESULT_DIR/$partition
        TGT_DIR=$UPDATE_DIR/$partition
    	echo "overwritting $TGT_DIR "
        mkdir -p $TGT_DIR
        cp $RESULT_DIR/* $TGT_DIR/
    done
    
fi
