#!/bin/bash

if [ -z "$1" ]
then
    echo "please specify task to execute"
    exit 1
fi
task=$1

if [ -z "$2" ]
then
    echo "please specify # of reqeusts to provide"
    exit 1
fi
req=$2

if [ -z "$3" ]
then
    echo "please specify batch size"
    exit 1
fi
batch=$3

if [ -z "$4" ]
then
    echo "please specify rate (in seconds)"
    exit 1
fi
rate=$4

if [ -z "$5" ]
then
    echo "exp flag not specified, using UNIFORM distribution"
    dist="uni"
else
    echo "exp flag specified as "$5 ""
    dist="exp"
fi
if [ -z "$6" ]
then
    echo "flux flag not specified"
    flux_flag=0
else
    echo "FLUX FLAG specified!!"
    flux_flag=1
fi


RES_DIR=$PWD/../resource 
DATA_ROOT_DIR=$RES_DIR

BUILD_DIR=$PWD/../bin


if [ "$task" == "ssd-mobilenetv1" -o "$task" == "traffic" ];
then
input_txt='input-camera.txt'

else
input_txt='input.txt'

fi

# frontend server
ADDR=10.0.0.12

$BUILD_DIR/client --task $task --hostname $ADDR  --portno 8080 \
        --requests $req --batch $batch --rate $rate \
        --input $RES_DIR/$input_txt --skip_resize 1 --root_data_dir $DATA_ROOT_DIR \
         --dist $dist --flux $flux_flag
