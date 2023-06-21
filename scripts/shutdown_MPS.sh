#!/bin/bash
if [ -z "$1" ] 
then
    echo "number of GPUs not given "
    exit
else
    NUM=$1
    NUM=$((NUM-1))
fi

pkill proxy # for killing proxy servers remaining in the system
for i in `seq 0 $NUM`
do
sudo nvidia-smi -i $i -c DEFAULT
done
echo quit | nvidia-cuda-mps-control

