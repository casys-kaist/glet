#include "global_scheduler.h"
#include "common_utils.h"
#include "interference_model.h"
#include "config.h"
#include "proxy_ctrl.h"
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include "scheduler_utils.h"
#include "backend_delegate.h"
#include "sys_monitor.h"

typedef void * (*THREADFUNCPTR)(void *);

/*Below are all initialzied in proxy_thread.cpp  */
extern std::map<proxy_info*, std::condition_variable*> PerProxyBatchCV;
extern std::map<proxy_info*,std::mutex*> PerProxyBatchMtx;
extern SysMonitor ServerState;
extern std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

GlobalScheduler::GlobalScheduler(){
}

GlobalScheduler::~GlobalScheduler(){
}

float GlobalScheduler::getBeaconInterval(){
	return BEACON_INTERVAL;
}

std::map<std::string, bool> getTasksNeedScheduling(SysMonitor* SysState){
	std::map<std::string, bool> needDecision;

	for(int i =0; i < SysState->getVectorOfNetNames()->size(); i++){
		std::string net_name = SysState->getVectorOfNetNames()->at(i);
		if(SysState->getLenofReqQueue(net_name)){
			needDecision[net_name] = true;
#ifdef DEBUG
			printf("[SCHEDULER] task : %s needs scheduling \n", net_name.c_str());
#endif
		}
		else 
			needDecision[net_name] = false;
	}
	return needDecision;
}

void GlobalScheduler::doMPSScheduling(SysMonitor* SysState){
//#ifdef DEBUG
	printf("[SCHEUDLER] execute scheduler \n");


	for (int i =0; i < SysState->getNGPUs(); i++) 
	{ 
		//for (unsigned int j =0; j<SysState->getDevMPSInfo(i)->size(); j++){
			//proxy_info* pPInfo = SysState->getDevMPSInfo(i)->operator[](j);
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			if(!pPInfo->isSchedulable) continue;
			for(auto iter1 : *SysState->getProxyNetList(pPInfo)){
			//for(std::vector<std::pair<std::string,int>>::iterator iter1 = SysState->PerProxyTaskList[pPInfo].begin(); 
			//		iter1 != SysState->PerProxyTaskList[pPInfo].end(); iter1++){
				printf("[SCHEDULER]BEFORE: task: %s batch: %d exec: %d in proxy[%d,%d,%d] \n",iter1.first.c_str(), \
						pPInfo->isTaskBatching->operator[](iter1.first),\
						pPInfo->isTaskExec->operator[](iter1.first), \
						pPInfo->dev_id, pPInfo->cap, pPInfo->dedup_num);
			}
		}
	}
//#endif 
	std::vector<std::shared_ptr<TaskSpec>> decision;
	decision = executeScheduler(SysState);

#ifdef DEBUG
	for(unsigned int i =0; i < decision.size(); i++ ){
		proxy_info* pPInfo = decision[i]->proxy;
		std::cout <<"[SCHEDULER]AFTER: scheduled: "<< decision[i]->ReqName << " " << proxy_info_to_string(pPInfo) << std::endl;
	}
	if (decision.empty()) printf("[SCHEDULER]AFTER: Nothing was scheduled  \n");
#endif
	if (decision.size()==0) return;

	for(unsigned int i =0; i < decision.size(); i++){
		proxy_info* pPInfo = decision[i]->proxy;
		PerProxyBatchMtx[pPInfo]->lock();
		SysState->insertToBatchQueueofProxy(pPInfo, decision[i]);
		PerProxyBatchCV[pPInfo]->notify_one(); 
		PerProxyBatchMtx[pPInfo]->unlock();
	}   
}


std::vector<std::shared_ptr<TaskSpec>> GlobalScheduler::executeScheduler(SysMonitor *SysState){
	std::vector<std::shared_ptr<TaskSpec>> decision;
	switch(mSchedulerMode){
		case MPS_STATIC:
			decision = staticMPSScheduler(SysState);
			break;
		case ORACLE:
			decision = OracleScheduler(SysState);
			break;
		default: // should not happen, conditions are already checked during initialization
			break;
	}
	return decision;
}

float GlobalScheduler::getTaskTgtRate(std::string model_name){
	return mPerTaskTgtRate[model_name];
}

void GlobalScheduler::initReqRate(std::string ReqName, int monitor_interval_ms, int query_interval_ms, float init_rate ){
	if (PerTaskArrivUpdateMtx.find(ReqName) == PerTaskArrivUpdateMtx.end()){
		PerTaskArrivUpdateMtx[ReqName] = new std::mutex();
		mPerTaskEWMARate[ReqName]=std::make_shared<EWMA::EWMA>(monitor_interval_ms, query_interval_ms);
		mPerTaskEWMARate[ReqName]->InitRate(init_rate);
	}
}

void GlobalScheduler::updateAvgReqRate(std::string ReqName, uint64_t timestamp){
	PerTaskArrivUpdateMtx[ReqName]->lock();
	mPerTaskEWMARate[ReqName]->UpdateRate(timestamp);
	PerTaskArrivUpdateMtx[ReqName]->unlock();
}

float GlobalScheduler::getTaskArivRate(std::string ReqName){
	PerTaskArrivUpdateMtx[ReqName]->lock();
	float rate=mPerTaskEWMARate[ReqName]->GetRate();
	PerTaskArrivUpdateMtx[ReqName]->unlock();
	return rate;
} 

void GlobalScheduler::addtoNetNames(std::string name)
{
	std::vector<std::string>::iterator it;
	it = find(mNetNames.begin(), mNetNames.end(), name);
	if (it == mNetNames.end()) {mNetNames.push_back(name);

#ifdef DEBUG
		printf("name pushed in netnames : %s \n", name.c_str());
#endif
	}

}


std::vector<std::string>* GlobalScheduler::getNetNames(){
	// the following should not fail if setup was called correctly
	return &mNetNames;
}

float GlobalScheduler::getMaxDelay(proxy_info *pPInfo){
	return pPInfo->duty_cycle;
}

int GlobalScheduler::getSLO(std::string name){
	return mPerModelSLO[name];
}

int GlobalScheduler::getModelID(std::string name){
	return SBPScheduler.getModelID(name);
}

bool sortdesc(const std::pair<std::string, unsigned int> &a, const std::pair<std::string, unsigned int> &b){
	return (a.second > b.second);
}

bool sort_slack_asc(const std::pair<std::string, float> &a, const std::pair<std::string,float> &b){
	return (a.second < b.second);
}

// schedules every available partition for the tasks
std::vector<std::shared_ptr<TaskSpec>> GlobalScheduler::staticMPSScheduler(SysMonitor *SysState){
	std::vector<std::shared_ptr<TaskSpec>> decision;
	std::map<std::string, bool> needDecision;
	needDecision= getTasksNeedScheduling(SysState);
	for(int j=0; j<SysState->getNGPUs(); j++){
		//for(unsigned int k=0; k<SysState->PerDevMPSInfo[j].size(); k++){
			//proxy_info *pPInfo = SysState->PerDevMPSInfo[j][k];
		for(auto pPInfo : *SysState->getDevMPSInfo(j)){
			if(pPInfo->LoadedModels.size()==0 || !pPInfo->isSchedulable) continue;
#ifdef DEBUG
			std::cout << "PerProxyBatchList size: " << SysState->getSizeofBatchQueueofProxy(pPInfo)<< std::endl;
#endif 
			//check if proxy is idle
			if(SysState->getSizeofBatchQueueofProxy(pPInfo) == 0){
				pPInfo->schedMtx->lock();
				bool assign_task=false;
				for(auto pair_info : *SysState->getProxyNetList(pPInfo)) {
					if( pPInfo->isTaskBatching->operator[](pair_info.first) + pPInfo->isTaskExec->operator[](pair_info.first) < 2  && \
							!pPInfo->isTaskBatching->operator[](pair_info.first)  && \
							needDecision[pair_info.first]) assign_task=true;
				}
				if(!assign_task){ 
					pPInfo->schedMtx->unlock();
					continue;
				}
				for(auto pair : *SysState->getProxyNetList(pPInfo)){
					if(!needDecision[pair.first]) continue;
					std::shared_ptr<TaskSpec> pTSpec = std::make_shared<TaskSpec>();
					pTSpec->device_id=j;
					pTSpec->BatchSize=pair.second;
					pTSpec->ReqName= pair.first;
					pTSpec->proxy = pPInfo;
					decision.push_back(pTSpec);
				}
				pPInfo->schedMtx->unlock();
			}
		}
	}
	return decision;
}
// seaches SysState->perProxyTaskList and updates everything batch size 
void GlobalScheduler::UpdateBatchSize(SysMonitor *SysState, std::string model_name, int new_batch_size){
	for(int i =0; i < SysState->getNGPUs(); i++){
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			for(auto task_info : *SysState->getProxyNetList(pPInfo)){
				if (task_info.first == model_name){
					task_info.second=new_batch_size;
#ifdef ST_DEBUG
					std::cout << __func__ <<": model: " << model_name << " batch size updated to " << new_batch_size
						<<std::endl;
#endif
				} 

			}
		}
	}
}

float GlobalScheduler::getTailLatency(proxy_info *pPInfo, std::string modelname){

	if (pPInfo->PerTaskLatencyVec->operator[](modelname)->size() == 0) return 0;
	std::mutex *Mtx = pPInfo->PerTaskLatencyVecMtx->operator[](modelname);
	Mtx->lock();

	float tail_latency = 0.0;

	for(std::deque<float>::iterator it = pPInfo->PerTaskLatencyVec->operator[](modelname)->begin(); 
			it != pPInfo->PerTaskLatencyVec->operator[](modelname)->end(); it++){
		if(tail_latency < (*it)) tail_latency = *it; 
	}
	Mtx->unlock();

	return tail_latency;
}
float GlobalScheduler::getAvgLatency(proxy_info *pPInfo, std::string modelname){
	unsigned int len=pPInfo->PerTaskLatencyVec->operator[](modelname)->size();

	if (len == 0) return 0; // not being tracked
	std::mutex *Mtx = pPInfo->PerTaskLatencyVecMtx->operator[](modelname);

	Mtx->lock();
	float sum=0;
	for(std::deque<float>::iterator it = pPInfo->PerTaskLatencyVec->operator[](modelname)->begin(); 
			it != pPInfo->PerTaskLatencyVec->operator[](modelname)->end(); it++){
		sum += *it;
	}
	Mtx->unlock();

	return float(sum/len);
}

void GlobalScheduler::clearPerModelLatency(proxy_info *pPInfo, std::string model){
	std::mutex *Mtx = pPInfo->PerTaskLatencyVecMtx->operator[](model);

	Mtx->lock();
	pPInfo->PerTaskLatencyVec->operator[](model)->clear();        
	Mtx->unlock();

#ifdef DEBUG
	printf("[PerModelLatency][%d,%d,%d] latency std::vector of %s was cleared \n",pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num, model.c_str());
#endif 

}

void GlobalScheduler::addLatency(proxy_info *pPInfo,std::string modelname, float latency){

	std::mutex *Mtx = pPInfo->PerTaskLatencyVecMtx->operator[](modelname);
	Mtx->lock();
	uint64_t VecSize = pPInfo->PerTaskLatencyVec->operator[](modelname)->size();
	if(VecSize > MA_WINDOW){
		for(uint64_t i=0; i < VecSize/2; i++) pPInfo->PerTaskLatencyVec->operator[](modelname)->pop_front(); // empty half of latency vec
	}
	if (pPInfo->PerTaskSkipCnt->operator[](modelname)<=0)
		pPInfo->PerTaskLatencyVec->operator[](modelname)->push_back(latency);
	else
		pPInfo->PerTaskSkipCnt->operator[](modelname)--;
	Mtx->unlock();
#ifdef DEBUG
	printf("[PerModelLatency][%d,%d,%d] added %lf to model %s \n", pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num, latency, modelname.c_str());
#endif 

}

// methods for update. managing latency values in EWMA style
float GlobalScheduler::getAvgLatency(std::string modelname){
	PerTaskLatUpdateMtx[modelname]->lock();
	float avg_val = mPerTaskEWMALat[modelname]->GetRate();
	PerTaskLatUpdateMtx[modelname]->unlock();
	return avg_val;
}
void GlobalScheduler::clearPerModelLatency(std::string model){
	PerTaskLatUpdateMtx[model]->lock();
	mPerTaskEWMALat[model]->InitRate(0.0);
	mPerTaskEWMALat[model]->UpdateValue(0.0);
	PerTaskLatUpdateMtx[model]->unlock();
}
void GlobalScheduler::addLatency(std::string modelname, float latency){
	PerTaskLatUpdateMtx[modelname]->lock();
	mPerTaskEWMALat[modelname]->UpdateValue(latency);
	PerTaskLatUpdateMtx[modelname]->unlock();
}   
float GlobalScheduler::getAvgTrpt(std::string modelname){
	PerTaskTrptUpdateMtx[modelname]->lock();
	float avg_val = mPerTaskEWMATrpt[modelname]->GetRate();
	PerTaskTrptUpdateMtx[modelname]->unlock();
	return avg_val;
}
void GlobalScheduler::clearPerModelTrpt(std::string model){
	PerTaskTrptUpdateMtx[model]->lock();
	mPerTaskEWMATrpt[model]->UpdateRate(0.0);
	PerTaskTrptUpdateMtx[model]->unlock();
}
void GlobalScheduler::addTrpt(std::string modelname, float latency){
	PerTaskTrptUpdateMtx[modelname]->lock();
	mPerTaskEWMATrpt[modelname]->UpdateValue(latency);
	PerTaskTrptUpdateMtx[modelname]->unlock();
}    

typedef struct _slackitem
{
	proxy_info *pPInfo;
	std::string modelname;
	float slack;

} SlackItem;

bool cmp_slack_item_asc(SlackItem &a, SlackItem &b){
	return a.slack < b.slack;

}

float GlobalScheduler::getEstLatency(std::string task, int batch, int cap, std::string type){   
	return  SBPScheduler.getLatency(type,getModelID(task),batch,cap);

}

bool cmp_task_rate_dsc(const Task &a, const Task &b){
	return a.request_rate > b.request_rate;
}

float GlobalScheduler::updateAndReturnMaxHistRate(std::string model_name, const float rate){
	mPerTaskRateHist[model_name].push_front(rate);
	if (mPerTaskRateHist[model_name].size() > HIST_LEN) {
		mPerTaskRateHist[model_name].pop_back();
	}
	auto mit =std::max_element(mPerTaskRateHist[model_name].begin(), mPerTaskRateHist[model_name].end());
	return *mit;
}

void GlobalScheduler::setupAllTasks(SysMonitor *SysState, std::vector<Task> &output_vec){
	std::vector<std::string> tasks;
	//for (std::map<std::string,std::queue<std::shared_ptr<request>>>::iterator it = SysState->ReqListHashTable.begin(); it != SysState->ReqListHashTable.end(); it++){
	for(auto it = SysState->getVectorOfNetNames()->begin(); it != SysState->getVectorOfNetNames()->end(); it++){
		// if target was not initiated at all and rate is 0, then continue
		float curr_rate = getTaskArivRate(*it);
		if (curr_rate ==0 && (mPerTaskTgtRate.find(*it) == mPerTaskTgtRate.end()) ) continue;
		if(mPerTaskTgtRate[*it]==0) continue;
		tasks.push_back(*it);
	} 
	if(!tasks.empty()) output_vec.clear();
	for(auto model_name : tasks){
		Task new_task;
		new_task.id= SBPScheduler.getModelID(model_name);
		new_task.request_rate=mPerTaskTgtRate[model_name];
		new_task.SLO = getSLO(model_name);
		// following will be modified by scheduler if successful
		new_task.throughput=0;
		new_task.batch_size=0;
#ifdef SCHED_DEBUG
		printf("[setupAllTasks] task: %d, avg rate: %d \n", new_task.id, new_task.request_rate );
#endif 
		output_vec.push_back(new_task);
	}

}


void GlobalScheduler::setupTasks(SysMonitor *SysState, bool include_downsize, std::vector<Task> &output_vec){
	std::vector<std::string> tasks;
	const float UP_THRESHOLD=5.0; // upper threshold of increasing rate, in percentage
	const int UP_THRESHOLD_RATE = 10; // upper threshold of increasing rate, in absolute rate
	const float DOWN_THRESHOLD=10.0; // threshold for lowering target range, in percentage
	const int DOWN_THRESHOLD_RATE = 10; // upper threshold of increasing rate, in absolute rate
	const float RATE_ROOM = 1.2;
	const float AGGRESIVE_RATE_ROOM=1.5;
	//for (std::map<std::string,std::queue<std::shared_ptr<request>>>::iterator it = SysState->ReqListHashTable.begin(); it != SysState->ReqListHashTable.end(); it++){
	for(auto it = SysState->getVectorOfNetNames()->begin(); it != SysState->getVectorOfNetNames()->end(); it++){
		// if target was not initiated at all and rate is 0, then continue
		float curr_rate = getTaskArivRate(*it);
		if (curr_rate ==0 && (mPerTaskTgtRate.find(*it) == mPerTaskTgtRate.end()) ) continue;
		float org_rate = updateAndReturnMaxHistRate(*it,curr_rate);
		float tgt_rate;
		if(org_rate < 125){
			tgt_rate = org_rate * AGGRESIVE_RATE_ROOM;
		}
		else  tgt_rate = org_rate * RATE_ROOM;
#ifdef EPOCH_DEBUG
		std::cout << "[setupTasks] org_rate: " << org_rate << "tgt_rate: " << tgt_rate << std::endl;
#endif


		if(curr_rate<1 && mPerTaskTgtRate[*it] > 1){
#ifdef EPOCH_DEBUG
			std::cout << "[setuptasks] SYS_FLUSH DETECTED" << std::endl;
#endif
			SysState->setSysFlush(true);
			mPerTaskTgtRate[*it]=0;
			continue;
		}

		if ( mPerTaskTgtRate[*it]==0) continue;
		// if target was not initiated at all, initiate targeting rate andd 
		if(mPerTaskTgtRate.find(*it) == mPerTaskTgtRate.end()){
			mPerTaskTgtRate[*it] =tgt_rate;
			tasks.push_back(*it);
		}
		else{
			float diff_pntg = ((tgt_rate - mPerTaskTgtRate[*it]) / mPerTaskTgtRate[*it]) * 100; 
			float diff_rate = tgt_rate - mPerTaskTgtRate[*it];
			if( diff_pntg > UP_THRESHOLD || diff_rate > UP_THRESHOLD_RATE){
				mPerTaskTgtRate[*it] = tgt_rate;
				tasks.push_back(*it);
			}

			if(include_downsize && diff_pntg < -DOWN_THRESHOLD ){
				// to avoid osciliating with too small request rates
#ifdef EPOCH_DEBUG
				std::cout << "[setuptasks] SYS_FLUSH DETECTED" << std::endl;
#endif
				SysState->setSysFlush(true);              
				mPerTaskTgtRate[*it] = tgt_rate;
				tasks.push_back(*it);
			}
		}
	} 
	for(auto model_name : tasks){
		Task new_task;
		new_task.id= SBPScheduler.getModelID(model_name);
		new_task.request_rate=mPerTaskTgtRate[model_name];
		new_task.SLO = getSLO(model_name);
		// following will be modified by scheduler if successful
		new_task.throughput=0;
		new_task.batch_size=0;
		output_vec.push_back(new_task);
	}

}


void BackupTgtRates(std::map<std::string, float> &org, std::map<std::string,float> &backup){
	for(auto pair_info : org){
		backup[pair_info.first]=pair_info.second;
	}

}
void RecoverTgtRates(std::map<std::string, float> &org, std::map<std::string,float> &backup){
	for(auto pair_info : backup){
		org[pair_info.first]=pair_info.second;
	}

}

void GlobalScheduler::recoverFromScheduleFailure(SysMonitor *SysState, std::map<std::string,float> &backup_rate, SimState &backup){
	printf("Failed scheduling!! Reverting for now! \n");
	RecoverTgtRates(mPerTaskTgtRate,backup_rate);
	removeMaxRates();
	recoverScheduler(backup, mCurrState);
}

void GlobalScheduler::RevertTgtRates(SimState &backup){
	for(auto info_pair : mPerTaskTgtRate){
		info_pair.second=0;
	}
	for(auto gpu_ptr : backup.vGPUList){
		for(auto node_ptr: gpu_ptr->vNodeList){
			for(auto task_ptr : node_ptr->vTaskList){
				mPerTaskTgtRate[SBPScheduler.getModelName(task_ptr->id)]+=task_ptr->throughput;
			}
		}
	}

}


void GlobalScheduler::updateGPUMemState(SimState &input, SysMonitor *SysState){
	updateGPUMemState(input,SysState,SysState->getNGPUs());
}

void GlobalScheduler::updateGPUMemState(SimState &input, SysMonitor *SysState, int num_of_gpus_to_update){
	if(input.vGPUList.empty()) SBPScheduler.initiateDevs(input, num_of_gpus_to_update);

	std::vector<int> dev_total_mem;
	std::vector<int> dev_used_mem;
	assert(input.vGPUList.size() >= num_of_gpus_to_update);
	for(int i =0; i <num_of_gpus_to_update; i++){
		input.vGPUList[i]->vLoadedParts.clear();
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			if(pPInfo->isSchedulable && !(pPInfo->LoadedModels.size() ==1 && pPInfo->isUnloading)){
				MemNode new_node;
				new_node.dedup_num=pPInfo->dedup_num;
				new_node.part=pPInfo->cap;
				input.vGPUList[i]->vLoadedParts.push_back(new_node);
#ifdef SCALE_DEBUG
				std::cout << __func__ << "part: [" << i<<","<<pPInfo->cap <<"] loaded to list" <<std::endl;
#endif
			}
		} 
		const bool ONLY_ONCE=true;
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			int total_mem = mProxyCtrl->getTotalMem(pPInfo);
			dev_total_mem.push_back(total_mem);
			dev_used_mem.push_back(std::min(mProxyCtrl->getUsedMem(pPInfo), total_mem));
			if(ONLY_ONCE) break;
		}
	}
	SBPScheduler.initDevMems(input);
	SBPScheduler.updateDevMemUsage(dev_used_mem,input);

#ifdef SCALE_DEBUG
	for(auto gpu_ptr : input.vGPUList){
		printMemState(gpu_ptr);

	}
#endif

}

// loads task_ptr to node_ptrs by checking loaded models
void GlobalScheduler::updateGPUModelLoadState(SimState &input, SysMonitor *SysState){
	assert(SysState->getNGPUs() == input.vGPUList.size());
	for(int i =0; i < SysState->getNGPUs(); i++)
	{
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			if(SysState->isProxyNetListEmpty(pPInfo)) continue;
			for(auto elem : *SysState->getProxyNetList(pPInfo)){
				for(auto node_ptr : input.vGPUList[i]->vNodeList){
					if(node_ptr->resource_pntg == pPInfo->cap && node_ptr->dedup_num == pPInfo->dedup_num)
					{
						TaskPtr new_task_ptr = std::make_shared<Task>();
						new_task_ptr->id=SBPScheduler.getModelID(elem.first);
						node_ptr->vTaskList.push_back(new_task_ptr);
					}
				}

			}
		}
	}

}

// compare prev_state and curr_state, and issue unloadModelasync
void GlobalScheduler::unloadModels(SimState &prev_state, SimState &curr_state, SysMonitor *SysState){

#ifdef EPOCH_DEBUG
	std::cout << "---prev_state---"  << std::endl;
	printResults(prev_state);
	std::cout << "---curr_state---" << std::endl;
	printResults(curr_state); 
#endif
	for(auto gpu_ptr: prev_state.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList){
			proxy_info *pPInfo = getProxyInfo(gpu_ptr->GPUID, node_ptr->resource_pntg, node_ptr->dedup_num, SysState);
			if(pPInfo->LoadedModels.empty()) continue;
			// try to find the node in curr state
			bool found = false;
			if(gpu_ptr->GPUID < curr_state.vGPUList.size()){ 
				for(auto node_ptr2 : curr_state.vGPUList[gpu_ptr->GPUID]->vNodeList){
					// if found!
					if(node_ptr2->resource_pntg == node_ptr->resource_pntg && node_ptr2->dedup_num == node_ptr->dedup_num){
						if(needToUnload(node_ptr2,pPInfo)){
							load_args *arg= new load_args();
							for(auto task_ptr : node_ptr2->vTaskList){
								arg->model_ids_batches.push_back(std::pair<int,int>(task_ptr->id,0));
							}
							arg->pPInfo=pPInfo;
							pPInfo->isUnloading=true;
							std::thread t(&GlobalScheduler::unloadModel_async, this, arg);
							t.detach();
						}
						found=true;
					}
				}
			}
			// if not found then just unload
			if(!found){
				load_args *arg= new load_args();
				arg->pPInfo=pPInfo;
				pPInfo->isUnloading=true;
				std::thread t(&GlobalScheduler::unloadModel_async, this, arg);
				t.detach();
			}
		}
	}

}


int GlobalScheduler::FullRepartition(SimState &input, SimState &output, SysMonitor *SysState){
	input.vGPUList.clear();
	output.vGPUList.clear();
	// update memory usage for input
	updateGPUMemState(input, SysState,SBPScheduler.getMaxGPUs());
	std::vector<Task> session;
	setupAllTasks(SysState,session);
	sort(session.begin(), session.end(), cmp_task_rate_dsc);
	return SBPScheduler.runScheduling(&session, input,output,true);
}


std::vector<std::shared_ptr<TaskSpec>> GlobalScheduler::OracleScheduler(SysMonitor *SysState){
	static uint64_t last_epoch=0;
	bool flush= false; // flag indicating whether to force flush or not
	std::vector<std::shared_ptr<TaskSpec>> decision;
	if (last_epoch ==0) last_epoch = getCurNs(); // this only occurs once

	uint64_t now = getCurNs();
	std::vector<Task> session;
	std::vector<Task> backup_session; // in case we need to do revert to previous session
	std::map<std::string, float> backup_tgt_rate; // in case we need to revert
	// if we need to update state this epoch then update routing info
	if((now-last_epoch) / 1000000 > EPOCH_INTERVAL){
		uint64_t start = getCurNs();
		uint64_t p1=0;
		BackupTgtRates(mPerTaskTgtRate,backup_tgt_rate);
		if(mNeedUpdate){
			applyRouting_sync(mNextState,SysState);
			// time:flow, curr->prev, next->curr
			copyToOutput(mCurrState,mPrevState);
			copyToOutput(mNextState,mCurrState);
			mNeedUpdate=false;
			// the following unloadModels is asynchronous call
			if(!mPrevState.vGPUList.empty()){
				unloadModels(mPrevState, mCurrState,SysState);
			} 

		}
		p1=getCurNs();
		setupTasks(SysState,true,session);
#ifdef EPOCH_DEBUG
		std::cout << "[setuptasks] SYS_FLUSH: " << SysState->isSysFlush()
			<< std::endl;
		if(!session.empty()){
			printf("session length: %lu \n", session.size());
			for (auto task : session){
				printf("task: %d, float: %lf \n", task.id, mPerTaskTgtRate[SBPScheduler.getModelName(task.id)]);
			}
		}
#endif
		if(!session.empty()){
#ifdef EPOCH_DEBUG
			printf("[OracleScheduler] (epoch) called %s \n", timeStamp() );
#endif
			//sort(session.begin(), session.end(), cmp_task_rate_dsc);
			copySession(session,backup_session);
			SimState output;
			// make a backup in case we need to revert
			SimState Backup,prev_output;
			copyToOutput(mCurrState,Backup);

			bool success =false;
			while(!success){
				output.vGPUList.clear();
				//initiate prev_output with currnet state of scheduling
				copyToOutput(mCurrState,prev_output);

				if(doScheduling(session,SysState,prev_output,output)){
#ifdef SCALE
					int curr_gpus = SBPScheduler.getMaxGPUs();
					// check if we can get more GPUs
					if(curr_gpus < SysState->getNGPUs()){// if so, add more gpus and continue
#ifdef SCALE_DEBUG
						std::cout << "SCALE-UP from " << curr_gpus << " to " << curr_gpus+1 << std::endl; 
#endif 
						// force full partition for next scheduling;

						SysState->setSysFlush(true);
						SBPScheduler.setMaxGPUs(++curr_gpus);
					}
					else{ // if not, this is impossible to schedule
						break;
					}
#else
					break;
#endif

				}
				else success=true;
			}
			if(!success){ // if impossible, revert to previous decision(stored in backup)
				recoverFromScheduleFailure(SysState,backup_tgt_rate,Backup);
			}
			else 
			{
#ifdef SCALE
				checkandReduceGPU(output,SysState);
#endif
				copyToOutput(output, mNextState);
				for(auto gpu_ptr : output.vGPUList) additionalLoad(gpu_ptr,SysState);
				mNeedUpdate=true;
#ifdef EPOCH_DEBUG
				std::cout << __func__ << ": nextstate:" << std::endl;
				printResults(mNextState);
#endif 
			} // if successfull
#ifdef EPOCH_DEBUG
			uint64_t end = getCurNs();
			printf("[OracleScheduler] SYNC took %lf ms \n", double(p1-start)/1000000);
			printf("[OracleScheduler] scheduling took %lf ms \n", double(end-start)/1000000 );
#endif
		} // session not empty
		last_epoch=getCurNs();
	}
	decision = staticMPSScheduler(SysState);
	return decision;

}



bool GlobalScheduler::doScheduling(std::vector<Task> &session, SysMonitor *SysState, SimState &prev_output, SimState &new_output){
	new_output.vGPUList.clear();
#ifdef SCALE
	// if current gpus and scheduler's max gpu is not same, then reset scheduling results
	if(SBPScheduler.getMaxGPUs() != prev_output.vGPUList.size()){
		SysState->setSysFlush(true);
		prev_output.vGPUList.clear();
	}
#endif
	updateGPUMemState(prev_output, SysState,SBPScheduler.getMaxGPUs());
	if(SysState->isSysFlush()){
#ifdef EPOCH_DEBUG
		printf("SYS_FLUSH_CALLED !! \n");
#endif 
		SysState->setSysFlush(false);
		if(FullRepartition(prev_output,new_output,SysState)){
			return EXIT_FAILURE;
		}
	}
	else{
		// 1 try all ~~
		if (SBPScheduler.runScheduling(&session, prev_output,new_output,false))
		{                 
#ifdef EPOCH_DEBUG
			printf("[OracleScheduler] INCREMENT_FAILED \n");
#endif
			// 2) if it failed, ~~
			if(FullRepartition(prev_output,new_output,SysState)){
				return EXIT_FAILURE;
			}  
		}
	}
	return EXIT_SUCCESS;

}


void GlobalScheduler::refillTaskList(SysMonitor *SysState, std::vector<Task> &task_list){
	for(auto it = SysState->getVectorOfNetNames()->begin(); it != SysState->getVectorOfNetNames()->end(); it++){
		float curr_rate = getTaskArivRate(*it);
		if (curr_rate ==0) continue;
		auto mit =std::max_element(mPerTaskRateHist[*it].begin(), mPerTaskRateHist[*it].end());
		float tgt_rate = *mit;
		// revert to higher rate, just to be safe
		mPerTaskTgtRate[*it]= std::max(mPerTaskTgtRate[*it], tgt_rate);
		Task new_task;
		new_task.id= SBPScheduler.getModelID(*it);
		new_task.request_rate=mPerTaskTgtRate[*it];
		new_task.SLO = getSLO(*it);
		// following will be modified by scheduler if successful
		new_task.throughput=0;
		new_task.batch_size=0;
#ifdef EPOCH_DEBUG
		printf("[IncrementalScheduling] task: %d, avg rate: %d \n", new_task.id, new_task.request_rate );
#endif
		task_list.push_back(new_task);
	}
}


bool GlobalScheduler::checkIfLoaded(proxy_info *pPInfo, std::string model_name){
	if(find(pPInfo->LoadedModels.begin(), pPInfo->LoadedModels.end(), model_name) != pPInfo->LoadedModels.end()){                
		return true;
	}
	return false;
}

bool GlobalScheduler::needToLoad(NodePtr &node_ptr, proxy_info *pPInfo){
	for(auto task_ptr : node_ptr->vTaskList){
		std::string str_name=SBPScheduler.getModelName(task_ptr->id);
		if(!checkIfLoaded(pPInfo,str_name)){
			return true;
		}
	}
	return false;
}

// checks whether models need to be loaded, load if it has to
void GlobalScheduler::additionalLoad(GPUPtr &gpu_ptr, SysMonitor *SysState){
	for(auto node_ptr : gpu_ptr->vNodeList){
		// if node is a reserved node then 
		if(node_ptr->vTaskList.empty()) continue;
		proxy_info *pPInfo = getProxyInfo(node_ptr->id,node_ptr->resource_pntg,node_ptr->dedup_num,SysState);

		load_args *args = new load_args();
		for(auto task_ptr : node_ptr->vTaskList){
			args->model_ids_batches.push_back(std::pair<int,int>(task_ptr->id,task_ptr->batch_size));
		}
		//load model asynch
		args->pPInfo=pPInfo;
		std::thread t(&GlobalScheduler::loadModel_sync_wrapper,this,args);
		t.detach();  

	}
}


//if there is a better policy for deciding when to unlaod, update this code
bool GlobalScheduler::needToUnload(NodePtr &node_ptr, proxy_info* pPInfo){
	// check whether there are models that should not be in proxy
	assert(node_ptr->id == pPInfo->dev_id &&\
			node_ptr->resource_pntg == pPInfo->cap &&\
			node_ptr->dedup_num == pPInfo->dedup_num );
	if(pPInfo->LoadedModels.empty()) return false;   
	for(auto str_name : pPInfo->LoadedModels){
		int model_id = SBPScheduler.getModelID(str_name);

		bool found=false;
		for(auto task_ptr : node_ptr->vTaskList){
			if(task_ptr->id == model_id) found=true; 
		}
		// if there is  at least one model that is not in vTaskList, return true
		if(!found) {
			return true;
		}
	}
	return false;
}

// the following function finds and unload models 
// Ex) some GPU in SysState={A,B}, gpu_ptr={B,C}, then B will not be unloaded but A will get unloaded 
void GlobalScheduler::unloadModels(GPUPtr &gpu_ptr, SysMonitor *SysState){
	for(auto pPInfo : *SysState->getDevMPSInfo(gpu_ptr->GPUID)){

		if(pPInfo->LoadedModels.empty()) continue;
#ifdef EPOCH_DEBUG
		std::cout << __func__<<":"<< proxy_info_to_string(pPInfo) << ": LoadedModels: ";
		for(auto model : pPInfo->LoadedModels){
			std::cout << " " << model;
		}
		std::cout << std::endl;
#endif
		bool found=false;
		for(auto node_ptr : gpu_ptr->vNodeList){
			if((node_ptr->resource_pntg == pPInfo->cap && node_ptr->dedup_num == pPInfo->dedup_num)){
				found = true;
				if(needToUnload(node_ptr,pPInfo)){
					load_args *args = new load_args();
					args->pPInfo=pPInfo;
					// push model's id which should stay loaded
					for(auto task_ptr : node_ptr->vTaskList){
						args->model_ids_batches.push_back(std::pair<int,int>(task_ptr->id,0));
					}
					pPInfo->isUnloading=true;
					std::thread t(&GlobalScheduler::unloadModel_async, this,args);
					t.detach();
					break;
				}
			}
		}
		if(!found){
			load_args *args = new load_args();
			args->pPInfo=pPInfo;
			// push model's id which should stay loaded
			pPInfo->isUnloading=true;
			std::thread t(&GlobalScheduler::unloadModel_async, this,args);
			t.detach();
		}
	}
}

void GlobalScheduler::applyReorganization(SysMonitor *SysState, const SimState &output){
	for(auto gpu_ptr : output.vGPUList){
		// async call to unloading models in background
		unloadModels(gpu_ptr,SysState);
		// async call to load models in background
		additionalLoad(gpu_ptr,SysState);
	}

}

int getRealIDfromMap(std::map<int, int> &id_map, int virt_id)
{
	for(auto obj : id_map){
		if(obj.second == virt_id)
			return obj.first;
	}
	return -1; // not supposed to happen
}

bool containID(std::vector<Task> &task_vec, int id_to_find){
	for(auto t : task_vec){
		if(t.id == id_to_find) return true;
	}
	return false;

}

void extractTasksFromProxys(std::vector<proxy_info*> &proxy_vec, const SimState &output, std::vector<Task> &output_task_vec){
	std::vector<std::string> model_name_vec;
	for(auto pPInfo : proxy_vec){
		for(auto gpu_ptr : output.vGPUList){
			for(auto node_ptr: gpu_ptr->vNodeList){
				if(pPInfo->dev_id != node_ptr->id || pPInfo->cap != node_ptr->resource_pntg || pPInfo->dedup_num != node_ptr->dedup_num) continue;
				for(auto task_ptr: node_ptr->vTaskList){
					// check if output_task_vec already has task_ptr->id
					if(containID(output_task_vec,task_ptr->id)){
						for(auto t : output_task_vec){
							if(t.id == task_ptr->id) t.request_rate += task_ptr->request_rate;
						}
						continue;
					}
					Task new_task;
					new_task.id=task_ptr->id;
					new_task.request_rate=task_ptr->request_rate;
					new_task.SLO=task_ptr->SLO;
					output_task_vec.push_back(new_task);
				}
			}

		}
	}

}

void GlobalScheduler::removeMaxRates(){
	for(auto pair_info : mPerTaskRateHist)
	{

		std::deque<float> *pHist = &mPerTaskRateHist[pair_info.first];
#ifdef EPOCH_DEBUG
		std::cout << "model: "<< pair_info.first << " hist_len: "<< pHist->size()<< std::endl;
#endif
		if(pHist->empty() || pHist->size() ==1) continue;

		auto mit = std::max_element(pHist->begin(), pHist->end());
		float max_val = *mit;
		pHist->erase(remove(pHist->begin(), pHist->end(),max_val),pHist->end());
	}
}


void clearAllTasks(SimState &input){
	for(auto gpu_ptr : input.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList){
			node_ptr->duty_cycle=0;
			node_ptr->vTaskList.clear();
			node_ptr->occupancy=0;
		}
	}

}

int GlobalScheduler::getTotalMemory_backend(int id, int node_id){
	BackendDelegate *pbd=NodeIDtoBackendDelegate[node_id];
	return pbd->getDeviceSpec()->getTotalMem();
}

int GlobalScheduler::getUsedMemory_backend(proxy_info *pPInfo){
	// return in MB
	return mProxyCtrl->getUsedMem(pPInfo);
}


int GlobalScheduler::oneshotScheduling(SysMonitor *SysState){
#ifdef SCHED_DEBUG
	std::cout << __func__ << ": called! "
		<<std::endl;
#endif
	// setup tasks
	std::vector<Task> tasks;
	setupAllTasks(SysState,tasks);
#ifdef SCALE_DEBUG
	std::cout << __func__ << ": initiate devs with " << SysState->getNGPUs() <<" gpus" <<std::endl;
#endif
	assert(SysState->getNGPUs()>=1);
	int init_ngpus=1;
	bool fail=true;
	while(fail){
		SBPScheduler.setMaxGPUs(init_ngpus);
		flushandUpdateDevs(mPrevState,init_ngpus);
#ifdef SCHED_DEBUG
		std::cout << __func__ << "init_ngpus: " << init_ngpus << ", with following setup: " << std::endl;
		printResults(mPrevState);
#endif
		fail = SBPScheduler.runScheduling(&tasks,mPrevState,mCurrState,/*allow_repart=*/true);
		if(fail && init_ngpus == SysState->getNGPUs()){
#ifdef SCHED_DEBUG
			std::cout << __func__ << ": intial scheduling FAILED! "
				<< std::endl;
#endif
			return EXIT_FAILURE;
		}
		//
		init_ngpus++;
	}
	std::cout << __func__ << ":  successfully finished! the results are as following: "
		<< std::endl;
	printResults(mCurrState);
#ifdef SCALE_DEBUG
	int used_gpus = getNumofUsedGPUs(mCurrState);
	std::cout << __func__ <<": scheduling result used : " << used_gpus << " out of total :"<< SBPScheduler.getMaxGPUs()
		<<std::endl;
#endif


	for(auto gpu_ptr : mCurrState.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList){
			proxy_info *pPInfo = getProxyInfo(gpu_ptr->GPUID,node_ptr->resource_pntg,node_ptr->dedup_num,SysState);
#ifdef FRONTEND_DEBUG
			std::cout << "[OneShotScheduling] booting proxy [" << pPInfo->dev_id << ", " << pPInfo->partition_num << "]" << std::endl;     
#endif
			if(node_ptr->vTaskList.empty()) continue;
			bootProxy_sync(pPInfo);
			std::vector<std::pair<int,int>> model_ids_batches;
			for(auto task_ptr : node_ptr->vTaskList){
				model_ids_batches.push_back(std::pair<int,int>(task_ptr->id, task_ptr->batch_size));
			}
			loadModel_sync(pPInfo, model_ids_batches);
		}
	}
	applyRouting_sync(mCurrState,SysState);

	return EXIT_SUCCESS;
}


int GlobalScheduler::readAndBoot(SysMonitor* SysState, std::string model_list_file){
	setupProxyTaskList(model_list_file, SysState);
	for(int i =0; i < SysState->getNGPUs(); i++){
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			if(SysState->isProxyNetListEmpty(pPInfo)) continue;
			bootProxy_sync(pPInfo);
		}
	}
	loadAllProxys(SysState);

//#ifdef FRONTEND_DEBUG
	for(int i =0; i < SysState->getNGPUs(); i++){
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			if(SysState->isProxyNetListEmpty(pPInfo)) continue;
			int used_mem = mProxyCtrl->getUsedMem(pPInfo);
			std::cout << "USED MEMORY OF GPU" << pPInfo->dev_id << ": " << used_mem<<std::endl;
		}
	}
//#endif 

}

bool GlobalScheduler::checkLoad(int model_id, proxy_info* pPInfo){
	return mLoadBalancer.checkLoad(model_id,pPInfo);    
}

bool GlobalScheduler::checkandReduceGPU(SimState &decision, SysMonitor *SysState){
#ifdef SCALE_DEBUG
	std::cout << __func__<<": received the following: " << std::endl;
	printResults(decision);
#endif
	int used_gpus = getNumofUsedGPUs(decision);
	// if less gpus can suffice, use less gpus
	if(SBPScheduler.getMaxGPUs() > used_gpus){
#ifdef SCALE_DEBUG
		std::cout << "SCALE-DOWN from " << SBPScheduler.getMaxGPUs() << " to " << used_gpus << std::endl; 
#endif  
		SBPScheduler.setMaxGPUs(used_gpus);
		return true;
	}
	return false;
}

void GlobalScheduler::flushandUpdateDevs(SimState &input, const int num_of_gpus){
	assert(num_of_gpus);
	input.vGPUList.clear();
	SBPScheduler.initiateDevs(input,num_of_gpus);
	std::vector<int> per_dev_mem;
	int checked_gpus=0;
	for(auto pair: NodeIDtoBackendDelegate){
		BackendDelegate *pbd = pair.second;       
		for(int i =0; i < pbd->getNGPUs(); i++){
			per_dev_mem.push_back(getTotalMemory_backend(i,pair.first));
			checked_gpus++;
			if(checked_gpus == num_of_gpus) break;
		}
		if(checked_gpus == num_of_gpus) break;
	}
	SBPScheduler.initDevMems(mPrevState);
}
