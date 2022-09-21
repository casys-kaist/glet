#include "scheduler_base.h"
#include "json/json.h"
#include "config.h"
#include "scheduler_utils.h"
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <glog/logging.h>

namespace Scheduling{
	/*
	static uint64_t getCurNs(){
		struct timespec ts; 
		clock_gettime(CLOCK_REALTIME, &ts);
		uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
		return t;
	}
	*/
	BaseScheduler::BaseScheduler(){}
	BaseScheduler::~BaseScheduler(){}
	bool BaseScheduler::initializeScheduler(std::string sim_config_json_file, \
			std::string mem_config_json_file,
			std::string device_config_json_file, \
			std::string res_dir, \
			std::vector<std::string> &batch_filenames){
		if(setupScheduler(sim_config_json_file)){
			std::cout << __func__ <<": failed in setting up " << sim_config_json_file
				<<std::endl;
			return false;
		}
		if(setupDevicePerfModel(device_config_json_file,res_dir)){
			std::cout << __func__ <<": failed in setting up " << device_config_json_file
				<<std::endl;
			return false;
		}
		if(setupPerModelMemConfig(mem_config_json_file)){
			std::cout << __func__ <<": failed in setting up " << mem_config_json_file
				<<std::endl;
			return false;

		}
		setupBatchLatencyTables(res_dir,batch_filenames);
		return true;
	}

	int BaseScheduler::getModelMemUSsage(int model_id){
		return _mapModelIDtoMemSize[model_id];
	}


} // Scheduling
