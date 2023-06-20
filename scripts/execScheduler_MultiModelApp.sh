#!/bin/bash

CMP_FLAG=1
UPDATE_FLAG=1 # flag indicating to update UPDATE_DI
EXP_FLAG=1

RES_DIR=../resource/
SIM_DIR=$RES_DIR/SchedConfigs
BIN=../bin/standalone_scheduler
CONFIG_DIR=$RES_DIR/MultiModelAppConfigs/
ROOT_RESULT_DIR=$RES_DIR/MultiModelAppSimulationResults


UPDATE_DIR=$RES_DIR/example_output_dir2


# Configurations to experiment
SCENS=('gpulet-int' 'gpulet-noint' 'greedy_part' 'timesharing_only')  

game_intervals=('200' '400' '600' '800' '1000' '1200' '1400' '1100' '1300' '1500' '1600' '1700' '1800')
game_intervals+=( '1900' '2000' '2100' '2200' '2300' '2400')
traffic_intervals=('100' '200' '300' '400' '450' '500' '550' '600' '700' '800')


WORKLOADS=('game' 'traffic')


mkdir -p $ROOT_RESULT_DIR

if [ $EXP_FLAG -eq 1 ];
then


for partition in ${SCENS[@]}
do
        RESULT_DIR=$ROOT_RESULT_DIR/$partition
        mkdir -p $RESULT_DIR
        rm $RESULT_DIR/*

	for workload in ${WORKLOADS[@]}
        do
            if [ "$workload" == 'game' ];then
                    INTERVALS=${game_intervals[@]}
            elif [ "$workload" == 'traffic' ]; then
                    INTERVALS=${traffic_intervals[@]}
            else
                    echo "no such workload as $workload"
                    exit 1
             fi   
	    echo $workload
            echo $partition

            for interval in ${INTERVALS[@]}
            do
               echo $interval

		taskfile=tasks_"$workload"_"$interval".csv
                configfile=config-$partition.json
                logfile="$workload"-"$interval"-"$partition"-log

		$BIN --resource_dir $RES_DIR --task_config $CONFIG_DIR/$taskfile \
		--sched_config $SIM_DIR/$configfile --output $RESULT_DIR/ModelList-"$workload"-"$interval".txt \
		--mem_config $RES_DIR/mem-config.json \
		--device_config $RES_DIR/device-config.json > "$RESULT_DIR"/$logfile

                done
        done
done
fi


if [ $UPDATE_FLAG -eq 1 ];then
    for partition in ${SCENS[@]}
    do
        RESULT_DIR=$ROOT_RESULT_DIR/$partition
        TGT_DIR=$UPDATE_DIR/$partition
        echo "updating $TGT_DIR "
        mkdir -p $TGT_DIR
        cp $RESULT_DIR/* $TGT_DIR/
    done
      
fi


