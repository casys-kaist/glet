#!/bin/bash

exp_flag=1
send_flag=1
iter_num=3
if [ -z $1 ]
then
	echo "please specify partition (Only used for naming result directory)"
	exit
else
	echo "received prefix: "$1", all result directories will have this prefix"
	prefix=$1
fi

# the frontend URL
REMOTE_URL='10.0.0.12'

ROOT_DIR=../
SCRIPT_DIR=$ROOT_DIR/scripts
RES_DIR=$ROOT_DIR/resource
RATE_DIR=$RES_DIR/PeakSearchConfigs
ROOT_LIST_DIR=$RES_DIR/PeakSearchResults

# Configurations to experiment
SCENS=('gpulet-int' 'gpulet-noint' 'greedy_part' 'timesharing_only')

DISTS=('exp')

if [ $exp_flag -eq 1 ]
then

	for scen in ${SCENS[@]}
	do
	echo "$scen"

## The following cases are picked randomly just to show an example. They are not the actual values that were used in the experiment.
## Please specify your own config files if you want to use this script for your own experiments

	if [ $scen == 'timesharing_only' ]; then
CASE=('game-69' 'game-68' 'game-67')
CASE+=('traffic-34' 'traffic-33' 'traffic-32')
CASE+=('scen1-14' 'scen1-13' 'scen1-12')
CASE+=('scen2-33' 'scen2-32' 'scen2-31')
CASE+=('scen3-9' 'scen3-8' 'scen3-7')
CASE+=('scen4-43' 'scen4-42' 'scen4-41')
CASE+=('scen5-30' 'scen5-29' 'scen5-28')

	elif [ $scen == 'gpulet-int' ]; then
CASE=('game-69' 'game-68' 'game-67')
CASE+=('traffic-34' 'traffic-33' 'traffic-32')
CASE+=('scen1-14' 'scen1-13' 'scen1-12')
CASE+=('scen2-33' 'scen2-32' 'scen2-31')
CASE+=('scen3-9' 'scen3-8' 'scen3-7')
CASE+=('scen4-43' 'scen4-42' 'scen4-41')
CASE+=('scen5-30' 'scen5-29' 'scen5-28')

	elif [ $scen == 'greedy_part' ]; then
CASE=('game-69' 'game-68' 'game-67')
CASE+=('traffic-34' 'traffic-33' 'traffic-32')
CASE+=('scen1-14' 'scen1-13' 'scen1-12')
CASE+=('scen2-33' 'scen2-32' 'scen2-31')
CASE+=('scen3-9' 'scen3-8' 'scen3-7')
CASE+=('scen4-43' 'scen4-42' 'scen4-41')
CASE+=('scen5-30' 'scen5-29' 'scen5-28')

	else  # gpulet-noint
CASE=('game-69' 'game-68' 'game-67')
CASE+=('traffic-34' 'traffic-33' 'traffic-32')
CASE+=('scen1-14' 'scen1-13' 'scen1-12')
CASE+=('scen2-33' 'scen2-32' 'scen2-31')
CASE+=('scen3-9' 'scen3-8' 'scen3-7')
CASE+=('scen4-43' 'scen4-42' 'scen4-41')
CASE+=('scen5-30' 'scen5-29' 'scen5-28')
	fi

	for workload in ${CASE[@]}
	do
		schedulers=('mps_static')
		for dist in ${DISTS[@]}
		do
			for s in ${schedulers[@]}
			do
				echo "----"
				echo $workload
				echo $scen
				echo $dist
				echo $s

				list_dir=$ROOT_LIST_DIR/$scen
				partition_file="$workload"-result.txt
				rate_file=$RATE_DIR/"$workload"-config.csv
				ssh $REMOTE_URL " cp $list_dir/$partition_file $RES_DIR/ModelList.txt"
				if [[ $workload == *"game"* ]];
				then
					echo "got game"
					IFS='-' 
					read -ra array <<< "$workload"
					work=${array[0]}
					rate=${array[1]}
					rate=$((rate*10))
					echo "$work"
					echo "$rate"
					IFS=''
					root_result_dir=$PWD/$prefix/$dist/"$scen"_"$rate"_"$work"
					mkdir -p $root_result_dir
					echo $root_result_dir
					./experMultiModelApp.sh $work $iter_num $rate $dist $s $root_result_dir
				elif [[ $workload == *"traffic"* ]]
				then 
					echo "got traffic"
					IFS='-' 
					read -ra array <<< "$workload"
					work=${array[0]}
					rate=${array[1]}
					rate=$((rate*10))
					echo "$work"
					echo "$rate"
					IFS=''
					root_result_dir=$PWD/$prefix/$dist/"$scen"_"$rate"_"$work"
					echo $root_result_dir
					mkdir -p $root_result_dir
					./experMultiModelApp.sh $work $iter_num $rate $dist $s $root_result_dir
				else	
					echo "group benchmark"
					root_result_dir=$PWD/$prefix/$dist/"$scen"_"$workload"
					echo $root_result_dir
					mkdir -p $root_result_dir
					./experMultiModelScen.sh $iter_num $rate_file $dist $s $root_result_dir
				fi
			done
		done
	done
done
fi #exp_flag
