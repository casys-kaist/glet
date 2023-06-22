#!/bin/bash

if [ -z "$1" ]
then
    echo "please specify task to execute"
    exit 1
fi
if [ -z "$2" ]
then
    echo "please specify # of reqeusts to provide"
    exit 1
fi
if [ -z "$3" ]
then
    echo "please specify batch size"
    exit 1
fi
if [ -z "$4" ]
then
    echo "please specify mean reqeust arrival time (in seconds)"
    exit 1
fi


ROOT_DIR=..
RES_DIR=$ROOT_DIR/resource/ # beware of the '/' at the end!!

BUILD_DIR=$ROOT_DIR/bin
task=$1
req=$2
batch=$3
mean=$4

input_txt='smaller_input.txt'


# The following might need to be changed accordingly for your server
set CUDA_VISIBLE_DEVICES=0
NSIGHT_BIN=/usr/local/cuda/nsight-compute-2022.1.1/nv-nsight-cu-cli

APP_CMD="$BUILD_DIR/standalone_inference --task $task --taskfile $RES_DIR/models/$task.pt --requests $req \
--batch $batch --mean $mean --input $RES_DIR/$input_txt --input_config_json $RES_DIR/input_config.json"

PROFILE_FILE="profile.csv"

round=1
sudo $NSIGHT_BIN --profile-from-start off $APP_CMD > $PROFILE_FILE &
echo $APP_CMD
PID=$!
wait $PID

