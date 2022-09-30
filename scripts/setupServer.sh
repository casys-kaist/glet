#!/bin/bash



if [ -z $1 ];
then
	echo "please provide scheduler to execute in frontend"
	exit
else
	echo "frontend will use $1 scheduler"
	SCHED=$1
fi

if [ -z $2 ];
then
	echo "please specifiy how many GPUs to use in backends"
	exit 
else
	echo "$2 will be used"
	ngpus=$2

fi
if [ -z $3 ];
then
	echo "drop flag not specified, drop if off "
	drop="no"
else
	echo "drop flag specified! drop is on "
	drop="yes"
fi

#backend_urls=('10.0.0.12' '10.0.0.20')
backend_urls=('10.0.0.20' '10.0.0.12')

declare -A ngpus_per_url
SCRIPT_DIR=$PWD
used_gpus=0
frontend_urls=('10.0.0.12')
for url in ${backend_urls[@]}
do
	ngpus_per_url[$url]=0
done

break_flag=0
org_ngpu=$ngpus
while [ $ngpus -gt 0 ]
do
	for url in ${backend_urls[@]}
	do
		if [ $ngpus -le 0 ];
		then
			break_flag=1
			break
		fi
		curr_num=${ngpus_per_url[$url]}
		curr_num=$((curr_num +1))
		ngpus_per_url[$url]=$curr_num
		ngpus=$((ngpus-1))
	done
	if [ $break_flag -eq 1 ];
	then
		break
	fi
done

for url in ${backend_urls[@]}
do
	echo "backend $url will be using ${ngpus_per_url[$url]} gpus"
done


for url in ${backend_urls[@]}
do
	if [ ${ngpus_per_url[$url]} -eq 0 ];
	then
		continue
	fi
	echo "setup backend in " $url
	ssh $url "cd $SCRIPT_DIR; sh executeBackend.sh ${ngpus_per_url[$url]} > backend_log" & 
	used_gpus=$((used_gpus +1))
done
sleep 5


for url in ${frontend_urls[@]}
do
	echo "setup frontend in " $url
       	if [ "$drop" == "no" ]	
	then
		ssh $url "cd $SCRIPT_DIR; sh executeFrontend.sh $SCHED> log" &
	else
		ssh $url "cd $SCRIPT_DIR; sh executeFrontend.sh $SCHED 1 > log" &
	fi
	sleep_time=$((org_ngpu*8 + 12))
	echo "waiting time: $sleep_time"
	sleep $sleep_time
done

