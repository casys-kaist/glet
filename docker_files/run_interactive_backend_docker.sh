#!/bin/bash

if [ -z $1 ];
then

	echo "please specify which GPU ID ex) '0' = first gpu, '1' = second gpu"
	exit
else
	echo "received $1 as gpu ID"
	GPUID=$1
fi

if [ -z $2 ];
then

	echo "please specify the private addresss this container will use"
	exit
else
	echo "received $2 as IP address"
	IP=$2
fi

host_ctrl_port=$((8081+$GPUID*2))
host_data_port=$((8082+$GPUID*2))
host_ssh_port=$((2222+$GPUID))


## IMPORTANT!!
# change mounting directory to your directory

docker run --rm -it \
	--runtime=nvidia  \
	--name backend-$GPUID \
	-e NVIDIA_VISIBLE_DEVICES=$GPUID \
	 --ipc=host \
	 -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/resource,target=/root/org/gpu-let/resource \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/scripts,target=/root/org/gpu-let/scripts \
	--ip $IP --network my-net\
	-p $host_ctrl_port:8081 -p $host_data_port:8082\
	-p $host_ssh_port:22 \
	sbchoi/glet-server:latest  \
	./executeBackend_docker_ver.sh $GPUID

