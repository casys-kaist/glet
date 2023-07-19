# Github repository for Gpu-let Prototype

To maximize the resource efficiency of inference servers, we proposed a key mechanism to exploit hardware support for spatial
partitioning of GPU resources. With the partitioning mechanism, a new abstraction layer of GPU resources is created with
configurable GPU resources. The scheduler assigns requests
to virtual GPUs, called **Gpu-lets**, with the most effective amount
of resources. The prototype framework auto-scales the required number of GPUs for a given workloads, minimizing the cost for cloud-based inference servers.
The prototype framework also deploys a remedy for potential interference
effects when two ML tasks are running concurrently in a GPU.


# Evaluated Environment

## OS/Software
- Ubuntu 18.04
- Linux Kernel 4.15
- CUDA 10.2
- cuDNN 7.6
- PyTorch 1.10

## Hardware

The prototype was evaluated with multi-GPU server with the following hardware:

- RTX 2080ti (11GB global memory)
- intel Xeon E5-2630 v4 
- Servers connected with 10 GHz Ethernet

# Getting Started

## Prerequisites

Install the following libraries and drivers to build the prototype

- LibTorch(PyTorch library for C++) = 1.10 

- CUDA >= 10.2

- CUDNN >= 7.6

- Boost >= 1.6 (script provided)

- opencv >= 4.0 (script provided)

- cmake >= 3.19, use cmake to build binaries (script provided)

- pandas (optional, only required for analyzing results)

## Prerequisites for running docker image

1. docker ver >= 20

2. nvidia-docker, for utilizing NVIDIA GPUs.


## Step-by-step building instructions

1. Download [libtorch](https://pytorch.org/cppdocs/installing.html) and extract all the content as **'libtorch'** under the root directory of this repo. (example: 'glet/libtorch')

2. Go to 'scripts/'

> cd scripts

3. Execute 'build_all.sh'

> ./build_all.sh

The script will use cmake to auto-configure build environments and build binaries.

# Running Examples

**More Example scripts will be added in the future**

Below are step-by-step examples for running the server and standalone components

## Executing inference on a single local GPU

Use 'execLocal.sh' to execute ML inference on local GPU. Useful for testing whether you have installed compatible SW stack and profiling latency.

1. Make sure you have downloaded models you want to execute and store them under 'resource/models/'.

2. Go to 'scripts/' 
> cd scripts

3. Execute execLocal.sh with parameters: 1) name of model you want to execute, 2) number of executions 3) batch size 4) interval between executions 5) (optional) percentage of computing resource.
> example) ./execLocal.sh resnet50 1000 1 0.1 50

## Executing offline scheduler

*Highly recommended that you replace example profile files with profile info on the platform you wish to execute before you use the offline scheduler*

The offline scheduler is used for generating static scheduling results (default file name is '*ModelList.txt*' but the name of the file can be customized with the --*output* argument). 
The static results are used as a guideline for the frontend server's *static* scheduler
and used for debugging the scheduler.  

The following is step-by-step explanation on how to setup the scheduler. Don't worry! We have also prepared example files for each step.
Please refer to *execScheduler_MultiModelApp.sh* and *execScheduler_MultiModelScen.sh*, if you just want an example of how to execute the scheduler.

(and please refer to src/standalone_scheduler.cpp for further details).

1. Make task configuration (as csv) files.
Task configuration file should contain the number of models, *N*, on the first line of file. For the next '*N*' lines, each line must specify (in the exact order) the ID of the model, incoming request rate (RPS) and SLO (in ms). The following command is an example of automating the process of creating task configuration files for multi model scenarios. Please refer the *createMultiModelAppConfigs.py* for multi model applications. 

> python createMultiModelScenConfigs.py *output_dir*

2. Prepare scheduler configuration file (default is resource/sim-config.json). 
This JSON file specifies 1) the type and number of GPU that should be used for scheduling (each type of device should be provided with device configuration files and more details are specified in the *next step*), 2) various flags regarding interference and incremental scheduling and 3) the amount of latency buffer to use when scheduling. 

3. Prepare device configuration file(s) and directories for each type of GPU you want to use for scheduling. Please refer to 'resource/device-config.json' and make sure that you have all the related files specified in device-config.json.


## Experimenting on Multiple Servers
Below are example scripts that will help you get started when experiementing with multiple servers.

1. setupServer.sh: Boots servers that are listed within the script. *Make sure each server can be accessed by ssh without passwords.*

2. shutdownServer.sh: Shutdowns servers that are listed within the script. The script shutdowns the backend servers and frontend servers in order. 

3. experMultiModelApp.sh: Boots servers (with setupServer.sh), generates requests for given app, shutdowns servers (with shutdownServer.sh) and analyzes the results. All scripts required for analyzing are provided in the repo.

4. experMultiModelScen.sh: Works simiarliy to *experMultiModelApp.sh* but experiments with a given configuration (JSON) file which specifies the request rate of each model.

5. experiment_ps.sh: An example script for showing how to use *experMultiModelApp.sh* and *experMultiModelScens.h*.


# Future Plans (*Updated 2023-07-18*)


Below is a list of items/features that are planned to be added to this repo.

- Source code of all SW components used in experimentation: standalone inference binary, standalone scheduler, request generator, backend server, frontend server and proxy server (**completed**)

- Scripts used for executing and analyzing experiments (**completed**)

- Docker related files e.g.) Dockerfile and scripts (**ongoing**)

- Improved UI for SW components 

- Misc: Refactoring for better code, consistency



# Contact

If you have any suggestions or questions feel free to send me an Email:

> sbchoi@casys.kaist.ac.kr


# Academic and Conference Papers

[**ATC**] "Serving Heterogeneous Machine Learning Models on Multi-GPU Servers with Spatio-Temporal Sharing", accepted for The 2022 USENIX Annual Technical Conference, July, 2022
