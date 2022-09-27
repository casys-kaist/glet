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
#include "torch_utils.h"
#include "common_utils.h" //printTimeStamp moved to her
#include "gpu_proxy.h"

#include "thread.h"
#include "request.h"
#include "batched_request.h"
#include "load_balancer.h"
#include "sys_monitor.h"
#include "global_scheduler.h"

#ifdef DEV_LOG
//added to print proxy execution LOG
std::map<proxy_info*,FILE*> devlogTable;

extern std::string str_START;
#endif 


#ifdef FIXED_CORES
//fixed cores for each device 
int gpucores[4][3]={{2,3,4},{7,8,9},{12,13,14},{17,18,19}};
#endif 
//synch related extern variables
// variables realted to main.cpp 
extern GlobalScheduler gScheduler;
extern SysMonitor ServerState;
extern bool finishedMPSProxyInit;
extern  std::map<std::string, std::mutex*> ReqMtxVec;

std::mutex initMtx;
std::map<std::string, std::mutex*> cmpTableMtx;  

//condition variables and vector needed for batching threads
std::map <proxy_info*,std::condition_variable*> PerProxyBatchCV;
std::map<proxy_info*, std::mutex*> PerProxyBatchMtx;

// added for output threads
std::map<proxy_info*,std::condition_variable*> PerProxyOutputCV;
std::map<proxy_info*, std::mutex*> PerProxyOutputMtx;
std::map<proxy_info*, std::deque<std::shared_ptr<batched_request>>* > PerProxyOutputQueue;

// following are synch variables for tagging IDs
int batchTaskID=0;
std::mutex batchIDMtx;

#ifdef DEBUG
std::mutex debug_mtx;
unsigned int sent_tasks=0;
unsigned int completed_tasks=0;
unsigned int will_send_tasks=0;
unsigned int exec_tasks=0;
#endif


void* sendBatchedRequest(std::shared_ptr<batched_request> &input_info, std::string strReqName, std::shared_ptr<TaskSpec> pTask, proxy_info* pPInfo);
void ProxyBasicSetup(proxy_info* pPInfo);



float getMaxFloat(const std::vector<float> vec){
	//assume every value is >0
	float max_val =0;
	for(auto val : vec){
		if(val > max_val) max_val=val;
	}
	assert(max_val !=0);
	return max_val;

}
void sendBatchedResults(std::shared_ptr<batched_request> &brp, std::string model_name, proxy_info* pPInfo){

	int numofReq= brp->getNTask();
	if(pPInfo->PerTaskTrp->find(model_name) == pPInfo->PerTaskTrp->end()){
		pPInfo->PerTaskTrp->operator[](model_name)=numofReq;   
	}
	else{
		pPInfo->PerTaskTrp->operator[](model_name)+=numofReq;
	}  
	std::vector<float> latency_vec;
	for(int i =0; i < numofReq; i++){
		std::shared_ptr<request> req = brp->getRequests()[i];
		req->setendCmpq(getCurNs());
		if(ServerState.isTrackTrpt()) {
			// only count good puts
			if(req->getLatency() < gScheduler.getSLO(model_name))
				ServerState.incPerModelFinCnt(model_name);
		}

		ServerState.getCompQueue()->enqueue(brp->getRequests()[i]);
		if(req->isAppFinished()){
#ifdef DEBUG
			printf("[sendBatched] taskid %d finished \n", req->getApp()->getTaskID());
#endif 
			req->getApp()->setEndExec(getCurNs());
			addtoAppQueue(req->getApp(), /*dropped=*/false);
		}
		else{
			AppSpec temp_app_spec = ServerState.getAppSpecVec()->operator[](req->getApp()->getAppSpecID());
			std::vector<int> dst_ids = temp_app_spec.getNextDsts(req->getCGID());
			for(std::vector<int>::iterator it = dst_ids.begin(); it != dst_ids.end(); it++){
				// we just skip if the destination is just an output. If there are additional operations for output this MUST be changed
				if(temp_app_spec.isOutput(*it))
					continue;
				else{
					std::string modelname = temp_app_spec.getModelName(*it);
#ifdef DEBUG
					printf("[sendBatched] taskid %d not finished, sended to computation id %d \n", req->getApp()->getTaskID(), *it);
#endif
					torch::Tensor temp = req->getApp()->getOutputTensor(req->getCGID());
					addtoModelQueue(modelname,temp,*it,req->getApp()); 
				}
			}
		}       
	}
	brp->getRequests().erase(brp->getRequests().begin(), brp->getRequests().begin() + numofReq);

}



pthread_t initProxyThread(proxy_info* pPInfo){
	std::mutex randnumMtx; // use lock in order to ensure unique numberint deviceID = gpu_id;
	//batched_tasks=0;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future for now set to max * 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initProxy, (void *)pPInfo) != 0)
		LOG(ERROR) << "Failed to create a batching thread.\n";
	return tid;

}

// check list, batch requests as much as possible  and call exeuction handler
void*  initProxy(void *args){
	proxy_info* pPInfo = (proxy_info*)args;
	int DeviceID = pPInfo->dev_id;

	initMtx.lock();

#ifdef FIXED_CORES
	cpu_set_t cpuset;
	pthread_t thread=pthread_self();
	for (int i =0; i<3 ; i++){ // we have three cores per each GPU
		CPU_SET(gpucores[DeviceID][i], &cpuset);
		printf("core %d set for PROXY[%d,%d,%d] \n",gpucores[DeviceID][i],pPInfo->dev_id, pPInfo->cap, pPInfo->dedup_num);
	}
	if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset)!=0){
		printf("Error in setting affinity for cpu thread\n");
		exit(1);
	}

#endif



#ifdef DEBUG
	printf("PerProxyIsSetup[DeviceID: %d,cap: %d]: %s\n",DeviceID,pPInfo->cap,pPInfo->isSetup ? "true":"false");
#endif 
	if (!pPInfo->isSetup){ // in order to avoid duplicate setup we check first!
		ProxyBasicSetup(pPInfo);//set up server
		pPInfo->isSetup=true;
	}
	initMtx.unlock();
	finishedMPSProxyInit=true;
	while (1){
		//wait for condition variable to be called        
		std::deque<std::shared_ptr<TaskSpec>> *pBatchList = ServerState.getBatchQueueofProxy(pPInfo);
		std::unique_lock<std::mutex> lk(*PerProxyBatchMtx[pPInfo]); 
		PerProxyBatchCV[pPInfo]->wait(lk, [&pBatchList]{return pBatchList->size();});
		std::shared_ptr<TaskSpec> task =  pBatchList->front();
		std::string StrReqName = task->ReqName;

		//std::queue<std::shared_ptr<request>>* pReqList=&ServerState.ReqListHashTable[StrReqName];
		std::queue<std::shared_ptr<request>>* pReqList=ServerState.getRequestQueueofNet(StrReqName);

#ifdef DEBUG
		printf("[BATCH][%d,%d,%d] batch list size: %lu, front task : %s\n",pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num,pBatchList->size(),StrReqName.c_str());
		uint64_t startBatchThread = getCurNs();
#endif 
		if (pReqList->empty() || !pPInfo->isSchedulable) { // possible if previous thread has already popped most of the requests, proxy is disconnected, not booted
#ifdef DEBUG
			printf("[BATCH][%d,%d,%d] skipping batching, thus exiting \n",pPInfo->dev_id, pPInfo->cap, pPInfo->dedup_num);       
#endif 
			pBatchList->pop_front();
			lk.unlock();
			PerProxyBatchCV[pPInfo]->notify_one();
			continue;
		}
		pBatchList->pop_front();
		lk.unlock();
		PerProxyBatchCV[pPInfo]->notify_one();
		uint64_t start_batch, end_batch, start_exec, end_exec;
		std::unique_lock<std::mutex> lk2(*(pPInfo->PerTaskBatchMtx->operator[](StrReqName)));
		start_batch = getCurNs();
		pPInfo->PerTaskBatchCV->operator[](StrReqName)->wait(lk2,[&pPInfo,StrReqName] {return !pPInfo->isTaskBatching->operator[](StrReqName);} );
		pPInfo->isTaskBatching->operator[](StrReqName)++;
		lk2.unlock();
		end_batch = getCurNs();
		std::shared_ptr<batched_request> pBatchedReq= std::make_shared<batched_request>(); // the local batched_request;
		pBatchedReq->setStrName(StrReqName);
		batchIDMtx.lock();
		pBatchedReq->setBatchID(batchTaskID++);
		batchIDMtx.unlock();
#ifdef DEBUG
		printf("[BATCH][%d,%d,%d][%d] waiting_batch took %lf ms  \n",pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num,pBatchedReq->getBatchID(),\
				double(end_batch-start_batch)/1000000);

#endif
		int maxbatch;
		float maxdelay;      
		pBatchedReq->setMaxBatch(task->BatchSize);
		maxdelay = gScheduler.getMaxDelay(pPInfo);

#ifdef DEBUG
		printf("[BATCH[[%d,%d,%d][%d] task: %s found %lu requests in queue \n",pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num,pBatchedReq->getBatchID(),StrReqName.c_str(),pReqList->size());
		printf("[BATCH][%d,%d,%d][%d] task: %s will wait for %lf ms \n",pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num,pBatchedReq->getBatchID(),StrReqName.c_str(),maxdelay);
#endif
		uint64_t batch_start=getCurNs();
		uint64_t temp;
		double elapsed = 0;
		const float WAIT_MILLI_SEC=0.5;
		const int QUEUE_THRESHOLD =1;
		while((pBatchedReq->getBatchNum() < pBatchedReq->getMaxBatch()))
		{
			elapsed=double(getCurNs()-batch_start)/1000000;
			if(maxdelay!=0 && elapsed >= maxdelay) break;
			if(pReqList->empty()){
				// if delay is setted to 0, this usually means to eager batch, thus just break and go
				if(maxdelay == 0.0){
					break;
				}

				// cut off cases that will exceed max delay 
				if(elapsed >= (maxdelay - WAIT_MILLI_SEC)) break;
				// wait, since there might be tasks in the future while this thread is waiting
				usleep(WAIT_MILLI_SEC*1000);        
				continue;
			}

			//added call to load balancer, checkLoad calls load balancer
			bool load_balance_result = gScheduler.checkLoad(gScheduler.getModelID(StrReqName),pPInfo);
			bool is_queue_full = (pReqList->size() > QUEUE_THRESHOLD ); 
			if(!load_balance_result &&  !is_queue_full){
				usleep(WAIT_MILLI_SEC*1000);
				continue;
			}
#ifdef LB_DEBUG
			cout << "task: " << StrReqName<< "load_balance_result: " << load_balance_result << ", is_queue_full: " << is_queue_full
				<< endl;
#endif

			ReqMtxVec[StrReqName]->lock();
			if (pReqList->empty()){ // just in case another thread popped a request right before acquiring lock 
				ReqMtxVec[StrReqName]->unlock();
				continue;
			}
			std::shared_ptr<request> r= pReqList->front();
			pReqList->pop();
			ReqMtxVec[StrReqName]->unlock();

			r->setDeviceID(DeviceID);
			r->setendReq(getCurNs());
			pBatchedReq->addRequestInfo(r);
		}
		uint64_t batch_end = getCurNs();

#ifdef DEBUG
		printf("[BATCH][%d,%d,%d][%d] task: %s batch size: %d maxdelay: %lf ms batching overhead: %lf ms \n", pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num, \
				pBatchedReq->getBatchID(),StrReqName.c_str(),pBatchedReq->getNTask(),\
				maxdelay, double(batch_end-batch_start)/1000000);
#endif
		if(pBatchedReq->getNTask() ==0){
			lk2.lock();
			pPInfo->isTaskBatching->operator[](StrReqName)--;           
			lk2.unlock();
			pPInfo->PerTaskBatchCV->operator[](StrReqName)->notify_one();        
#ifdef DEBUG
			printf("[BATCH][%d,%d,%d] no requests found for task %s in the end, thus exiting \n",pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num,StrReqName.c_str());  
#endif 

		}
		if(pBatchedReq->getNTask() !=0){//check whether OK to execute
			for (int id=0; id< pBatchedReq->getNTask();id++){
				pBatchedReq->getRequests()[id]->setendBatch(getCurNs());
				pBatchedReq->getRequests()[id]->setBatchID(pBatchedReq->getBatchID());                
			}
			std::unique_lock<std::mutex> lk3(*(pPInfo->PerTaskExecMtx->operator[](StrReqName)));
#ifdef DEBUG
			debug_mtx.lock();
			will_send_tasks+=pBatchedReq->getBatchNum();    
			printf("[DEBUG] will_send_tasks: %u \n", will_send_tasks);
			debug_mtx.unlock();    
			uint64_t wait_exec_start, wait_exec_end;
			printf("[%s][BATCH][%d,%d,%d][%d]  execCV started \n", timeStamp(), pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,pBatchedReq->getBatchID());

			wait_exec_start=getCurNs();
#endif

			pPInfo->PerTaskExecCV->operator[](StrReqName)->wait(lk3,[&pPInfo,StrReqName] {return !pPInfo->isTaskExec->operator[](StrReqName);} );
			pPInfo->isTaskExec->operator[](StrReqName)++;
			lk3.unlock();
			lk2.lock();
			pPInfo->isTaskBatching->operator[](StrReqName)--;           
			lk2.unlock();
			pPInfo->PerTaskBatchCV->operator[](StrReqName)->notify_one();        
#ifdef DEBUG
			printf("[%s][BATCH][%d,%d,%d][%d] execCV finished \n", timeStamp(), pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,pBatchedReq->getBatchID());
			wait_exec_end = getCurNs();
			printf("[BATCH][%d,%d,%d][%d] task: %s exec_waiting_time: %lf ms \n", pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,\
					pBatchedReq->getBatchID(),StrReqName.c_str(),\
					double(wait_exec_end-wait_exec_start)/1000000);
			debug_mtx.lock();
			exec_tasks+=pBatchedReq->getBatchNum();    
			printf("[DEBUG] exec_tasks: %u \n", exec_tasks);
			debug_mtx.unlock();    
#endif 

			end_exec=getCurNs();
			uint64_t start_send=getCurNs();
			sendBatchedRequest(pBatchedReq,StrReqName,task,pPInfo);
			uint64_t end_send=getCurNs();
#ifdef DEBUG
			printf("[BATCH][%d,%d,%d][%d] task: %s sending overhead: %lf ms \n", pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,\
					pBatchedReq->getBatchID(),StrReqName.c_str(),\
					double(end_send-start_send)/1000000);
			printf("[BATCH][%d,%d,%d][%d] task: %s turnaround time: %lf ms \n", pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num, \
					pBatchedReq->getBatchID(),StrReqName.c_str(),\
					double(end_send - end_batch)/1000000 );
#endif 
		}
		pBatchedReq.reset();
	}// infinite loop, for PerProxyBatchCV
}//batch handler function

void ProxyBasicSetup(proxy_info* pPInfo){
	std::map<std::string,torch::jit::script::Module> *tlnets = new std::map<std::string, torch::jit::script::Module>(); 
	int deviceID = pPInfo->dev_id;
	PerProxyBatchMtx[pPInfo]=new std::mutex();

	std::condition_variable *pCV = new std::condition_variable();
	PerProxyBatchCV[pPInfo]=pCV;
	pCV = new std::condition_variable();
	PerProxyOutputCV[pPInfo]=pCV;
	PerProxyOutputMtx[pPInfo]= new std::mutex();
	PerProxyOutputQueue[pPInfo]=new std::deque<std::shared_ptr<batched_request>>;

	ServerState.setupProxy(pPInfo);
	std::vector<std::string> *pVecNetNames=gScheduler.getNetNames();  

	for (std::vector<std::string>::iterator it = pVecNetNames->begin(); it != pVecNetNames->end(); it++){
		std::string req_name = *it;
		pPInfo->isTaskExec->operator[](req_name)=0;
		pPInfo->isTaskBatching->operator[](req_name)=0;
		pPInfo->PerTaskBatchCV->operator[](req_name) = new std::condition_variable();
		pPInfo->PerTaskExecCV->operator[](req_name)= new std::condition_variable();
		pPInfo->PerTaskBatchMtx->operator[](req_name) = new std::mutex();
		pPInfo->PerTaskExecMtx->operator[](req_name) = new std::mutex();
	}
	pPInfo->isSetup=true;
#ifdef DEV_LOG
	std::string proxyname = "proxy_log";
	proxyname = proxyname + to_string(pPInfo->dev_id)+to_string(pPInfo->cap)+to_string(pPInfo->dedup_num);
	proxyname = proxyname + ".txt";
	devlogTable[pPInfo]=fopen(proxyname.c_str(), "w");
#endif 

	// Load model weights
	std::string dev;
	std::vector<std::string> *pVec=gScheduler.getNetNames();  

	for(std::vector<std::string>::iterator it = pVec->begin(); it != pVec->end(); it++){
		std::string NetName=*it;

		// init net hashes 
		//first check whether is a list for the hash
		if(!ServerState.isQueueforNet(NetName)){
#ifdef DEBUG 
			printf("inserting %s into hash table(s)\n", NetName.c_str());
#endif
			ServerState.addNewQueueforNet(NetName);
			//also make a new mutex for the request
			ReqMtxVec[NetName] = new std::mutex();
			// complete queues and corresponding std::mutexes
			cmpTableMtx[NetName]=new std::mutex();
		}
	}
}


void* sendBatchedRequest(std::shared_ptr<batched_request> &input_info, std::string strReqName, std::shared_ptr<TaskSpec> pTask, proxy_info* pPInfo) {
	char req_name[MAX_REQ_SIZE];
	strcpy(req_name, strReqName.c_str());

	int tnum = input_info->getNTask();

#ifdef DEBUG
	printf("[EXEC][%d,%d,%d] there are total %d requests batched for task %s\n",pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,tnum,req_name);
	printf("[EXEC][%d,%d,%d] batch ID : %d, batched task's task ID : ",pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,input_info->getBatchID());
	for (int id=0; id<tnum;id++){
		printf("%d ",input_info->getRequests()[id]->getTaskID());
	}    
	printf("\n");
#endif 

#ifdef DEV_LOG
	std::string msg=str_START + to_string(tnum);
	printTimeStampWithName(req_name,msg.c_str(),devlogTable[pPInfo]);
	fflush(devlogTable[pPInfo]);
#endif 


	assert(tnum == input_info->getBatchedTensor().size(0)); 

	pPInfo->sendMtx->lock();  

#ifdef DEBUG
	printf("[%s][EXEC][%d,%d,%d][%d] start task \n", timeStamp(), pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num, input_info->getBatchID());
#endif

	for (int id=0; id<input_info->getNTask();id++){
		input_info->getRequests()[id]->setstartExec(getCurNs());
	}     
	PerProxyOutputMtx[pPInfo]->lock();
	PerProxyOutputQueue[pPInfo]->push_back(input_info);
	PerProxyOutputMtx[pPInfo]->unlock();
	PerProxyOutputCV[pPInfo]->notify_all();
	at::Tensor output_temp = input_info->getBatchedTensor();
	// change next line for backend/
	//sendRequestToBackend(pPInfo->in_fd,input_info->getBatchID(), ServerState.PerModeltoIDMapping[strReqName] , output_temp);
	sendRequestToBackend(pPInfo->in_fd,input_info->getBatchID(), ServerState.getIDfromModel(strReqName) , output_temp);
	pPInfo->sendMtx->unlock();  

#ifdef DEBUG
	debug_mtx.lock();
	sent_tasks+=input_info->getBatchNum();    
	printf("[DEBUG] sent tasks: %u \n", sent_tasks);
	debug_mtx.unlock();    
#endif

	return NULL;
}


pthread_t initOutputThread(proxy_info *pPInfo){
	std::mutex randnumMtx; // use lock in order to ensure unique numberint deviceID = gpu_id;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future for now set to max * 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initOutput, (void *)pPInfo) != 0)
		LOG(ERROR) << "Failed to create a batching thread.\n";
	return tid;

}

void *initOutput(void *args){
	proxy_info* pPInfo = (proxy_info*)args;
	std::string strReqName;
	std::deque<std::shared_ptr<batched_request>> *pQueueList = PerProxyOutputQueue[pPInfo];
	while(true){
		std::unique_lock<std::mutex> lk(*PerProxyOutputMtx[pPInfo]);
		PerProxyOutputCV[pPInfo]->wait(lk, [&pQueueList]{return pQueueList->size();});
		std::shared_ptr<batched_request> input_info= pQueueList->front();
		pQueueList->pop_front();

		strReqName = input_info->getStrName();
		int tnum = input_info->getNTask();

		torch::Tensor output = recvResult(pPInfo, tnum, input_info->getBatchID());
#ifdef DEBUG
		printf("[%s][EXEC][%d,%d,%d][%d] end task \n", timeStamp(), pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num, input_info->getBatchID());
#endif
		lk.unlock();
		PerProxyOutputCV[pPInfo]->notify_all(); // just in case there  are other pending threads

#ifdef DEBUG
		debug_mtx.lock();
		completed_tasks+=input_info->getBatchNum();    
		printf("[DEBUG] completed tasks: %u \n", completed_tasks);
		debug_mtx.unlock();    
#endif

		pPInfo->PerTaskExecMtx->operator[](strReqName)->lock();
		pPInfo->isTaskExec->operator[](strReqName)--;
		pPInfo->PerTaskExecMtx->operator[](strReqName)->unlock();
#ifdef DEBUG
		printf("[%s][EXEC][%d,%d,%d][%d] ended task %s \n", timeStamp(), pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num, input_info->getBatchID(),strReqName.c_str());
#endif
		pPInfo->PerTaskExecCV->operator[](strReqName)->notify_all();



#ifdef DEBUG
		printf("[OUTPUT][%d,%d,%d] finished task %s \n", pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num,strReqName.c_str());
#endif 

		for (int id=0; id<tnum;id++){
			input_info->getRequests()[id]->setendExec(getCurNs());
		}

		input_info->allocateOutput(output); 
		sendBatchedResults(input_info, strReqName, pPInfo);
		PerProxyOutputCV[pPInfo]->notify_all(); // just in case there  are other pending threads

	}

}
