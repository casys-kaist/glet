#include <stdio.h>
#include <iostream>
#include <assert.h>
#include "profile.h"

ReqProfile::ReqProfile(int req_id, int job_id){
    _id = req_id;
    _jid=job_id;
}

ReqProfile::~ReqProfile(){
}

void ReqProfile::setInputStart(uint64_t time){
    _input__start=time;
}

void ReqProfile::setInputEnd(uint64_t time){
    _input_end = time;
}

void ReqProfile::setCPUCompStart(uint64_t time){
    _cpu_comp_start = time;
}

void ReqProfile::setGPUStart(uint64_t time){
    //added for debugging this functiion
    #ifdef PROFILE
    std::cout << __func__ << ": called for req id: " << _id << std::endl;
    #endif
    _gpu_start = time;
}

void ReqProfile::setGPUEnd(uint64_t time){
    #ifdef PROFILE
    std::cout << __func__ << ": called for req id: " << _id << std::endl;
    #endif
    _gpu_end = time;
}

void ReqProfile::setCPUPostEnd(uint64_t time){
    _cpu_post_end=time;
}

void ReqProfile::setOutputStart(uint64_t time){
    _output_start=time;
}

void ReqProfile::setOutputEnd(uint64_t time){
    _output_end = time;
}

void ReqProfile::setBatchSize(int time){
    _batch_size = time;
} 

void ReqProfile::printTimes(){
    printf("[PROFILE] printing execution time info of req_id: %d \n",_id);
    assert(_input__start);
    assert(_gpu_end);
    assert(_gpu_start);
    assert(_output_end);
    double total = double(_output_end - _input__start)/1000000;
    double input_cpu = double(_input_end - _input__start)/1000000;
    double input_delay = double(_cpu_comp_start - _input_end)/1000000;
    double preprocess = double(_gpu_start - _cpu_comp_start)/1000000;
    double total_gpu = double(_gpu_end - _gpu_start)/1000000;
    double postprocess = double(_cpu_post_end - _gpu_end)/1000000;
    double _output_delay=double(_output_start - _cpu_post_end)/1000000;
    double _output_cpu = double(_output_end - _output_start)/1000000;
    double total_cpu = total-total_gpu;
    printf("[PROFILE] %d id: %d batch_size: %d input_cpu: %lf input_delay: %lf preprocess: %lf total_gpu: %lf postprocess: %lf _output_delay: %lf _output_cpu: %lf total_cpu: %lf \n" ,\
                    _jid,_id,_batch_size, \
                    input_cpu,\
                    input_delay,\
                    preprocess,\
                    total_gpu,\
                    postprocess,\
                    _output_delay,\
                    _output_cpu,\
                    total_cpu);
}
