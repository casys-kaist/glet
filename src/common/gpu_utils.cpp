#include "gpu_utils.h"
#include <nvml.h>
#include <map>
//#include "backend_delegate.h"

//extern std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

GPUUtil::GPUUtil(){
    nvmlInit();
}

GPUUtil::~GPUUtil(){
    nvmlShutdown();
}

uint64_t GPUUtil::GetTotalMemory(int id){
     nvmlDevice_t gpu0;
     nvmlDeviceGetHandleByIndex(id, &gpu0);
     nvmlMemory_t mem_info;
     nvmlDeviceGetMemoryInfo(gpu0,&mem_info);
     // return in MB (used to be in bytes)
     return mem_info.total/(1024*1024);
}

uint64_t GPUUtil::GetUsedMemory(int id){
    nvmlDevice_t gpu0;
     nvmlDeviceGetHandleByIndex(id, &gpu0);
     nvmlMemory_t mem_info;
     nvmlDeviceGetMemoryInfo(gpu0,&mem_info);
     // return in MB (used to be in bytes)
     return mem_info.used/(1024*1024);
}


