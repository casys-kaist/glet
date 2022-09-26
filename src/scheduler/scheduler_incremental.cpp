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

	bool cmp_task_dsc(const Task &a, const Task &b){
		return a.request_rate * a.SLO < b.request_rate * b.SLO || 
			((a.request_rate * a.SLO == b.request_rate *b.SLO)  && (a.id> b.id));
	}


	/*
	   bool cmp_task_dsc(const Task &a, const Task &b){
	   return a.request_rate < b.request_rate  || 
	   ((a.request_rate  == b.request_rate)  && (a.id> b.id));

	//    return a.SLO < b.SLO || 
	//        ((a.SLO == b.SLO)  && (a.id> b.id));
	}
	*/

	bool IncrementalScheduler::elasticPartitioning(std::vector<Task> &session, SimState &decision){
		sort(session.begin(), session.end(),cmp_task_dsc);
		for(auto task : session){
			// check if we have an available saturate table for task
			if (_perModelSatTable.find(task.id) == _perModelSatTable.end())
			{
				//if not, setup a table
				initSaturateTrp(task);
			}
			if(addModeltoSchedule(task, decision)){
#ifdef SCHED_DEBUG
				std::cout << "[incrementalScheduling] adding model failed!" << std::endl;
#endif
				return EXIT_FAILURE;
			}

#ifdef SCHED_DEBUG
			printResults(decision);
#endif
		}

	}

// stores value for saturating throughput of each available partition
	void IncrementalScheduler::initSaturateTrp(Task &task){

	}

	void IncrementalScheduler::estimateTrp(std::string device, Task &task, int rate, std::vector<NodePtr> &output_vec, const int MAX_PART){
	
	}




	
} // namespace:Scheduling
