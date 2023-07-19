#!/bin/bash


## IMPORTANT: replace mount directory with your absolute directory
docker run --rm -it \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/resource,target=/root/org/gpu-let/resource \
	--mount type=bind,source=/home/sbchoi/zenodo/glet/scripts,target=/root/org/gpu-let/scripts \
	--gpus all \
	--network my-net\
	 -p 22 \
	sbchoi/glet-base:latest
