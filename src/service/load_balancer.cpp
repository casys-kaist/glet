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
}


int LoadBalancer::updateTable(std::map<proxy_info *, std::vector<std::pair<int, double>>> &trps){
}

int LoadBalancer::renewTable(int model_id, int key, double trpt){

}

int LoadBalancer::updateWRR(SimState &sched_results){
}

proxy_info* LoadBalancer::choseProxy(int model_id){
}


// wrapper function: returns whether the proxy is supposed to take the task(model_name)
bool LoadBalancer::checkLoad(int model_id, proxy_info* pPInfo){
}

bool LoadBalancer::checkWRR(int model_id, proxy_info* pPInfo){
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

int LoadBalancer::checknCreateMtx(int model_id){
}
// returns whether the credit hold by 'key' for model_id is smaller than min
bool LoadBalancer::checkMinCredit(int model_id, int key){
}
