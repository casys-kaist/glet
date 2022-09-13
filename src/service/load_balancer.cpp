#include "load_balancer.h"
#include "common_utils.h"
#include <iostream>
#include <random>

#define EPOCH_IN_MILLIS 500
#define MIN_EPOCH 100
#define MAX_EPOCH 1000

// constants used for shifting when making keys
#define MODEL_ID_SHIFT 10000000
#define PART_SHIFT 1000
#define GPU_ID_SHIFT 10

LoadBalancer::LoadBalancer(){
	// turns off load balancing by default
	_type=NO;
}

LoadBalancer::~LoadBalancer(){}
void LoadBalancer::clearTables(){
	_TaskIDtoKeysMapping.clear();
	_keyToCreditMapping.clear();
	//_keyToFailMapping.clear();
	_keyToCurrCreditMapping.clear();
	_TaskIDToMinCreditMapping.clear();
}

void LoadBalancer::setType(std::string balancer_type){
}

// wrapper function: updates internal tables, returns whether successful or not 
int LoadBalancer::updateTable(SimState &sched_results){
	std::unique_lock<std::mutex> lk(_mtx);
	switch(_type){
		case NO:
			return EXIT_SUCCESS;
		case WRR:
			return updateWRR(sched_results);
		default: // should not happend
			exit(1);
	};
}


int LoadBalancer::updateTable(std::map<proxy_info *, std::vector<std::pair<int, double>>> &trps){
	std::unique_lock<std::mutex> lk(_mtx);
	if(_type == NO){
		return EXIT_SUCCESS;
	}
	// clear previous table
	clearTables();

	// for task-partition , allocate tickets
	for(auto pair_info : trps){
		for(auto item : pair_info.second){
			//checknCreateMtx(item.first);
			int key = getKey(item.first, pair_info.first);
			if(renewTable(item.first,key,item.second))
				return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

int LoadBalancer::renewTable(int model_id, int key, double trpt){
	_keyToCreditMapping[key] = 1/double(trpt);
#ifdef LB_DEBUG
	std::cout << "key: "<<key << "updated to " <<  _keyToCreditMapping[key]
		<<std::endl;
#endif

	_keyToCurrCreditMapping[key]=static_cast<double>(0.0);
	_TaskIDToMinCreditMapping[model_id]=static_cast<double>(10.0);
	_TaskIDtoKeysMapping[model_id].push_back(key);
	return EXIT_SUCCESS;

}

int LoadBalancer::updateWRR(SimState &sched_results){
// clear previous table

	clearTables();
	// for task-partition , allocate tickets
	for(auto gpu_ptr : sched_results.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList){
			for(auto task_ptr : node_ptr->vTaskList){
				//checknCreateMtx(task_ptr->id);
				int key = getKey(task_ptr->id, gpu_ptr->GPUID, node_ptr->resource_pntg, node_ptr->dedup_num);
				if(renewTable(task_ptr->id,key,task_ptr->throughput))
					return EXIT_FAILURE;
			}
		}
	}
	// check for abnormal results (for debugging)
	return EXIT_SUCCESS;
}

proxy_info* LoadBalancer::choseProxy(int model_id){
	proxy_info* ret_proxy = NULL;
	assert(ret_proxy != NULL);
	return ret_proxy;

}


// wrapper function: returns whether the proxy is supposed to take the task(model_name)
bool LoadBalancer::checkLoad(int model_id, proxy_info* pPInfo){
	std::unique_lock<std::mutex> lk(_mtx);
	switch(_type){
		case NO:
			return true;
		case WRR:
			return checkWRR(model_id,pPInfo);
		default: // should not happen
			exit(1);      
	};
	return false;
}

bool LoadBalancer::checkWRR(int model_id, proxy_info* pPInfo){
		int key = getKey(model_id,pPInfo);
	// DO NOT ERASE the following checking code.
	// the proxy thread sometimes call checkWRR even it is not supposed to execute the model
	// this happens during trasitions of scehduling period, just make it return FAIL
	auto it = _keyToCurrCreditMapping.find(key);
	if(it == _keyToCurrCreditMapping.end()){
		return EXIT_FAILURE;        
	}
	if(checkMinCredit(model_id,key)){
		// update model's credit, and keys currnet credit
		_keyToCurrCreditMapping[key]+=_keyToCreditMapping[key];
		_TaskIDToMinCreditMapping[model_id]=_keyToCurrCreditMapping[key];
		return true;
	} 
	// update fail count and allocate task
	return false;
}


int LoadBalancer::getKey(int model_id, proxy_info* pPInfo){
	return getKey(model_id, pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num);
}

// returns hash key
int LoadBalancer::getKey(int model_id, int gpu_id, int part, int dedup_num){
	// model_id: 0 ~ LARGE NUMBER
	// part: 20,40,50,60,80,100
	// gpu_id: 0~20 
	// dedup_num: 0~5 
	return MODEL_ID_SHIFT * model_id + PART_SHIFT * part +  GPU_ID_SHIFT * gpu_id + 1 * dedup_num;
}


bool LoadBalancer::containModelID(int key, int model_id){
	int _model_id = key / MODEL_ID_SHIFT;
	return model_id == _model_id;
}
int LoadBalancer::checknCreateMtx(int model_id){
	auto it = _TaskIDtoMtxMapping.find(model_id);
	if(it==_TaskIDtoMtxMapping.end()){
		_TaskIDtoMtxMapping[model_id]=new std::mutex();
	}
	return EXIT_SUCCESS;
}
// returns whether the credit hold by 'key' for model_id is smaller than min
bool LoadBalancer::checkMinCredit(int model_id, int key){
	return _keyToCurrCreditMapping[key] <= _TaskIDToMinCreditMapping[model_id];
}