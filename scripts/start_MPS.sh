#!/bin/bash

if [ -z "$1" ] 
then
    echo "Number of GPUs not given! The sript will use nvidia-smi to detect number of GPUs "
    NUM=$(nvidia-smi --query-gpu=name --format=csv,noheader | wc -l)
else
    NUM=$1
fi
echo "Starting MPS on $NUM GPUs"
NUM=$((NUM-1))
GPUS=`seq 0 $NUM`

# form comma seperated strings

for id in ${GPUS[@]}
do
	str="$str"$id
	if [ $id -ne $NUM ]
	then
		str="$str"","
	fi

done

export CUDA_VISIBLE_DEVICES=$str

for id in ${GPUS[@]}
do
    sudo nvidia-smi -i $id -c EXCLUSIVE_PROCESS
done


export CUDA_MPS_PIPE_DIRECTORY=/tmp/nvidia-mps # Select a location that’s accessible to the given $UID

export CUDA_MPS_LOG_DIRECTORY=/tmp/nvidia-log # Select a location that’s accessible to the given $UID

nvidia-cuda-mps-control -d # Start the daemon

echo "start_server -uid $(id -u sbchoi)" |  nvidia-cuda-mps-control # start server manually
echo "MPS initiation complete"
