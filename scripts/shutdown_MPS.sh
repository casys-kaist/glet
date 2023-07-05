#!/bin/bash
if [ -z "$1" ] 
then
    echo "Number of GPUs not given! The sript will use nvidia-smi to detect number of GPUs "
    NUM=$(nvidia-smi --query-gpu=name --format=csv,noheader | wc -l)

else
    NUM=$1
fi

NUM=$((NUM-1))

pkill proxy # for killing proxy servers remaining in the system
for i in `seq 0 $NUM`
do
sudo nvidia-smi -i $i -c DEFAULT
done
echo quit | nvidia-cuda-mps-control
echo "MPS shutdown complete"
