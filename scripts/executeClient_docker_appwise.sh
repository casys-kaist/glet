#!/bin/bash


#EXPERIMENT_TIME=3200
EXPERIMENT_TIME=10


if [ -z $1 ];
then
	echo "please specify result dir"
	exit
else
	echo "results will be stored under : " $1
	result_dir=$1
fi

if [ -z $2 ];
then 
	echo "please specifiy mode: chose between overlapped and seperate"
	exit
else
	echo "mode chosen : $2"
	mode=$2
fi



#SERVER RELATED VARs
ROOT_DIR=../
# dir for storing setup files and other resources
RESOURCE_DIR=$ROOT_DIR/resource
DATA_ROOT_DIR=$RESOURCE_DIR
BUILD_DIR=$PWD/../bin

RESULT_DIR=$PWD/$result_dir/$mode
mkdir -p $RESULT_DIR


id=0
APPS=('resnet50' 'vgg16')

nclient=1
request=$(($EXPERIMENT_TIME * 50))

for app in ${APPS[@]}
do
	echo $app
	echo $request
done

sleep 2

i=0
# address of frontend docker 
ADDR=10.10.0.20

per_model_dir=$RESOURCE_DIR/per-model-rates-$mode
echo "started booting clients"
for app in ${APPS[@]};
do
	
	echo "booting "$app
	if [ "$app" == "ssd-mobilenetv1" -o "$app" == "traffic" ];
	then
	input_txt='input-camera.txt'

	else
	input_txt='input.txt'
	fi

	for num in `seq 1 $nclient`
	do

	$BUILD_DIR/client --task $app --hostname $ADDR  --portno 8080 \
        --requests $request --batch 1 --rate 50 \
        --input $RESOURCE_DIR/$input_txt --skip_resize 1 --root_data_dir $DATA_ROOT_DIR \
        --dist exp --flux 0 1>$RESULT_DIR/$app-$num-client.csv &

	pids[${i}]=$!
	i=$((i+1))
	done #nclient 
done #app

echo "finished booting clients, waiting for " $EXPERIMENT_TIME "seconds + 20seconds for server to finish"

sleep_time=$((EXPERIMENT_TIME + 20))
sleep $sleep_time

for pid in ${pids[*]};
do
   #wait $pid
   kill -9 $pid
done #PID

#fi #EXP_FLAG
echo "all requests from client has returned" 

