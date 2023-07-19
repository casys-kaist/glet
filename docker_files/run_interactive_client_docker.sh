#!/bin/bash


if [ -z $1 ];
then

	echo "please specify the private addresss this container will use"
	exit
else
	echo "received $1 as IP address"
	IP=$1
fi

if [ -z $2 ];
then

	echo "please specify result_dir"
	exit
else
	echo "received $2 as result_dir"
	result_dir=$2
fi


host_ssh_port=$((2221))

## IMPORTANT!!
# change mounting directory to your directory

docker run --rm -it \
	--runtime=nvidia  \
	--name client \
	 --ipc=host \
	 -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/resource,target=/root/org/gpu-let/resource \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/scripts,target=/root/org/gpu-let/scripts \
	--ip $IP --network my-net\
	-p $host_ssh_port:22 \
	sbchoi/glet-server:latest \
	 ./executeClient_docker_appwise.sh $result_dir overlapped

