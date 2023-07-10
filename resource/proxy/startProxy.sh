#!/bin/bash

if [ -z $1 ]
then
        echo "no device was specified! please specify a device ex) startProxy.sh 0"
        exit
fi

if [ -z $2 ]
then
        echo "no threadcap was specified, using default 100%"
        cap=100

else
        echo "using threadcap "$2
        cap=$2
fi
if [ -z $3 ]
then
        echo "no dedup was specified, using default 0"
        dedup=0

else
        echo "using dedup "$3
        dedup=$3
fi
if [ -z $4 ]
then
        echo "no file for model listing was provided, using default file"
        model_list='../ModelList.txt'

else
        echo "model list file "$4
        model_list=$4
fi
if [ -z $5 ]
then
        echo "no flag indicating whether first partition or second partition"
        exit

else
        echo "use $5 partition "
        part_flag=$5
fi
if [ -z $6 ]
then
	echo "please specify the total number of gpus in this node"
	exit
else
	ngpu=$6
fi
if [ -z $7 ]
then
	echo "please specify the maximum number of partitions per GPU"
	exit
else
	nparts=$7
fi


cd $PWD
device=$1
# resource directory 
COMMON_DIR=$PWD/../
BIN=$PWD/../../bin/proxy

SERVER_PID=$(echo get_server_list | nvidia-cuda-mps-control)

if [ -z $SERVER_PID ]
then
        echo "there is no mps server, make sure that you have turned on MPS Server, proxy not turned on"
        exit
fi


export CUDA_VISIBLE_DEVICES=$device
echo set_active_thread_percentage $SERVER_PID $cap | nvidia-cuda-mps-control
echo $PWD

if [ "$device" -eq 0 ] #respect NUMA 
then
        taskset -c 0-4 $BIN --common $COMMON_DIR --devid $device  --threadcap $cap --dedup $dedup --model_list $model_list --partition $part_flag --ngpu $ngpu --npart $nparts> log_"$device"_"$cap"_"$dedup"

elif [ "$device" -eq 1 ] #respect NUMA 
then
        taskset -c 5-9 $BIN --common $COMMON_DIR --devid $device  --threadcap $cap --dedup $dedup --model_list $model_list --partition $part_flag --ngpu $ngpu --npart $nparts > log_"$device"_"$cap"_"$dedup"

elif [ "$device" -eq 2 ] #respect NUMA 
then
        taskset -c 10-14 $BIN --common $COMMON_DIR --devid $device  --threadcap $cap --dedup $dedup --model_list $model_list --partition $part_flag --ngpu $ngpu --npart $nparts > log_"$device"_"$cap"_"$dedup"

else
        taskset -c 15-19 $BIN --common $COMMON_DIR --devid $device  --threadcap $cap --dedup $dedup --model_list $model_list --partition $part_flag  --ngpu $ngpu --npart $nparts > log_"$device"_"$cap"_"$dedup"

fi

#revert to old state just in case
export CUDA_VISIBLE_DEVICES=0


