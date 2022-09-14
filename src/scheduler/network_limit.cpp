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

int NetworkLimitChecker::adjustBatchSizetoNetBW(TaskPtr &task_ptr, GPUPtr &to_be_inserted_gpu){
 
}
