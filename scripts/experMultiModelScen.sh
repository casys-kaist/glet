#!/bin/bash


#FLAGS
server_flag=0 # flag for starting up remote server
exp_flag=0  # flag for sending clients
analyze_flag=0 # for analyzing completed experiment

if [ -z $1 ]
then
	echo "no iteration number was specified"
	exit 1
else
	
	echo "this experiment will be iterated for "$1 "times"
	iternum=$1
fi

if [ -z $2 ]
then
	echo "no request rate input file was specified"
	exit 1
else
	echo "all request's rate will be setted as written in "$2
	req_rate_file=$2
fi

if [ -z $3 ]
then
	echo "no distribution specified "
	exit 1
else
	if [ "$3" == "uni" ]
	then
		echo "will use uniform distribution"
		dist=$3
	elif [ "$3" == "exp" ]
	then
	       echo "will use exponential distribution"
	       dist=$3
       	else
	      	echo "unrecognized distribution $3" " check your script!"
		exit 1
		fi
fi

if [ -z $4 ]
then
        echo "no scheduler is specified"
        exit 1
else
        echo "will pass $4 as scheduler to frontend server"
        scheduler=$4
fi

if [ -z $5 ]
then
	echo "please specify result directory"
	exit 1
else
	echo "results will be stored under "$5
	root_result=$5
fi

# this script has been modified for ASPLOS'21 version
REMOTE_URL='10.0.0.12'

# DIRS
ROOT_DIR=$HOME/org/gpu-let
RES_DIR=$ROOT_DIR/resource
SCRIPT_DIR=$ROOT_DIR/scripts
DATA_DIR=$ROOT_DIR/data


#SERVER_DIR=$ROOT_DIR/service
#RATE_DIR=$RES_DIR/rates

#CLIENT SCRIPTS
IMG_SH=execReqGen.sh


#ANALYZING SCRIPT
PY_ANALYZE=analyzeServerClient.py
PY_ANALYZE2=analyzeScheduleDecisions.py
PY_ITER_ANALYZE=analyzeInterScheduleResults.py
PY_ANALYZE_GPU=parseNVML.py

## 


declare -A intervals
declare -A nclients
declare -A requests


models=([0]='lenet1' [6]='googlenet' [7]='resnet50' [8]='ssd-mobilenetv1' [9]='vgg16')


APPS=()

SKIP_FIRST=1

#declare -A intervals_per_model
declare -A requests_per_model
declare -A nclients_per_model
declare -A rates_per_model

while IFS=, read id rate SLO
do	
	if [ $SKIP_FIRST -eq 1 ]; then 
		SKIP_FIRST=0
		continue
	fi
	echo $id
	echo ${models[$id]}
	echo $rate
	echo $SLO
	# spawn more client processes for better request rates
	if [ $rate -le 200 ];
	then
		nclient=1
	elif [ $rate -le 400 ];
	then 
		nclient=2
	elif [ $rate -le 600 ];
	then 
		nclient=3
	else
		nclient=4
	fi
	rate=$((rate/nclient))
	request=$((20 * rate))	

	requests_per_model[${models[$id]}]=$request
	rates_per_model[${models[$id]}]=$rate
	nclients_per_model[${models[$id]}]=$nclient
	echo "rate: " ${rates_per_model[${models[$id]}]}
	echo "nclient:  "${nclients_per_model[${models[$id]}]}
	echo "request: "${requests_per_model[${models[$id]}]}  
	APPS+=(${models[$id]})

done < $req_rate_file
sleep 2


config_file='config-models.json'

ROOT_RESULT_DIR=$root_result

echo "results will be stored under  "$ROOT_RESULT_DIR
mkdir -p $ROOT_RESULT_DIR

for app in ${APPS[@]};
do
	echo $app " will be profiled with "${nclients_per_model[$app]}" threads for " ${requests_per_model[$app]} "tasks with rate of  " ${rates_per_model[$app]}
done

if [  "$server_flag" -eq 1 ]
then

echo "IF YOU WANT TO BACK OFF, press ctrl + c !! YOU HAVE 10 seconds"
sleep 10
fi


for iter in `seq 1 $iternum`
do
	
#####################
#experiment
#####################

	if [ "$exp_flag" -eq 1 ]
	then
		go=False
		RESULT_DIR=$ROOT_RESULT_DIR/$iter/$scheduler
		mkdir -p $RESULT_DIR
		while [ "$go" == "False" ]
		do
		if [ $server_flag -ne 1 ]
		then
			go=True		
		fi

		#wait for previous server to fully shutdown
		if [ "$server_flag" -eq 1 ]
		then
		# start remote server
		ssh $REMOTE_URL "cd $RES_DIR; cp $config_file config.json"
		./shutdownServer.sh
		sleep 3
		./setupServer.sh $scheduler 4 1
		sleep 10
		fi # server_flag
		echo "clients started sending request" 
		i=0
		for app in ${APPS[@]}; 
		do

			for num in `seq 1 ${nclients_per_model[$app]}`
			do
			if [ "$dist" == "exp" ]
			then
			./$IMG_SH $app ${requests_per_model[$app]} 1 ${rates_per_model[$app]} 1 1>$RESULT_DIR/$app-$num-client.csv &
			elif [ "$dist" == "uni" ] 
			then 
			./$IMG_SH $app ${requests_per_model[$app]} 1 ${rates_per_model[$app]} 1>$RESULT_DIR/$app-$num-client.csv &
			fi
			pids[${i}]=$!
			i=$((i+1))
			done #nclient 
		done #app
		echo "waiting for server to compute"
		sleep 30
		for pid in ${pids[*]}; 
		do
			wait $pid
			#kill -9 $pid
		done
		#echo "all requests from client has returned" 
		if [ "$server_flag" -eq 1 ]
		then
		echo "waiting for server to finish!"
		sleep 3
		# 4 stop server and clean up data
		#ssh $REMOTE_URL "pkill djinn"
		./shutdownServer.sh
		sleep 20
		scp $REMOTE_URL:$SCRIPT_DIR/log.txt $RESULT_DIR/server-model.csv
		scp $REMOTE_URL:$SCRIPT_DIR/Applog.txt  $RESULT_DIR/server-app.csv
		#scp $REMOTE_URL:$SCRIPT_DIR/throughput_log.txt  $RESULT_DIR/server-throughput.csv
		#scp $REMOTE_URL:$SCRIPT_DIR/queue_log.txt  $RESULT_DIR/server-queue.csv
		scp $REMOTE_URL:$SCRIPT_DIR/log  $RESULT_DIR/server-log.txt

		ssh $REMOTE_URL "rm $SCRIPT_DIR/log.txt; rm $SCRIPT_DIR/Applog.txt" # erase files so that results dont get mixed up
		fi #server_flag
	done # go

	fi # exp_flag
done # iter

if [ $analyze_flag -eq 1 ]
then
    for iter in `seq 1 $iternum` 
    do
	RESULT_FILE=$iter-schd_metrics.csv
	# erase file just in case previous analysis influences current results
	rm $ROOT_RESULT_DIR/$RESULT_FILE
	python $SCRIPT_DIR/$PY_ANALYZE $ROOT_RESULT_DIR/$iter $ROOT_RESULT_DIR/$RESULT_FILE
    echo "Metric results are stored in $ROOT_RESULT_DIR/$RESULT_FILE"
    done
    RESULT_FILE=schd_decisions.csv
    rm $ROOT_RESULT_DIR/$RESULT_FILE
    python $SCRIPT_DIR/$PY_ITER_ANALYZE $ROOT_RESULT_DIR  $ROOT_RESULT_DIR/$RESULT_FILE $iternum
    echo "Average of iterations are stored in $ROOT_RESULT_DIR/$RESULT_FILE"
fi # analyze_flag

