#!/bin/bash


if [ -z $1 ]
then
echo "no scheduler was specified! default "mps_static" will be used"
scheduler='mps_static'
else
scheduler=$1
fi


if [ -z $2 ]
then
    echo "drop option not turned on as second parameter is empty"
    drop=0
  else
    drop=1.0
fi



MAX_NUM_OF_PROXYS=7 # 20,40,50(x2),60,80,100

RES_DIR=../resource/

BIN=../bin/frontend

sim_config=$RES_DIR/sim-config.json
mem_config=$RES_DIR/mem-config.json
latency_prof=$RES_DIR/latency.csv
proxy_config=$RES_DIR/proxy_config.json
rate_file=$RES_DIR/rates.csv
exp_config=$RES_DIR/config.json
model_list_file=$RES_DIR/ModelList.txt
device_config=$RES_DIR/device-config.json
backend_list=$RES_DIR/BackendList_docker.json


echo $proxy_config

$BIN --portno 8080 -s $scheduler --resource_dir $RES_DIR \
        --drop $drop --config_json $exp_config --proxy_json $proxy_config \
        --sim_config $sim_config --mem_config $mem_config \
        --latency_profile $latency_prof --init_rate  $rate_file --model_list $model_list_file \
        --backend_json $backend_list --device_config $device_config > log


