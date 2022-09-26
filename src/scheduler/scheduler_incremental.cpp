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
#ifdef SCHED_DEBUG
		std::cout << "[initSaturateTrp] Called for task  " << task.id << std::endl;
#endif
		std::vector<SatTrpEntry> *new_table = new std::vector<SatTrpEntry>();
		Node temp_node;
		SimState dummy_sim;
		NodePtr temp_node_ptr=std::make_shared<Node>(temp_node);
		for(auto type_num_pair : _typeToNumofTypeTable){
			std::string type= type_num_pair.first;
			for(auto part : _availParts){
				temp_node_ptr->resource_pntg=part;
				temp_node_ptr->type=type;
				int batch_size = getMaxBatch(task,temp_node_ptr,dummy_sim,task.request_rate,/*is_residue=*/false,/*interfereence=*/false);
				// check if this is because 2*L(b=1) > SLO
				if (batch_size ==0){
					float latency = getLatency(type,task.id,1,temp_node_ptr->resource_pntg);
					if ( 2*latency > task.SLO) {
						batch_size=1;
#ifdef SCHED_DEBUG
						std::cout << "[initSaturateTrp] 2*L(b=1) > SLO, fixing batch size to 1 "<< std::endl;
#endif

					}
					else{
						std::cout << "[initSaturateTrp] cannot setup SaturateTable for task : " << task.id << std::endl;
						return;
					}
				}
				float latency = getLatency(type,task.id,batch_size,temp_node_ptr->resource_pntg);
				float trp = batch_size * 1000.0 / latency;
				SatTrpEntry new_entry;
				new_entry.max_batch=batch_size;
				new_entry.part=part;
				new_entry.sat_trp=trp;
				new_entry.type=type;
				new_table->push_back(new_entry);
#ifdef SCHED_DEBUG
				std::cout << "[initSaturateTrp] setted up resource pntg: " << new_entry.part <<", as trp:  "<< new_entry.sat_trp<< std::endl;
#endif
			}
		}
		_perModelSatTable[task.id]=new_table;
	}

	void IncrementalScheduler::estimateTrp(std::string device, Task &task, int rate, std::vector<NodePtr> &output_vec, const int MAX_PART){
		if(rate <=TRP_SLACK) return; // yes... if the rate is just too smaddll then return 
#ifdef SCHED_DEBUG
		printf("[estimateTrp] rate: %d called for model id: %d  \n",rate,task.id );
#endif 
		int temp_batch;
		int max_part;
		if(_useSelfTuning){
			getEstimateTrpST(device, task, rate, output_vec,MAX_PART);
			return;
		}
		else
			max_part = getMaxReturnPart(task,device);
		float limit = getMaxSaturateTrp(task, temp_batch,max_part, device);
#ifdef SCHED_DEBUG
		std::cout << "max_part: " << max_part << " limit: "<<limit << " rate: " << rate << std::endl;
#endif
		if(limit < rate){
			//allocate saturate node and residue node 
			int sat_part=0;
			float sat_trp=0;
			int sat_batch=0;

			sat_part=max_part;
			sat_trp=getMaxSaturateTrp(task,sat_batch,sat_part,device);

			// make saturate node and allocate task
			NodePtr temp_node_ptr = makeEmptyNode(output_vec.size(),sat_part,device); 
			TaskPtr temp_task_ptr = createNewTaskPtr(task.id,rate,task.SLO,sat_batch,sat_trp); 
			temp_node_ptr->occupancy=1;
			temp_node_ptr->type=device;
			temp_node_ptr->vTaskList.push_back(temp_task_ptr);
			temp_node_ptr->duty_cycle = getLatency(device, task.id,sat_batch,temp_node_ptr->resource_pntg);
			output_vec.push_back(temp_node_ptr);

			// recursive call 
			estimateTrp(device,task,rate-sat_trp, output_vec, MAX_PART);
		}
		else{ // this is it!! this time allocate residue node and return
			//NodePtr temp_node_ptr = makeEmptyNode(output_vec.size(),100);
			NodePtr temp_node_ptr = makeEmptyNode(output_vec.size(),max_part,device);
			int max_batch;
			int min_part = getMinPart(device, task, temp_node_ptr,rate,max_batch);
			temp_node_ptr->resource_pntg=min_part;
			float latency = getLatency(device, task.id,max_batch,min_part);
#ifdef SCHED_DEBUG
			printf("[estimateTrp]residue node- latency: %lf, batch_size: %d, rate: %d, part: %d  \n", latency, max_batch, rate, min_part );
#endif
			temp_node_ptr->duty_cycle= std::max(max_batch * (float(1000.0)  / rate), latency);
			temp_node_ptr->type = device;
			float trp = (max_batch * 1000.0) / temp_node_ptr->duty_cycle;
			TaskPtr temp_task_ptr = createNewTaskPtr(task.id,rate,task.SLO,max_batch,trp);
			temp_node_ptr->occupancy= latency / temp_node_ptr->duty_cycle;
			temp_node_ptr->vTaskList.push_back(temp_task_ptr);
			output_vec.push_back(temp_node_ptr);
		}
	}




	
} // namespace:Scheduling
