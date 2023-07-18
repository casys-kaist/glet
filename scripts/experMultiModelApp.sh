#!/bin/bash

#FLAGS
server_flag=1 #flag for starting up remote server
exp_flag=1 #flag for sending clients
analyze_flag=1 # for analyzing completed experiment

if [ -z $1 ] # specify which app 
then
	echo "please specify ONE of following: 'game' or 'traffic'"
	exit 1
else
	if [ $1 == "game" -o $1 == "traffic" ]
	then
		tasks=$1
	else 
		echo $1 "is not allowed"
		exit
	fi
fi

if [ -z $2 ]
then
	echo "no iteration number was specified"
	exit 1
else
	echo "this experiment will be iterated for "$2 "times"
	iternum=$2
fi

if [ -z $3 ]
then
	echo "no request rate specified"
	exit 1
else
	echo "all request's average interval will be setted to "$3
	input_rate=$3
fi

if [ -z $4 ]
then
	echo "no distribution specified "
	exit 1
else
	if [ "$4" == "uni" ]
	then
		echo "will use uniform distribution"
		dist=$4
	elif [ "$4" == "exp" ]
	then
	       echo "will use exponential distribution"
	       dist=$4
       	else
	      	echo "unrecognized distribution $4" " check your script!"
		exit 1
		fi
fi

if [ -z $5 ]
then
	echo "no scheduler is specified"
	exit 1
else
 	echo "will use scheduler $5"
	scheduler=$5
	if [ "$scheduler" == "mps_static" ];
	then
		echo "Do NOT forget to rename (or copy) scheduling result to resource/ModelList.txt for frontend server"
	fi
fi

if [ -z $6 ]
then
	echo "no result directory was specified!, please specify result dir"
	exit 1
else
	echo "results will be stored under "$6
	root_result=$6
fi

REMOTE_URL='10.0.0.12'
# DIRS
ROOT_DIR=../ 
RES_DIR=$ROOT_DIR/resource
SCRIPT_DIR=$ROOT_DIR/scripts
DATA_DIR=$ROOT_DIR/data

#SERVER SCRIPTS
SERVER_SH=executeServer.sh

#CLIENT SCRIPTS
IMG_SH=execReqGen.sh

#ANALYZING SCRIPT
PY_ANALYZE=analyzeServerClient.py

#PY_ANALYZE=analyzeAppPerf.py
PY_ITER_ANALYZE=analyzeInterScheduleResults.py

if [ $input_rate -le 200  ]; then
	nclient=1 #number of identical clients to generate
elif [ $input_rate -le 400 ]; then
	nclient=2
elif [ $input_rate -le 600 ]; then
	nclient=3
elif [ $input_rate -le 800 ]; then
	nclient=4
elif [ $input_rate -le 1000 ]; then
	nclient=5
elif [ $input_rate -le 1200 ]; then
	nclient=6 
elif [ $input_rate -le 1400 ]; then
	nclient=7
elif [ $input_rate -le 1600 ]; then
	nclient=8
elif [ $input_rate -le 1800 ]; then
	nclient=9
elif [ $input_rate -le 2000 ]; then
	nclient=10
else ##  over 2000
	nclient=10 
fi
req_rate=$((input_rate / nclient))
requests=$((req_rate*20))


if [ $tasks == "traffic" ]
then
	APPS=('traffic')
	config_file='config-apps.json'
elif [ $tasks == "game" ]
then
	APPS=('game')
	config_file='config-apps.json'

fi
apps_num=${#APPS[@]}
intervals_num=${#intervals[@]}

ROOT_RESULT_DIR=$root_result

for app in ${APPS[@]};
do
	echo $app " will be executing "$nclient" threads for " $requests "tasks with rate of  " $req_rate
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
			./shutdownServer.sh
			# start remote server
			ssh $REMOTE_URL "cd $RES_DIR; cp $config_file config.json"
			# turn off drop for self_tuning

			if [ $scheduler == 'self_tuning' ]; 
			then 
				./setupServer.sh $scheduler 4
			else
				./setupServer.sh $scheduler 4 1
			fi # self_tuning
		fi # server_flag
		echo "clients started sending request" 
		i=0
		for num in `seq 1 $nclient`
		do
			for app in ${APPS[@]}; 
			do
				echo $requests
				if [ "$dist" == "exp" ]
				then
					./$IMG_SH $app $requests 1 $req_rate 1 1>$RESULT_DIR/$app-$num-client.csv &
				elif [ "$dist" == "uni" ] 
				then 
				./$IMG_SH $app $requests 1 $req_rate 1>$RESULT_DIR/$app-$num-client.csv &
				fi
				pids[${i}]=$!
				i=$((i+1))
			done #app
		done #nclient
		echo "waiting for server to compute"
		sleep 30
		for pid in ${pids[*]}; 
		do
			wait $pid
		done
		if [ "$server_flag" -eq 1 ]
		then
			echo "waiting for server to finish!"
		# stop server and clean up data
		./shutdownServer.sh
		# give time for closing socket on OS
		sleep 20
		scp $REMOTE_URL:$SCRIPT_DIR/log.txt $RESULT_DIR/server-model.csv
		scp $REMOTE_URL:$SCRIPT_DIR/Applog.txt  $RESULT_DIR/server-app.csv
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
    RESULT_FILE=schd_decisions-$input_rate.csv
    rm $ROOT_RESULT_DIR/$RESULT_FILE
    python $SCRIPT_DIR/$PY_ITER_ANALYZE $ROOT_RESULT_DIR  $ROOT_RESULT_DIR/$RESULT_FILE $iternum
    echo "Average of iterations are stored in $ROOT_RESULT_DIR/$RESULT_FILE"
fi # analyze_flag

