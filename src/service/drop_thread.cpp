#include "thread.h"
#include "common_utils.h"
#include "request.h"
#include "sys_monitor.h"
#include "global_scheduler.h"
#include <memory>
#include <queue>

const uint64_t INTERVAL_MILLIS=5;

extern SysMonitor ServerState;
extern std::map<std::string, std::mutex*> ReqMtxVec;
extern GlobalScheduler gScheduler;

void *initLazyDrop(void *args){
	double DropRatio = *(double*)(args);
	printf("[DropThread] DropRation initialzied as: %lf \n", DropRatio);
	while(1){
		usleep(INTERVAL_MILLIS *1000);
		for(auto it : *ServerState.getReqListHashTable()){
			if(!it.second.empty()){
				ReqMtxVec[it.first]->lock();
				//check from front and drop if needed
				while(!it.second.empty()){
					uint64_t now=getCurNs();
					uint64_t start = it.second.front()->getStart();
					std::shared_ptr<request> req = it.second.front();
					// compare with SLO
					if(double(now-start)/1000000 > DropRatio * double(gScheduler.getSLO(it.first))){
						req->setDropped(true);
						if(!req->getApp()->isDropped()){
							req->getApp()->setDropped(true);
							addtoAppQueue(req->getApp(), /*dropped=*/true);
						}
						ServerState.getCompQueue()->enqueue(req);
						it.second.pop();
					}
					else break;
				}
				ReqMtxVec[it.first]->unlock();         
			}
		}
	}
	return (void*)0;
}

void *initEagerDrop(void *args){
	double DropRatio = *(double*)(args);
	printf("[DropThread] DropRation initialzied as: %lf \n", DropRatio);
	while(1){
		usleep(INTERVAL_MILLIS *1000);
		for(auto it : *ServerState.getReqListHashTable()){
		//for(std::map<std::string, std::queue<std::shared_ptr<request>>>::iterator it=ServerState.ReqListHashTable.begin(); 
		//		it != ServerState.ReqListHashTable.end(); it++ ){
			int max_batch_size=32;
			if(!it.second.empty()){
				max_batch_size=0;
				int cap=0;
				std::string type;

				for (int i =0; i < ServerState.getNGPUs(); i++){
					for(auto pPInfo : *ServerState.getDevMPSInfo(i)){
						for (auto tinfo : *ServerState.getProxyNetList(pPInfo)){
							if (tinfo.first == it.first){
								if(tinfo.second > max_batch_size){
									max_batch_size = tinfo.second;
									cap=pPInfo->cap;
									type = pPInfo->type;
								}                                         
							}

						}
					}
				}

				if(max_batch_size ==0 || cap == 0) continue;

				ReqMtxVec[it.first]->lock();
				bool exit=false;
				//check from front and drop of L(b) + queueing delay > SLO) 
				while(!exit){
					uint64_t now=getCurNs();
					uint64_t start = it.second.front()->getStart();
					// get batch size, get max if there are several proxys
					int batch_size = (it.second.size() >= max_batch_size) ? max_batch_size : it.second.size();
					// compare with L(b) + queueing delay
					if(double(now-start)/1000000 + gScheduler.getEstLatency(it.first,batch_size,cap,type) > DropRatio * double(gScheduler.getSLO(it.first))){
						std::shared_ptr<request> req = it.second.front();

						req->setDropped(true);
						if(!req->getApp()->isDropped()){
							req->getApp()->setDropped(true);
							addtoAppQueue(req->getApp(), /*dropped=*/true);
						}
						ServerState.getCompQueue()->enqueue(req);
						it.second.pop();
					}
					else exit=true;
					if (it.second.empty()) exit=true;
				}
				ReqMtxVec[it.first]->unlock();         
			}
		}
	}
	return (void*)0;
}


pthread_t initDropThread(double *DropRatio){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB

	pthread_t tid;
#ifdef EAGER_DROP
	if (pthread_create(&tid, &attr, initEagerDrop, (void*)DropRatio) != 0)
		LOG(ERROR) << "Failed to create performance monitor thread.\n";

#else
	if (pthread_create(&tid, &attr, initLazyDrop, (void*)DropRatio) != 0)
		LOG(ERROR) << "Failed to create performance monitor thread.\n";
#endif
	return tid;

}


