#ifndef _PROXY_H__
#define _PROXY_H__
#include <torch/script.h>
#include <mutex> 
#include <deque>
#include <map>
#include <string>

typedef struct _proxy_info {
    /*node_id : 
    * this is used only in frontend, to identify which backend this proxy is related to
    */
    int node_id;
    int dev_id;
    int cap;
    int dedup_num; // especially needed if there are more than one pair of {dev_id, cap}
    int in_fd;
    int out_fd;
    bool isBooted; // indicating whether proxy is booted and ready for connection
    bool isConnected;//indicating whether proxy is connected to backend
    bool isConnectedtoBackend;  // inidicating whether frontned proxy is connected to corresponding backend channel 
    bool isSetup; // indicating whether thread is all set up, CV, mtx, batch list
    bool isSchedulable;
    bool isLoading;
    bool isUnloading;
    float duty_cycle; //  used when we wait for batch, usually setted from SBP algorithm
    int sendSemp; 
    bool isReserved; // flag for indicating whether this is currently a reserved partition or not, used in scheduling
    int partition_num; // index for representing which partition of GPU
    //added field for stiring type info
    std::string type;
    std::mutex *sendMtx; // used for controlling threads which send to proxy server 
    std::mutex *recvMtx; // used for controlling threads which are recieving results from proxy server
    std::mutex *schedMtx; // used for avoiding routing race conditions
    //below are used for scheduling, signaling
    std::map<std::string,int> *isTaskBatching;
    std::map<std::string,int> *isTaskExec;
    std::map<std::string, std::condition_variable*>  *PerTaskBatchCV;
    std::map<std::string, std::condition_variable*>  *PerTaskExecCV;
    std::map<std::string, std::mutex*> *PerTaskBatchMtx;
    std::map<std::string, std::mutex*> *PerTaskExecMtx;
    // latency tracking related 
    std::map<std::string, std::deque<float>*> *PerTaskLatencyVec; // tracks latency of each model, used in scheduling
    std::map<std::string, std::mutex*> *PerTaskLatencyVecMtx;
    std::map<std::string, int> *PerTaskSkipCnt;
    // used when calculating utilizatoin of a partition 
    std::map<std::string, float> *PerTaskTrp; // tracks achieved througput of each model, it it scehduling
    std::vector<std::string>  LoadedModels;

    _proxy_info(){
        sendMtx = new std::mutex();
        recvMtx = new std::mutex();
        schedMtx = new std::mutex();
        isTaskExec = new std::map<std::string,int>();
        isTaskBatching = new std::map<std::string,int>();
        PerTaskBatchCV = new std::map<std::string, std::condition_variable*>();
        PerTaskExecCV = new std::map<std::string, std::condition_variable*>();
        PerTaskBatchMtx =  new std::map<std::string, std::mutex*>();
        PerTaskExecMtx = new std::map<std::string, std::mutex*>(); 
        PerTaskLatencyVec = new std::map<std::string, std::deque<float>*>();
        PerTaskLatencyVecMtx = new std::map<std::string, std::mutex*>();
        PerTaskSkipCnt = new std::map<std::string,int>();
        PerTaskTrp = new std::map<std::string, float>();
        isUnloading=false;   
        isSchedulable=false;
        isLoading=false;
        isConnected=false;
        isConnectedtoBackend=false;
        isReserved=false;
        int node_id=0;
    }

    ~_proxy_info()
    {
        delete sendMtx;
        delete recvMtx;
        delete isTaskExec;
        delete isTaskBatching;
        delete PerTaskBatchCV;
        delete PerTaskExecCV;
        delete PerTaskBatchMtx;
        delete PerTaskExecMtx;
        delete PerTaskLatencyVec;
        delete PerTaskLatencyVecMtx;
        delete PerTaskSkipCnt;
        delete PerTaskTrp;
    }
} proxy_info;



int readInputTypesJSONFile(const char * configJSON, std::unordered_map<int,std::string> &input_map, std::unordered_map<int,std::string> &output_map);
int connectGPUProxyIn(int gpuid, int thredcap, int dedup);
int connectGPUProxyOut(int gpuid, int threadcap, int dedup);
int sendRequest(int in_fd, int rid, int jobid,torch::Tensor &input_tensor); 
int sendRequest(int in_fd, int rid, int jobid, std::vector<int64_t> &dims); 
torch::Tensor recvResult(proxy_info *pPInfo, int batch_size, int req_id);
std::string proxy_info_to_string(proxy_info* pPInfo);
// send requests to backend
int sendRequestToBackend(int input_socket_fd,int rid, int jid, torch::Tensor &input_tensor);


#else
#endif
