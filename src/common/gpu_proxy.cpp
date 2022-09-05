
#include <fstream>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctime>
#include <cmath>
#include <glog/logging.h>
#include <time.h>
#include <cstring>
#include <assert.h>
#include <condition_variable>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>   
#include <mutex>  
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "gpu_proxy.h" 
#include "batched_request.h"
#include "socket.h"
#include "common_utils.h"
#include "global_scheduler.h"
#include "json/json.h"



int readInputTypesJSONFile(const char * configJSON, std::unordered_map<int,std::string> &input_map, std::unordered_map<int,std::string> &output_map){
    std::cout << __func__ << ": reading JSON File: " << configJSON
    <<std::endl;
    Json::Value root;
    std::ifstream ifs;
    ifs.open(configJSON);
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
        input_map[id]=root["Models"][i]["input_type"].asString();
        output_map[id]=root["Models"][i]["output_type"].asString();  
        std::cout << __func__ <<": setted up " << id << "'s input_type as " << input_map[id]
        << " and output type as " << output_map[id]
        <<std::endl;
    }
    ifs.close();
    return EXIT_SUCCESS;
   
}


int connectGPUProxyOut(int gpuid, int threadcap, int dedup){
    struct sockaddr_un d;
    int d_fd;
    memset(&d, 0, sizeof(struct sockaddr_un));
    d_fd=socket(AF_UNIX, SOCK_STREAM, 0); 
    if (d_fd == -1) {
        printf("SOCKET ERROR = %s\n", strerror(errno));
        exit(1);
    }
    // disable nagle's TCP delay algorithm
    int nvalue=1;
    setsockopt(d_fd,IPPROTO_TCP,TCP_NODELAY, &nvalue, sizeof(int));
    d.sun_family=AF_UNIX;
    char buffer[50];
    char idbuffer[20];
    strcpy(buffer,"/tmp/gpusock_output_");
    snprintf(idbuffer,sizeof(idbuffer),"%d_%d_%d",gpuid,threadcap, dedup);
    strcat(buffer,idbuffer);
#ifdef DEBUG
    printf("[PROXY] connecting to %s \n", buffer);
#endif
    strcpy(d.sun_path, buffer);
    int r2=connect(d_fd, (struct sockaddr*)&d, sizeof(d));
    if (r2 == -1) {
        printf("SOCKET ERROR = %s\n", strerror(errno));
        exit(1);
    }
#ifdef DEBUG
      printf("[PROXY] Connected to proxy server of gpu%d with socket number %d\n",gpuid, d_fd);
#endif
    struct timeval tv;
 //   tv.tv_sec = timeout_in_seconds;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(d_fd, SOL_SOCKET,SO_RCVTIMEO, (const char*)&tv, sizeof tv);
   return d_fd;
}

int connectGPUProxyIn(int gpuid, int threadcap, int dedup){
    struct sockaddr_un d;
    int d_fd;
    memset(&d, 0, sizeof(struct sockaddr_un));
    d_fd=socket(AF_UNIX, SOCK_STREAM, 0); 
    if (d_fd == -1) {
        printf("SOCKET ERROR = %s\n", strerror(errno));
        exit(1);
    }
    int nvalue=1;
    // disable nagle's TCP delay algorithm
    setsockopt(d_fd,IPPROTO_TCP,TCP_NODELAY, &nvalue, sizeof(int));


    d.sun_family=AF_UNIX;
    char buffer[50];
    char idbuffer[20];
    strcpy(buffer,"/tmp/gpusock_input_");
    snprintf(idbuffer,sizeof(idbuffer),"%d_%d_%d",gpuid,threadcap,dedup);
    strcat(buffer,idbuffer);
#ifdef DEBUG
    printf("[PROXY] connecting to %s \n", buffer);
#endif
    strcpy(d.sun_path, buffer);
    int r2=connect(d_fd, (struct sockaddr*)&d, sizeof(d));
    if (r2 == -1) {
        printf("SOCKET ERROR = %s\n", strerror(errno));
        exit(1);
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(d_fd, SOL_SOCKET,SO_RCVTIMEO, (const char*)&tv, sizeof tv);
#ifdef DEBUG
      printf("[PROXY] Connected to proxy server of gpu%d with socket number %d\n",gpuid, d_fd);
#endif
    return d_fd;

}


int sendRequest(int in_fd, int rid, int jobid, std::vector<int64_t> &dims){
    // int sock_elts=1;
    int dim = dims.size();
#ifdef DEBUG
    std::cout << "[sendRequest] in_fd: " << in_fd << std::endl;
    std::cout << "[sendRequest] input has" <<  dims.size() <<" dimensions" <<std::endl;
    for(unsigned int i=0; i<dims.size(); i++){
        printf("[sendRequest] dim%d: length: %lu \n",i, dims[i]);
    }
    
#endif 
    int ret=write(in_fd, (void *)&dim,sizeof(int));
    int r;
    r=write(in_fd, (void*)&jobid,sizeof(int));
    if(ret<=0){
        perror("write");
        return EXIT_FAILURE;
    }
    for(unsigned int i=0; i<dim; i++){
        int len = dims[i];
        write(in_fd, (void *)&len,sizeof(int));
    }
     //send request ID 
    r=write(in_fd, (void*)&rid,sizeof(int));
    if (r == -1){
         printf("SEND ERROR - RID: %d\n", rid);
         printf("SOCKET ERROR = %s\n", strerror(errno));
         return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


int sendRequest(int in_fd, int rid, int jobid, torch::Tensor &input_tensor){
    int sock_elts=1;
    int dim =input_tensor.sizes().size();
#ifdef DEBUG
    std::cout << "[sendRequest] in_fd: " << in_fd << std::endl;
    std::cout << "[sendRequest] input has" <<  input_tensor.sizes().size() <<" dimensions" <<std::endl;
    for(unsigned int i=0; i<input_tensor.sizes().size(); i++){
        printf("[sendRequest] dim%d: length: %lu \n", i,input_tensor.size(i));
    }
#endif 
    int ret=write(in_fd, (void *)&dim,sizeof(int));
    int r;
    r=write(in_fd, (void*)&jobid,sizeof(int));
    if(ret<=0){
        perror("write");
        return EXIT_FAILURE;
    }
    for(unsigned int i=0; i<input_tensor.sizes().size(); i++){
        int len = input_tensor.size(i);
        ret=write(in_fd, (void *)&len,sizeof(int));
        sock_elts*=len;
    }
     //send request ID 
    r=write(in_fd, (void*)&rid,sizeof(int));
    ret=write(in_fd, (void *)&sock_elts,sizeof(int));
    if (input_tensor.dtype() == torch::kFloat32){
        float *raw_data=(float*)input_tensor.data_ptr();  
        r=socket_send(in_fd, (char*)raw_data, sock_elts*sizeof(float), false);
    }
    else if(input_tensor.dtype() == torch::kInt64){
        int64_t *raw_data=(int64_t*)input_tensor.data_ptr();  
        r=socket_send(in_fd, (char*)raw_data, sock_elts*sizeof(int64_t), false);
    }
    if (r == -1){
         printf("SEND ERROR - RID: %d\n", rid);
         printf("SOCKET ERROR = %s\n", strerror(errno));
         return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int sendRequestToBackend(int in_fd, int rid, int jobid, torch::Tensor &input_tensor){
     int r;
    // send job id 
    r=write(in_fd, (void*)&jobid,sizeof(int));
     //send request ID 
    r=write(in_fd, (void*)&rid,sizeof(int));
    int batch_size=input_tensor.size(0);
    r=write(in_fd, (void *)&batch_size,sizeof(int));
    if (r == -1){
         printf("SEND ERROR - RID: %d\n", rid);
         printf("SOCKET ERROR = %s\n", strerror(errno));
    }
    #ifndef NO_NET
    int sock_elts=1;
    for(unsigned int i=0; i<input_tensor.sizes().size(); i++){
        int len = input_tensor.size(i);
        sock_elts*=len;
    }
   if (input_tensor.dtype() == torch::kFloat32){
        float *raw_data=(float*)input_tensor.data_ptr();  
        r=socket_send(in_fd, (char*)raw_data, sock_elts*sizeof(float), false);
    }
    else if(input_tensor.dtype() == torch::kInt64){
        int64_t *raw_data=(int64_t*)input_tensor.data_ptr();  
        r=socket_send(in_fd, (char*)raw_data, sock_elts*sizeof(int64_t), false);
    }
    #else
    #endif
    return r;
}

torch::Tensor recvResult(proxy_info *pPInfo, int batch_size, int req_id){
    int ret;
    int out_fd = pPInfo->out_fd;
    int TCP_NO_DELAY_VAL=1;
    int TCP_QUICK_VAL=0;
#ifdef DEBUG
    printf("[SERVER_PROXY] Waiting for output \n");
#endif 
    setsockopt(out_fd,IPPROTO_TCP,TCP_NODELAY, (void*)&TCP_NO_DELAY_VAL, sizeof(int));
    torch::TensorOptions options(torch::kFloat32);
    std::vector<int64_t> dims;
    int dim;
    if(setsockopt(out_fd, IPPROTO_TCP, TCP_QUICKACK, &TCP_QUICK_VAL, sizeof(int))){
        perror("recvResult setsocketopt");
    }
    ret=read(out_fd, &dim, sizeof(int));
    if(setsockopt(out_fd, IPPROTO_TCP, TCP_QUICKACK, &TCP_QUICK_VAL, sizeof(int))){
        perror("recvResult setsocketopt");
    }
    if(ret == -1){      
        printf("[PROXY_SERVER] READ ERROR! on %s \n", proxy_info_to_string(pPInfo).c_str());
        printf("ERROR: was waiting for req_id: %d \n", req_id);
        printf("SOCKET ERROR = %s\n", strerror(errno));        
    }
    int size;
    int len=1;
    if(setsockopt(out_fd, IPPROTO_TCP, TCP_QUICKACK, &TCP_QUICK_VAL, sizeof(int))){
        perror("recvResult setsocketopt");
    }
    for(int j =0; j<dim;j++){
            ret=read(out_fd, &size, sizeof(int));
            if(setsockopt(out_fd, IPPROTO_TCP, TCP_QUICKACK, &TCP_QUICK_VAL, sizeof(int))){
                perror("recvResult setsocketopt");
            }
            len*=size;
            dims.push_back(size);
    }
    assert(dims[0] == batch_size);
#ifndef NO_NET
	float *tmp = (float*)malloc(len*sizeof(float));
#ifdef DEBUG
    printf("[SERVER_PROXY] Waiting for raw data \n");
#endif 

    if ((ret=socket_receive(out_fd, (char*)tmp,len*sizeof(float), false)) <=0){ 
            printf("ERROR in receiving output data\n");
            printf("ERROR: was waiting for req_id: %d \n", req_id);
            printf("SOCKET ERROR = %s\n", strerror(errno));
    }  
    if(setsockopt(out_fd, IPPROTO_TCP, TCP_QUICKACK, &TCP_QUICK_VAL, sizeof(int))){
        perror("recvResult setsocketopt");
    }
#ifdef DEBUG
    printf("[SERVER_PROXY] Got output \n");
#endif 
    torch::Tensor output = convert_rawdata_to_tensor(tmp,dims,options);
#else
    // create dummy tensor
    torch::Tensor output = torch::ones(batch_size);
#endif    
	return output;
}

std::string proxy_info_to_string(proxy_info* pPInfo){
    std::string ret_string="["+ std::to_string(pPInfo->dev_id) + "," + std::to_string(pPInfo->cap) + "," + std::to_string(pPInfo->dedup_num)+"]";
    return ret_string;

}
