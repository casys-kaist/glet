#include "proxy_ctrl.h"
#include "thread.h"
#include "common_utils.h"
#include "interference_model.h"
#include "config.h"
#include "global_scheduler.h"
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <assert.h>
#include "scheduler_utils.h"
#include "backend_delegate.h"
#include "json/json.h"

extern SysMonitor ServerState;
extern std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

const int DEFAULT_MAX_BATCH=32;

int GlobalScheduler::setSchedulingMode(std::string mode, SysMonitor *SysState){

	if(mode == "mps_static"){
		mSchedulerMode=MPS_STATIC;
	}
	else if(mode == "oracle"){
		mSchedulerMode=ORACLE;
		ALLOW_REPART=true;
	}
	else
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

void GlobalScheduler::setLoadBalancingMode(std::string mode){
	mLoadBalancer.setType(mode);
}

//added for incremental scheduling
std::vector<Task> GlobalScheduler::getListofTasks(SysMonitor *SysState){
	std::vector<std::string> models;
	std::vector<Task> ret_task;
	for(auto name : mNetNames){
		std::vector<std::string>::iterator it = find(models.begin(), models.end(),name);
		if(it == models.end()){	
			models.push_back(name);
		}
	}	
	for(auto name : models){
		Task new_task;
		new_task.id=SBPScheduler.getModelID(name);
		new_task.SLO=getSLO(name);
		new_task.request_rate=0; // this is OK since, there is no need for request rate to be present when getting saturated trp
		ret_task.push_back(new_task);
		std::cout << "[getListofTasks] task: " << new_task.id << ", SLO: "<< new_task.SLO << std::endl;
	}
	return ret_task;

}

std::vector<int> GlobalScheduler::getListofParts(SysMonitor *SysState){
	std::vector<int> ret_vec;
	for(int i=0; i<SysState->getNGPUs(); i++){
		for(auto proxy_info : *SysState->getDevMPSInfo(i)){
			int part = proxy_info->cap;
			if(part==0) continue;
			std::vector<int>::iterator it = find(ret_vec.begin(), ret_vec.end(),part);
			if(it == ret_vec.end()) ret_vec.push_back(part);
		}
	}
	assert(!ret_vec.empty());
	return ret_vec;
}

std::vector<std::vector<int>> GlobalScheduler::getParts(SysMonitor *SysState){
	std::vector<std::vector<int>> ret_vec;
	for(int i=0; i<SysState->getNGPUs(); i++){
		std::vector<int> tmp;
		for(auto proxy_info : *SysState->getDevMPSInfo(i)){
			int part = proxy_info->cap;
			if(part==0) continue;
			tmp.push_back(part);
		}
		ret_vec.push_back(tmp);
	}
	assert(!ret_vec.empty());
	for(auto vec: ret_vec) assert(!vec.empty());
	return ret_vec;
}

proxy_info* createNewProxy(int dev_id, int cap, int dedup_num, int part_index, std::string type){
	proxy_info* pPInfo = new proxy_info();
	pPInfo->dev_id = dev_id;
	pPInfo->cap = cap;
	pPInfo->dedup_num=dedup_num;
	pPInfo->duty_cycle=0; // THIS WILL BE OVERWRITTEN IN setupProxyTaskList
	pPInfo->partition_num = part_index;
	pPInfo->isBooted=false;
	pPInfo->isConnected=false;
	pPInfo->isSetup=false;
	pPInfo->type=type;
#ifdef DEBUG
	printf("[createNewProxy] create new proxy: [%d,%d,%d][%d] type: %s \n",dev_id, cap, dedup_num, part_index, type.c_str());
#endif
	return pPInfo;
}

void GlobalScheduler::setupSBPScheduler(SysMonitor *SysState, std::map<std::string,std::string> &param_file_list, std::string res_dir){
	setupPerModelLatencyVec(SysState);
	//for the moment we hardcode the name of files required
	std::vector<std::string> files = {"1_28_28.txt", "3_224_224.txt", "3_300_300.txt"};
	bool success = SBPScheduler.initializeScheduler(param_file_list["sim_config"], \
			param_file_list["mem_config"], \
			param_file_list["device_config"], \
			res_dir,\
			files);
	if(!success){
		std::cout<< __func__ <<": CRITICAL : failed to initialize scheduler!"
			<<std::endl;
		exit(1); 
	}

	SBPScheduler.setMaxGPUs(SysState->getNGPUs());

	//make proxy_info for every possible partitoin on every GPU
	std::vector<Task> list_of_tasks = getListofTasks(SysState);
	for (auto task : list_of_tasks){
		SBPScheduler.initSaturateTrp(task);
	}
}

void GlobalScheduler::setupScheduler(SysMonitor* SysState, std::map<std::string,std::string> &param_file_list, std::string scheduler, std::string res_dir ){
	if(setSchedulingMode(scheduler, SysState)){
		printf("exiting server due to undefined scheduling mode\n");
		exit(1);
	}	
#ifdef ChECK_NETBW
	SBPScheduler.setupNetworkChecker(res_dir+"/"+param_file_list["proxy_json"]);
#endif
	switch(mSchedulerMode){
		case MPS_STATIC:
			setupSBPScheduler(SysState,param_file_list,res_dir);
			setupMPSInfo(SysState, param_file_list["model_list"]);
			break;
		case ORACLE:
			setupSBPScheduler(SysState,param_file_list, res_dir);
			EPOCH_INTERVAL= 20000; // interval of checking rates, in ms,
			BEACON_INTERVAL=EPOCH_INTERVAL; // required in initReqRate
			HIST_LEN=30 / float(EPOCH_INTERVAL / 1000.0);
			break;
		default: // should not happen, conditions are already checked during initialization
			break;
	}
}

// create proxy_infos for 'ngpus' on node 'node_id' with configurable 'parts'
void GlobalScheduler::createProxyInfos(SysMonitor *SysState, std::vector<int> parts, int node_id, int ngpus, std::string type){
	//make proxy_info for every possible partitoin on every GPU
	std::map<int,int> perdev_part_idx;
	for(int i =0; i < SysState->getNGPUs()+ngpus; i++) perdev_part_idx[i]=0;
	for(int devindex = SysState->getNGPUs(); devindex < SysState->getNGPUs()+ngpus; devindex++ ){
		for(int part: parts){
			int dedup_num=0;
			proxy_info* pPInfo = createNewProxy(devindex,part,dedup_num,perdev_part_idx[devindex], type);
			pPInfo->node_id=node_id;
			SysState->insertMPSInfoToDev(devindex,pPInfo);
			perdev_part_idx[devindex]++;
			// if partition is 50, make another proxy, with dedup_num=1
			if(part == 50){ 
				dedup_num++;
				proxy_info* pPInfo = createNewProxy(devindex,part,dedup_num,perdev_part_idx[devindex], type);
				pPInfo->node_id=node_id;
				SysState->insertMPSInfoToDev(devindex,pPInfo);
				perdev_part_idx[devindex]++;
			}
		}
	}
	SysState->setNGPUs(SysState->getNGPUs()+ngpus);
}

void GlobalScheduler::setupInitTgtRate(std::string rate_file){
	std::cout << __func__ <<": reading " << rate_file
		<< std::endl;
	std::string str_buf;
	std::fstream fs;
	fs.open(rate_file, std::ios::in);
	if(!fs){
		std::cout << __func__ << ": file " << rate_file << " not found"
			<<std::endl;
		exit(1);
	}

	getline(fs, str_buf);
	int num_of_rates = stoi(str_buf);
	for(int j=0;j<num_of_rates;j++)
	{ 
		getline(fs,str_buf,',');
		int id=stoi(str_buf);

		getline(fs,str_buf,',');
		int request_rate=stoi(str_buf);

		getline(fs,str_buf,',');
		int SLO=stoi(str_buf);
		mPerTaskTgtRate[SBPScheduler.getModelName(id)]=request_rate;

	}  
	fs.close();
#ifdef EPOCH_DEBUG
	std::cout << "Rate setup ------------------------------------------------" << std::endl; 
	std::cout << "task name : [request_rate]" << std::endl;
	for(auto info_pair : mPerTaskTgtRate)
	{ 
		std::cout << info_pair.first << " : [" << info_pair.second << "]" << std::endl;
	}
	std::cout << std::endl;
	std::cout << "------------------------------------------------" << std::endl; 
#endif

}

void GlobalScheduler::setupLatencyModel(std::string latency_file){
	mLatencyModel.setupTable(latency_file);
}



void GlobalScheduler::setupMPSInfo(SysMonitor *SysState, std::string capfile){
	std::ifstream infile(capfile);
	std::string line;
	std::string token;
	std::string first_task;
	std::map<int, int> perdev_part_idx;
	for(int i =0; i < SysState->getNGPUs(); i++) perdev_part_idx[i]=0;
	while(getline(infile,line)){
		std::istringstream ss(line);
		getline(ss,token,',');
		int devindex=stoi(token);
		getline(ss,token,',');
		int cap=stoi(token);
		getline(ss,token,',');
		int dedup_num=stoi(token);
		getline(ss,token,',');
		proxy_info *pPInfo = getProxyInfo(devindex,cap,dedup_num,SysState);		
		bool found = false;
		//for(std::vector<proxy_info*>::iterator it = SysState->PerDevMPSInfo[devindex].begin(); 
		//		it != SysState->PerDevMPSInfo[devindex].end(); it++)
		for(auto it : *SysState->getDevMPSInfo(devindex))
		{
			if(it->cap == cap && it->dedup_num == dedup_num){
				found= true;
				break;
			}
		}
		if(!found){
			proxy_info* pPInfo = createNewProxy(devindex,cap,dedup_num,perdev_part_idx[devindex],pPInfo->type);
			perdev_part_idx[devindex]++;
			SysState->insertMPSInfoToDev(devindex,pPInfo);

		}
	}

}

// following is a one-time intiation code
void GlobalScheduler::loadAllProxys(SysMonitor *SysState){
	for(int i=0; i < SysState->getNGPUs(); i++){
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			// following is an async call
			loadModelstoProxy(pPInfo,SysState);
			// wait for 0.1 seconds(enough time to send load instruction to backend node)
			usleep(100*1000);
		}
	}
	for(int i=0; i < SysState->getNGPUs(); i++){
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			if(!SysState->isProxyNetListEmpty(pPInfo)) mProxyCtrl->waitProxy(pPInfo,RUNNING);
		}
	}
}

void GlobalScheduler::setupProxyTaskList(std::string specfile, SysMonitor *SysState){
#ifdef FRONTEND_DEBUG
	printf("[setup] setupProxyTaskList called for file: %s \n", specfile.c_str());
#endif 
	//throughput info to deliver to load balancer
	std::map<proxy_info*, std::vector<std::pair<int,double>>> mapping_trps;
	ProxyPartitionReader ppr = ProxyPartitionReader(specfile, SysState->getNGPUs());
	std::vector<task_config> list = ppr.getAllTaskConfig();
	for(std::vector<task_config>::iterator it = list.begin(); it != list.end(); it++){
		proxy_info* ptemp=getProxyInfo(it->node_id,it->thread_cap,it->dedup_num,SysState);
#ifdef DEBUG
		std::cout <<"[setup] name: " << it->name << ", batch_size: " << it->batch_size << std::endl;
#endif
		if(it->name=="reserved"){
			ptemp->isReserved=true;
			continue;
		}
		ptemp->isSchedulable=true;
		std::pair<std::string, int> t(it->name,it->batch_size);
		SysState->insertNetToProxyNetList(ptemp,t);
		//SysState->PerProxyTaskList[ptemp].push_back(t);
		ptemp->duty_cycle = it->duty_cycle;      
		// for load balancer, we also get throughput
		double duty_cycle = std::max(it->duty_cycle,getEstLatency(it->name,it->batch_size,it->thread_cap,ptemp->type));
		double trpt = it->batch_size * (1000 / it->duty_cycle);
		std::pair<int, double> t2(getModelID(it->name),trpt);
		mapping_trps[ptemp].push_back(t2);
	}
	setupLoadBalancer(mapping_trps);

//#ifdef FRONTEND_DEBUG
	printf("[setup] perProxyNetList configures as following: \n");
	for(int j=0; j<SysState->getNGPUs(); j++){
		//for(unsigned int k=0; k<SysState->PerDevMPSInfo[j].size(); k++){
		//	proxy_info *pPInfo = SysState->PerDevMPSInfo[j][k];
		for(auto pPInfo : *SysState->getDevMPSInfo(j)){
			printf("proxy[%d,%d,%d]: \n", pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num);
			for(auto it : *SysState->getProxyNetList(pPInfo)){
				printf("task: %s, batch: %d, duty_cycle: %lf \n", it.first.c_str(), it.second, pPInfo->duty_cycle);
			}
		}
	}
//#endif
}
void GlobalScheduler::setupPerModelLatencyVec(SysMonitor *SysState){ // this initiates mutex locks and std::deques for proxy/model latency vectors
#ifdef DEBUG
	printf("[SETUP] setting up per proxy/model mutex \n");
#endif
	const int INIT_SKIP_CNT = 100;
	for(uint64_t i=0; i<SysState->getNGPUs(); i++){
		//for(std::vector<proxy_info*>::iterator it = SysState->PerDevMPSInfo[i].begin(); it != SysState->PerDevMPSInfo[i].end(); it++){
		//	proxy_info *pPInfo = (*it);
		for(auto pPInfo : *SysState->getDevMPSInfo(i)){
			pPInfo->PerTaskLatencyVecMtx=new std::map<std::string,std::mutex*>();
			pPInfo->PerTaskLatencyVec=new std::map<std::string,std::deque<float>*>();
			pPInfo->PerTaskSkipCnt = new std::map<std::string, int>();
			for(std::vector<std::string>::iterator it = mNetNames.begin(); it != mNetNames.end(); it++){
				pPInfo->PerTaskLatencyVec->operator[](*it) = new std::deque<float>();
				pPInfo->PerTaskLatencyVecMtx->operator[](*it) = new std::mutex();
				pPInfo->PerTaskSkipCnt->operator[](*it) = INIT_SKIP_CNT;
			}
		}
	}
}

void GlobalScheduler::setupModelSLO(std::string name, int SLO)
{
	mPerModelSLO[name]=SLO;
#ifdef DEBUG
	printf("[SETUP] mPerModelSLO[%s]: %d \n", name.c_str(), SLO);
#endif
}

void GlobalScheduler::setMaxBatch(std::string name, std::string type, int max_batch)
{
	std::tuple<std::string, std::string> query = make_tuple(name, type);
	mMaxBatchTable[query]=max_batch;
}

void GlobalScheduler::setMaxDelay(std::string name, std::string type, int max_delay){
	std::tuple<std::string, std::string> query = make_tuple(name, type);
	mMaxDelayTable[query]=max_delay;

}

/*
void GlobalScheduler::initPerModelLatEWMA(SysMonitor *SysState, int monitor_interval_ms, int query_interval_ms){
	for(auto modelname: mNetNames){	
		PerTaskLatUpdateMtx[modelname]=new std::mutex();
		SysState->PerModelLatestLat[modelname]=0;
		mPerTaskEWMALat[modelname]=std::make_shared<EWMA::EWMA>(monitor_interval_ms, query_interval_ms);
	}
}
*/
void GlobalScheduler::initPerModelTrptEWMA(SysMonitor *SysState, int monitor_interval_ms, int query_interval_ms){
	for(auto modelname: mNetNames){
		PerTaskTrptUpdateMtx[modelname]=new std::mutex();
		SysState->setPerModelFinCnt(modelname,0);
		mPerTaskEWMATrpt[modelname]=std::make_shared<EWMA::EWMA>(monitor_interval_ms, query_interval_ms);
	}

}

void GlobalScheduler::initPerModelPartSum(SysMonitor *SysState, std::unordered_map<int,int> &sum_of_parts){
	for(auto gpu_ptr : mCurrState.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList){
			for(auto task_ptr : node_ptr->vTaskList){
				int model_id = task_ptr->id;
				if(sum_of_parts.find(model_id) == sum_of_parts.end()){
					sum_of_parts[model_id]=node_ptr->resource_pntg;
				}
				else{
					sum_of_parts[model_id]+=node_ptr->resource_pntg;
				}
			}
		}
	}
#ifdef ST_DEBUG
	std::cout << __func__ << ": all model's sum of part initated as following : "
		<<std::endl;
	for(std::unordered_map<int,int>::iterator it = sum_of_parts.begin(); it != sum_of_parts.end(); it++){
		std::cout <<"model id: " << it->first << " part:  " << it->second
			<<std::endl;
	}
#endif

}

void GlobalScheduler::setupBackendProxyCtrls(){
	int i=0;
	bool first=true;
	for(auto pair :  NodeIDtoBackendDelegate){
		if(first){
			mProxyCtrl = new BackendProxyCtrl();
			first=false;
		}
		mProxyCtrl->addBackendDelegate(pair.first, pair.second);
#ifdef FRONTEND_DEBUG
		std::cout << "intiated BackendProxyCtrl for node : " << pair.first << std::endl;
#endif
	}
}


void GlobalScheduler::setupLoadBalancer(std::map<proxy_info*, std::vector<std::pair<int,double>>> &mapping_trps){
#ifdef LB_DEBUG
	for(auto pair_info : mapping_trps){
		for(auto item : pair_info.second){
			std::cout << __func__ <<": pushing <" << item.first << ", " << item.second << "> to " << proxy_info_to_string(pair_info.first)
				<< std::endl;
		}
	}
#endif 


	if(mLoadBalancer.updateTable(mapping_trps)){
		std::cout << __func__ << ": ERROR when update load balancer table, exiting program"
			<<std::endl;
		exit(1);
	}
}


int GlobalScheduler::setupModelwithJSON(const char* AppJSON, std::string res_dir, GlobalScheduler &scheduler){
	Json::Value root;
	std::ifstream ifs;
	std::string full_name = res_dir + "/"+std::string(AppJSON);
#ifdef DEBUG
	printf("%s: Reading App JSON File: %s \n", __func__, full_name.c_str());
#endif 
	ifs.open(full_name);
	// fail check
	if(!ifs){
		std::cout << __func__ << ": failed to open file: " << full_name
			<<std::endl;
		exit(1);
	}

	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;
	if (!parseFromStream(builder, ifs, &root, &errs)) {
		std::cout << errs << std::endl;
		ifs.close();
		return EXIT_FAILURE;
	}

	for(unsigned int i=0; i < root["Models"].size();i++){
		scheduler.addtoNetNames(root["Models"][i]["model"].asString());
		if (root["Models"][i].isMember("max_batch"))
			scheduler.setMaxBatch(root["Models"][i]["model"].asString(), "gpu", root["Models"][i]["max_batch"].asInt());
		else
			scheduler.setMaxBatch(root["Models"][i]["model"].asString(), "gpu", DEFAULT_MAX_BATCH);   
		if (root["Models"][i].isMember("SLO")) scheduler.setupModelSLO(root["Models"][i]["model"].asString(), root["Models"][i]["SLO"].asInt());
	}
}