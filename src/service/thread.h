#ifndef _THREAD_H_
#define _THREAD_H_
#include "sys_monitor.h"

#include <pthread.h>
#include <stdio.h>
#include <vector>
#include <deque>
#include <mutex>

#include "socket.h"
#include "request.h"
#include "batched_request.h"
#include "gpu_proxy.h"

#define NUM_CORE 20 // the number of total logical cores within the system
#define DROP_ID -1  //ID= -1 , indicates this task was dropped, used in drop_thread.cpp


pthread_t initMonitorThread();
void *initMonitor(void* args);

pthread_t initGPUMonitorThread();
void *initGPUMonitor(void* args);

pthread_t initUtilMonitorThread();
void *initGPUMonitor(void* args);

pthread_t initRequestThread(int sock);
//pthered_t request_med_thread_init(int sock, int gpu_id);
void *handleRequest(void* sock);

//pthread_t initExecutionThread(batched_request *req);
void handleExecution(std::shared_ptr<batched_request> input_info,std::string strReqName,std::shared_ptr<TaskSpec> pTask);

pthread_t initProxyThread(proxy_info* pPInfo);
void* initProxy(void *args);

pthread_t initOutputThread(proxy_info *pPInfo);
void *initOutput(void *args);

pthread_t initServerThread(int numGPU);
void* initServer(void* numGPU);


void addtoAppQueue(std::shared_ptr<AppInstance> appinstance, bool dropped);
torch::Tensor getRandomTensor(std::vector<int64_t> dims);


pthread_t initSendResultsThread();
void* initSend(void *args);

void configureAppSpec(std::string config_json,std::string resource_dir, SysMonitor &state);


pthread_t initAppThread(AppSpec *App); // app-wise threads 
void* initApp(void *args);

pthread_t initDropThread(double *DropRatio); // will check each request queue periodically and drop requests if they are over certain slakcs
void *initDrop(void *args);

void sendBatchedResults(std::shared_ptr<batched_request> &brp, std::string reqname); // defined in proxy_thread.cpp 
void addtoModelQueue(std::string StrName, torch::Tensor &input, int cgid,std::shared_ptr<AppInstance> task); // defined in server_thread.cpp
int bootProxys(SysMonitor &SysState); // defined int proxy_ctrl_thread.cpp

// receives data and sends it to proxy
pthread_t initBackendDataThread(int socket);
void* initBackendData(void *args);

// reveives control/scheduling result through socket
pthread_t initBackendControlThread(int socket);
void* initBackendControl(void *args);

#else
#endif // #define _THREAD_H_
