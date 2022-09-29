#!/bin/bash

sudo apt-get -y update && sudo apt-get -y upgrade

# basic sw 
sudo apt-get -y install build-essentials wget git zip 

# libraries required for building
sudo apt-get -y install libboost-all-dev libgoogle-glog-dev libssl-dev 

# script provided installation
./install_cmake.sh

./install_opencv_cpp.sh

