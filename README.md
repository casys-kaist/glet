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


## Step-by-step building instructions

1. Download [libtorch](https://pytorch.org/cppdocs/installing.html) and extract all the content as **'libtorch'** under the root directory of this repo. (example: 'glet/libtorch')

2. Go to 'scripts/'

> cd scripts

3. Execute 'build_all.sh'

> ./build_all.sh

The script will use cmake to auto-configure build envrionments and build binaries.

**Example scripts will be added in the future**

# Future Plans

Below is a list of items/features that are planned to be added to this repo.

- Source code of all SW components used in experimentation: standalone inference binary, standalone scheduler, request generator, backend server, frontend server and proxy server

- Scripts used for executing and analyzing experiments

- Docker related files e.g.) Dockerfile

- Improved UI for SW components

- Misc: Refactoring for better code, consistency



# Contact

If you have any suggestions or questions feel free to send me an Email:

> sbchoi@casys.kaist.ac.kr


# Academic and Conference Papers

[**ATC**] "Serving Heterogeneous Machine Learning Models on Multi-GPU Servers with Spatio-Temporal Sharing", accepted for The 2022 USENIX Annual Technical Conference, July, 2022
