#!/bin/bash

if [ -z $1 ]
then
echo "no scheduler was specified!"
exit
else
scheduler=$1
fi

if [ -z $2 ];
then
	echo "please specify the private addresss this container will use"
	exit
else
	echo "received $2 as IP address"
	IP=$2
fi

if [ -z $3 ]
then
    echo "drop option not turned on as second parameter is empty"
    drop=0
  else
    drop=1.0
fi


host_port=8080
host_ssh_port=3333

## IMPORTANT!!
# change mounting directory to your directory

docker run  -it \
	--runtime=nvidia  \
	 --ipc=host \
	 --name frontend\
	 -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/resource,target=/root/org/gpu-let/resource \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/scripts,target=/root/org/gpu-let/scripts \
	--ip $IP --network my-net\
	-p $host_port:8080 \
	-p $host_ssh_port:22 \
	sbchoi/glet-server:latest \
	./executeFrontend_docker_ver.sh $scheduler $drop
