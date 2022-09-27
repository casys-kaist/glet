#include <torch/script.h> // One-stop header.
#include <cuda_runtime.h>

#include <iostream>
#include <string>
#include <memory>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <queue>
#include <condition_variable>

#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#include "socket.h"
#include "common_utils.h"

#include "app_instance.h"
#include "input.h"
#include "thread.h"
#include "request.h"
#include "batched_request.h"
#include "custom_ops.h"

#define CHECKING_INTERVAL 0.5
namespace po = boost::program_options; 
using namespace cv;

extern FILE* pLogFile;

////system state related variables
extern SysMonitor ServerState;
bool READY_TO_EXEC;


double rand_mean;
extern GlobalScheduler gScheduler;

unsigned int TaskID=0;
unsigned int ReqID=0;

//  synch related variables       
std::map<std::string, std::mutex*> ReqMtxVec; // used when popping or pushing
std::mutex ReqCntMtx; // mutex associated with request id counter value
std::mutex TaskCntMtx; // mutex associated with task id counter value

std::mutex readyMtx;// "         ready flag

std::mutex createMtx; // lock used when creating Per task Mtxs
//std::map<std::string, mutex*> PerTaskArrivUpdateMtx; 

std::condition_variable readyCV;
std::mutex readyCVMtx; // the global_mutex used with the conditional variable
int ready=0; // works as a flag and semaphore

//flag for start sending requests

//condition variables and vector needed for batching threads
std::vector<std::condition_variable*> PerDeviceBatchCV; 
std::vector<std::mutex*> PerDeviceBatchMtx; 
std::vector<bool> PerDeviceIsSetup;

pthread_t initServerThread(int numGPU){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	READY_TO_EXEC=false; 
	if (pthread_create(&tid, &attr, initServer, (void *)(intptr_t)numGPU) != 0)
		LOG(ERROR) << "Failed to create a request handler thread.\n";
	while(!READY_TO_EXEC){usleep(1*1000);}
	pthread_detach(tid);
	return tid;
}
void* initServer(void* numGPU){
	READY_TO_EXEC=true;
	while (1){
		while(!ready){
			usleep(CHECKING_INTERVAL * 1000);
			//monitors ready queues and resets 'ready' if there are left overs in the queue
			//for(std::map<std::string, std::queue<std::shared_ptr<request>>>::iterator it=ServerState.ReqListHashTable.begin(); 
			//		it != ServerState.ReqListHashTable.end(); it++ ){
			for(auto it : *ServerState.getReqListHashTable()){
				if(it.second.size()){
					readyMtx.lock();
					ready=it.second.size();
					readyMtx.unlock();
				}
			}

		}
		if(!ready) continue;
		gScheduler.doMPSScheduling(&ServerState);
		if (!ready){ // if still not ready, it means there are really no tasks in all queues
#ifdef DEBUG 
			printf("[SERV]server is turning idle! \n");
#endif
		}
	} // infinite loop
	return (void*)1;
}

pthread_t initSendResultsThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initSend, NULL) != 0)
		LOG(ERROR) << "Failed to create a send results thread.\n";
	return tid;
}
void* initSend(void *args){
#ifdef MUTRACE
	static unsigned int cnt=2000;
#endif 
	while(true){
		std::shared_ptr<request> tReq;
		if(ServerState.getCompQueue()->try_dequeue(tReq)){
			tReq->setendSend(getCurNs());
			tReq->writeToLog(pLogFile);
#ifdef MUTRACE
			if(cnt !=0){
				cnt = cnt -1 ;
				printf("[MUTRACE] CNT: %u \n",cnt);
			}
			else
				exit(0);
#endif 
		}
		else
			usleep(0.5*1000);
	}
	return NULL;
}
void addtoModelQueue(std::string StrName, torch::Tensor &input, int cgid,std::shared_ptr<AppInstance> task){
	if(!task->isDropped() && ServerState.isTrackInterval()) ServerState.incPerModelCnt(StrName);
	ReqCntMtx.lock(); // unlock counter
	std::shared_ptr<request> pReq = std::make_shared<request>(++ReqID, 0, 1); // * second parameter of request used to be SockNum, third is batchsize
	ReqCntMtx.unlock(); // unlock counter
	pReq->setStart(getCurNs());
	pReq->setTaskID(task->getTaskID());
	pReq->setupAppInstance(task);
	pReq->setCGID(cgid);
	pReq->pushInputData(input);
	pReq->setReqName(StrName.c_str());
	ReqMtxVec[StrName]->lock();
	ServerState.getReqListHashTable()->operator[](StrName).push(pReq);
	ReqMtxVec[StrName]->unlock(); 
	ready++; // set the flag!
	readyCV.notify_one();
}
