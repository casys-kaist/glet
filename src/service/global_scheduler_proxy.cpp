#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <cassert>

#include "global_scheduler.h"
#include "common_utils.h"
#include "config.h"
#include "proxy_ctrl.h"
#include "scheduler_utils.h"
#include "backend_delegate.h"
#include "backend_proxy_ctrl.h"

extern SysMonitor ServerState;
std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

int getJobID(std::string model_name, SysMonitor *SysState){
	return SysState->getIDfromModel(model_name);
}

proxy_info* GlobalScheduler::getProxyInfo(int dev_id,int cap, int dedup_num, SysMonitor* SysState){
#ifdef FRONTEND_DEBUG
	std::cout << __func__<< ": dev_id: " << dev_id << ", nGPUs: " << SysState->getNGPUs()
		<<std::endl;
#endif 
	assert(dev_id < SysState->getNGPUs());
	proxy_info* ret = NULL;
	for(auto pPInfo : *SysState->getDevMPSInfo(dev_id)){
		if(pPInfo->cap == cap && pPInfo->dedup_num == dedup_num) 
		{
			ret=pPInfo;
			break;
		}
	}
	assert(ret != NULL);
	return ret;
}


void GlobalScheduler::loadModelstoProxy(proxy_info* pPInfo, SysMonitor *SysState){
	if(!SysState->isProxyNetListEmpty(pPInfo)){
		std::cout << "[loadModelstoProxy] issueing load instruction to proxy["<< pPInfo->dev_id <<"," << pPInfo->cap << "," << pPInfo->dedup_num << "]" << std::endl;
	}   
	for(auto pair_info: *SysState->getProxyNetList(pPInfo)){
		std::string model_name= pair_info.first;
		int batch_size = pair_info.second;
		std::cout << "[loadModelstoProxy] issueing load inst for model: " << model_name << std::endl;
		int jobid=getJobID(model_name,SysState);
		mProxyCtrl->makeProxyLoadModel(pPInfo,jobid,batch_size);
		if(find(pPInfo->LoadedModels.begin(), pPInfo->LoadedModels.end(),model_name) == pPInfo->LoadedModels.end()){
			pPInfo->LoadedModels.push_back(model_name);
		}   
	}   
	return;
}

void GlobalScheduler::unloadModelsfromProxy(proxy_info* pPInfo, SysMonitor *SysState){
	for(auto model_name : pPInfo->LoadedModels){
		mProxyCtrl->makeProxyUnloadModel(pPInfo,getJobID(model_name,SysState));
	}   
	mProxyCtrl->waitProxy(pPInfo,RUNNING);
	pPInfo->LoadedModels.clear();
	return;

}

void GlobalScheduler::bootProxy_sync(proxy_info* pPInfo){
#ifdef EPOCH_DEBUG
	std::cout << __func__ << ": started " << proxy_info_to_string(pPInfo) << std::endl;
#endif
	assert(!pPInfo->isConnected);
	mProxyCtrl->startProxy(pPInfo, ServerState.getNGPUs(), ServerState.getNumProxyPerGPU());
	std::cout << __func__<< ": finished startProxy "<< std::endl;
	mProxyCtrl->waitProxy(pPInfo,RUNNING);
	pPInfo->isBooted=true;
	mProxyCtrl->connectProxy(pPInfo);
	pPInfo->isConnected=true;
#ifdef EPOCH_DEBUG
	std::cout << __func__ << ": completed " << proxy_info_to_string(pPInfo) << std::endl;
#endif

}

void GlobalScheduler::shutdownProxy_sync(proxy_info* pPInfo){
#ifdef EPOCH_DEBUG
	std::cout << __func__ << ": started " << proxy_info_to_string(pPInfo) << std::endl;
#endif
	//assumptions 
	assert(pPInfo->LoadedModels.empty());
	assert(pPInfo->isConnected);
	assert(pPInfo->isBooted);
	pPInfo->isSchedulable=false;
	mProxyCtrl->endProxy(pPInfo);
	mProxyCtrl->waitProxy(pPInfo,FLUSHED);
	mProxyCtrl->disconnectProxy(pPInfo);
	mProxyCtrl->waitProxy(pPInfo,COLD);
	pPInfo->isBooted=false;
	pPInfo->isConnected=false;
#ifdef EPOCH_DEBUG
	std::cout << __func__ << ": completed " << proxy_info_to_string(pPInfo) << std::endl;
#endif

}

void* GlobalScheduler::loadModel_sync_wrapper(void *args){
#ifdef EPOCH_DEBUG
	uint64_t start,end;    
	start=getCurNs();
#endif
	load_args* load_arg= (load_args*)args;
	proxy_info* pPInfo = load_arg->pPInfo;
	loadModel_sync(pPInfo, load_arg->model_ids_batches);
	free(args);
	return (void*)0;
}

void GlobalScheduler::loadModel_sync(proxy_info* pPInfo, std::vector<std::pair<int,int>> &model_ids_batches){
#ifdef EPOCH_DEBUG
	uint64_t start,end;    
	start=getCurNs();
#endif
	// wait until unloading is finsiheed(if called) 
	pPInfo->isLoading=true;
	if (pPInfo->isUnloading){
#ifdef EPOCH_DEBUG
		usleep(1*1000*1000); 
		std::cout << "[loadModel_sync]"<<proxy_info_to_string(pPInfo)<<" waiting for unload to finish" <<std::endl;
#endif

	}
	while(pPInfo->isUnloading){
		usleep(1*1000);
	}
	// if proxy not booted then wait until it boots 
	if(mProxyCtrl->getProxyState(pPInfo) == COLD && ALLOW_REPART){
#ifdef EPOCH_DEBUG
		std::cout << "[loadModel_sync]"<<proxy_info_to_string(pPInfo)<<" waiting for proxy to boot" <<std::endl;
#endif
		bootProxy_sync(pPInfo);
	}    

	for(auto id_batch_pair : model_ids_batches){
		int id = id_batch_pair.first;
		int batch_size = id_batch_pair.second;
		if(!checkIfLoaded(pPInfo,SBPScheduler.getModelName(id))){
			mProxyCtrl->markProxy(pPInfo,LOADING);        

#ifdef EPOCH_DEBUG
			std::cout << "[loadModel_sync]" << "(" << timeStamp()<< ")" <<"isueing load model " << id 
				<<" with batch size : " << batch_size
				<<" to " <<proxy_info_to_string(pPInfo)<< std::endl;
#endif
			mProxyCtrl->makeProxyLoadModel(pPInfo,id,batch_size);
			pPInfo->LoadedModels.push_back(SBPScheduler.getModelName(id));
		}

	}
	mProxyCtrl->waitProxy(pPInfo,RUNNING);
#ifdef EPOCH_DEBUG
	end=getCurNs();
	std::cout << "[loadModel_sync]" << proxy_info_to_string(pPInfo) << "finished loading models"<< std::endl;
	std::cout << "[loadModel_sync]"<< proxy_info_to_string(pPInfo) << "loading took: "<< double(end-start)/1000000 << "ms"<< std::endl;
#endif
	pPInfo->isLoading=false;
}



void* GlobalScheduler::unloadModel_async(void* args){
	load_args* arg = (load_args *)args;
	proxy_info* pPInfo = arg->pPInfo;
#ifdef EPOCH_DEBUG
	uint64_t start,end;
	start=getCurNs();
	std::cout << "[unloadModel_async]"<< proxy_info_to_string(pPInfo)<< " started at "<< timeStamp() << std::endl;;
#endif
	// 1. make sure requests are flushed before proceeding to unload
#ifdef EPOCH_DEBUG
	std::cout << __func__<<":"<< proxy_info_to_string(pPInfo) << ": LoadedModels: ";
	for(auto model : pPInfo->LoadedModels){
		std::cout << " " << model;
	}
	std::cout << std::endl;
	std::cout << __func__<<":"<< proxy_info_to_string(pPInfo) << ": exempting ids: ";
	for(auto id : arg->model_ids_batches){
		std::cout << " " << id;
	}
	std::cout << std::endl;


#endif
	std::vector<int> exempting_ids={};
	for(auto id_batch_pair : arg->model_ids_batches ){
		exempting_ids.push_back(id_batch_pair.first);
	}

	for(auto it2 = pPInfo->LoadedModels.begin() ; it2 != pPInfo->LoadedModels.end();){
		std::string model_name = *it2;
		int model_id = SBPScheduler.getModelID(model_name);

		// if loaded model is not supposed to be loaded
		if(find(exempting_ids.begin(), exempting_ids.end(), model_id) == exempting_ids.end()){
			bool is_task_cleared=false;
			while(!is_task_cleared){
				is_task_cleared=true;
				if(ServerState.getSizeofBatchQueueofProxy(pPInfo) !=0){
					is_task_cleared=false;
				}
				if(pPInfo->isTaskBatching->operator[](model_name) != 0) is_task_cleared=false;
				if(pPInfo->isTaskExec->operator[](model_name) != 0) is_task_cleared=false;
				if(!is_task_cleared) usleep(1*1000);
#ifdef FRONTEND_DEBUG
#endif
			}
			mProxyCtrl->markProxy(pPInfo,LOADING);

#ifdef EPOCH_DEBUG
			std::cout << "[unloadModel_async]" << "(" << timeStamp()<< ")" << proxy_info_to_string(pPInfo) << "issueing unload model_id: "<< model_id << std::endl;
#endif
			mProxyCtrl->makeProxyUnloadModel(pPInfo,model_id);
			it2=pPInfo->LoadedModels.erase(it2);

		}
		else (it2)++;
	}
#ifdef EPOCH_DEBUG
	std::cout << "[unloadModel_async]" << "(" << timeStamp()<< ")" << proxy_info_to_string(pPInfo) << "waiting for proxy" << std::endl;
#endif

	mProxyCtrl->waitProxy(pPInfo,RUNNING);
#ifdef EPOCH_DEBUG
	std::cout << "[unloadModel_async]" << "(" << timeStamp()<< ")" << proxy_info_to_string(pPInfo) << "ended waiting" << std::endl;
#endif
	free(arg);
#ifdef EPOCH_DEBUG
	end=getCurNs();
	std::cout << "[unloadModel_async]" << proxy_info_to_string(pPInfo) << " finished! took " <<\
		double(end-start)/1000000<< "ms" <<std::endl;;
#endif
	// if there are no more models for this proxy, then shut it down
	if(pPInfo->LoadedModels.empty() && ALLOW_REPART){
		shutdownProxy_sync(pPInfo);
		pPInfo->isReserved=false;
		pPInfo->isSchedulable=false;
	}
	pPInfo->isUnloading=false;


}

void GlobalScheduler::applyRouting_sync(SimState &output, SysMonitor *SysState){
	// 1. first flush all routing info in SysState
	// and make sure every proxy has finished loading
#ifdef EPOCH_DEBUG
	std::cout << __func__ << ": called at " << timeStamp() << std::endl;
#endif

	for(int i =0; i < SysState->getNGPUs(); i++){
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			SysState->getProxyNetList(pPInfo)->clear();

#ifdef EPOCH_DEBUG
			std::cout << __func__ << ": waiting for " << proxy_info_to_string(pPInfo)
				<< std::endl;
#endif
			while(pPInfo->isLoading) {usleep(1*1000);}
			pPInfo->isSchedulable=false;
		}
	}
	// 2. found proxy_infos and update them
	for(auto gpu_ptr : output.vGPUList){
		for(auto iter1 : gpu_ptr->vNodeList){          
			proxy_info *pPInfo = getProxyInfo(iter1->id,iter1->resource_pntg,iter1->dedup_num,SysState);
			if(!iter1->vTaskList.empty()){
				//pPInfo->isReserved=false;
				for(auto task_ptr : iter1->vTaskList){
					std::pair<std::string, int> new_pair;
#ifdef EPOCH_DEBUG
					std::cout << __func__ << ": pushed model " << task_ptr->id << "to proxy" << proxy_info_to_string(pPInfo) << std::endl;
#endif 
					new_pair.first=SBPScheduler.getModelName(task_ptr->id);
					new_pair.second=task_ptr->batch_size;
					SysState->insertNetToProxyNetList(pPInfo,new_pair);
				}
				pPInfo->isSchedulable=true;
				pPInfo->duty_cycle=iter1->duty_cycle;
			}
		}

	}
	mLoadBalancer.updateTable(output);
}

