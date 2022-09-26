#include "scheduler_base.h"
#include "scheduler_incremental.h"
#include "scheduler_utils.h"
#include "network_limit.h"
#include <assert.h>
#include <glog/logging.h>
#include <algorithm>
#include <iostream>
#include <math.h>
#include "config.h"


namespace Scheduling{
	const int TRP_SLACK=15;
	IncrementalScheduler::IncrementalScheduler(){}
	IncrementalScheduler::~IncrementalScheduler(){}

	bool IncrementalScheduler::runScheduling(std::vector<Task> *task_list, SimState &prev_output, SimState &new_output, bool allow_repart){
		if(allow_repart) _useRepartition=true;		
		else _useRepartition=false;
		if(incrementalScehduling(*task_list, prev_output)){
			return EXIT_FAILURE;
		}
		copyToOutput(prev_output,new_output);
		return EXIT_SUCCESS;
	}

	float min(const float a, const float b){
		return (a<=b) ? a : b;
	}

	bool checkContain(const int model_num, const NodePtr &node_ptr){
		for(auto task_ptr : node_ptr->vTaskList){
			if(model_num == task_ptr->id) return 1;
		}
		return 0;
	}

	bool cmp_nodeptr_occu_dsc(const NodePtr &a, const NodePtr &b){
		return a->occupancy > b->occupancy;
	}


	
} // namespace:Scheduling
