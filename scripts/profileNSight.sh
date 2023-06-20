#!/bin/bash

GPU_FLAG=1
ANALYZE_FLAG=1

# DIRS
ROOT_DIR=$HOME/public/glet
echo $ROOT_DIR
SCRIPT_DIR=$ROOT_DIR/scripts
ROOT_RESULT_DIR=$ROOT_DIR/data

ANALYZE_PY=parseNSight.py

BATCHES=('1' '2' '4' '8' '12' '16' '20' '24' '28' '32')
APPS=('densenet161' 'mnasnet1_0' 'mobilenet_v2' 'bert')
#APPS+=('resnet50' 'lenet' 'vgg16' 'ssd-mobilenetv1' 'googlenet')
apps_num=${#APPS[@]}
mean=0.1
req=1
RESULT_DIR=$ROOT_RESULT_DIR/nsight-results
mkdir -p $RESULT_DIR
for batch in ${BATCHES[@]}
do
if [ $GPU_FLAG == '1' ]
then 
IMG_SH=$SCRIPT_DIR/execLocal_nsight.sh

for app in ${APPS[@]}
do
        echo "profiling "$app " on batch size" $batch "on gpu"
        # start client
        $IMG_SH $app $req $batch $mean 
        mv profile.csv $RESULT_DIR/$batch-$app-gpu.csv
        
done #benchmarks
fi #GPU_FLAG
done #BATCHES

#done #option
###############################################
# analyze results
##############################################

if [ $ANALYZE_FLAG == '1' ]
then
   python $ANALYZE_PY $RESULT_DIR nsight-output.txt

fi
