#!/bin/bash

# get all files to be used for building docker 

mkdir -p tmp/
mkdir -p tmp/scripts

cp -R ../src tmp/
cp ../scripts/*.sh tmp/scripts/
cp ../scripts/*.py tmp/scripts/
cp ../CMakeLists.txt tmp/

ls tmp
ls tmp/scripts

docker build -t glet-server:latest .

# remove temporal directory
rm -rf tmp
