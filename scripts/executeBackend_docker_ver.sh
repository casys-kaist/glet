#!/bin/bash


if [ -z $1 ]
then
    echo "please specifiy gpuid to attach the backend docker"
    exit
  else
    gpuid=$1
fi

MAX_NUM_OF_PROXYS=7 # 20,40,50(x2),60,80,100

RES_DIR=../resource/


BIN=../bin/backend


proxy_config=$RES_DIR/proxy_config.json
exp_config=$RES_DIR/config.json

echo $proxy_config

first_idx=$gpuid
last_idx=$gpuid

$BIN --backend_control_portno 8081 --backend_data_portno 8082 \
        --gpu_idxs $first_idx-$last_idx --nproxy $MAX_NUM_OF_PROXYS \
	--proxy_script startProxy_docker.sh \
        --config_json $exp_config --proxy_json $proxy_config \
	--full_proxy_dir /root/org/gpu-let/resource/proxy \
	--resource_dir $RES_DIR

