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

	
} // namespace:Scheduling
