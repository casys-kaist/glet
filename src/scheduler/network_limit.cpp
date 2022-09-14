#include "network_limit.h"
#include <iostream>
#include "json/json.h"
#include <assert.h>

NetworkLimitChecker::NetworkLimitChecker(){}
NetworkLimitChecker::~NetworkLimitChecker(){}
int NetworkLimitChecker::setupPerTaskInputDimension(std::string json_file){
    std::cout << __func__ << ": reading JSON File: " << json_file
    <<std::endl;
    Json::Value root;
    std::ifstream ifs;
    ifs.open(json_file);
    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << __func__ << ": Failed parsing from stream" << std::endl;
        std::cout << errs << std::endl;
        ifs.close();
        return EXIT_FAILURE;
    }
    for(unsigned int i=0; i< root["Models"].size(); i++){
        int id = root["Models"][i]["proxy_id"].asInt();
        int size =1 ;     
         for(unsigned int j=0; j<root["Models"][i]["input_dim"].size(); j++ ){
             size *= root["Models"][i]["input_dim"][j].asInt();
        }
        // turn into bytes according to type
        if(root["Models"][i]["input_type"].asString() == "FP32" ){
            size *=4;
        }
        else if (root["Models"][i]["input_type"].asString() == "INT64"){
            size *=8;
        }
        else{
            std::cout<<"Unrecognized type " << root["Models"][i]["input_type"].asString() << std::endl;
            return EXIT_FAILURE;
        }
        _taskIDtoRequiredBytesPerRequest[id]=size;
        #ifdef SCHED_DEBUG
        std::cout << __func__ <<": setted up " << id << " as " <<  _taskIDtoRequiredBytesPerRequest[id]
        <<std::endl;
        #endif
    }
    ifs.close();
    return EXIT_SUCCESS;   
}

double NetworkLimitChecker::getRequiredBandwidth(GPUPtr gpu){
    double sum_of_req_bandwidth=0;
    for(auto node_ptr : gpu->vNodeList){
        for(auto task_ptr : node_ptr->vTaskList){
                sum_of_req_bandwidth += double(task_ptr->throughput) * _taskIDtoRequiredBytesPerRequest[task_ptr->id];
        }
    }
    // return in gbits
    return (sum_of_req_bandwidth*8)/ 1024 / 1024/ 1024;
}

// returns false if NOT OK, true if OK
bool NetworkLimitChecker::isBandwidthOK(SimState decision){
       for(auto gpu_ptr : decision.vGPUList ){
           if(!isBandwidthOK(gpu_ptr)) return false;
       }
       return true;
}

bool NetworkLimitChecker::isBandwidthOK(GPUPtr gpu_ptr){
    double gbit_bandwidth = getRequiredBandwidth(gpu_ptr);
    #ifdef SCHED_DEBUG
        std::cout << __func__ << ": gpu_id: "<< gpu_ptr->GPUID <<", gbit_bandwidth: " << gbit_bandwidth << std::endl;
    #endif
    if(gbit_bandwidth > _LIMIT_GBITS) return false;
    return true;
}

int NetworkLimitChecker::adjustBatchSizetoNetBW(TaskPtr &task_ptr, GPUPtr &to_be_inserted_gpu){
    // 1. first get remainin bandwidth
    double remain_bw = std::max(_LIMIT_GBITS - getRequiredBandwidth(to_be_inserted_gpu), 0.0);
    #ifdef sched_debug
    std::cout << __func__ <<": called with remaining bw : " << remain_bw << std::endl;
    #endif
    if(remain_bw == 0.0)
        return EXIT_FAILURE;
    assert(task_ptr->batch_size && task_ptr->throughput); 
    int curr_batch = task_ptr->batch_size;
    float org_trpt = task_ptr->throughput;
    for(int i = curr_batch; i>0;i--){
        double request_required_throughput = ((i/double(curr_batch)) * task_ptr->throughput) * _taskIDtoRequiredBytesPerRequest[task_ptr->id];
        request_required_throughput = (request_required_throughput * 8 / 1024 / 1024/ 1024);
        if(request_required_throughput <= remain_bw){
            task_ptr->batch_size=i;
            task_ptr->throughput = (i/float(curr_batch)) * task_ptr->throughput;
            #ifdef sched_debug
            std::cout << __func__ <<": changed from " << org_trpt << " to " << task_ptr->throughput
            <<std::endl;
            #endif
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}
