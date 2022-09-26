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

bool cmp_nodeptr_dsc(const NodePtr &a, const NodePtr &b){
	return a->resource_pntg > b->resource_pntg;
}


bool cmp_nodeptr_asc(const NodePtr &a, const NodePtr &b){
	return a->resource_pntg < b->resource_pntg;
}

bool isModelScheduled(Task &task, SimState &decision){
	bool ret_val = false;
	for(auto gpu_ptr : decision.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList){
			for(auto task_ptr: node_ptr->vTaskList){
				if(task.id == task_ptr->id) ret_val=true;
			}
		}
	}
	return ret_val;
}

bool IncrementalScheduler::addModeltoSchedule(Task &task, SimState &decision){
			bool split=false;
			bool good_to_go=false;
			bool failed=false;
			task.additional_rate=0;
			// allocate min part, until this task does not require more gpu-lets
			// allocate saturate
			int prev_rate=task.request_rate;
			std::vector<NodePtr> estimate_result;
			bool scheduled=isModelScheduled(task,decision);
#ifdef SCHED_DEBUG
			std::cout << __func__ << "model: " << task.id << " scheduled?: " << scheduled
				<<std::endl;
#endif
			if(!scheduled){
				bool goto_adjust=false;
				getEstimate(task, estimate_result, getMaxPartSize(decision));
				//sort(estimate_result.begin(), estimate_result.end(),cmp_nodeptr_asc);
				sort(estimate_result.begin(), estimate_result.end(),cmp_nodeptr_dsc);
				if(!checkFit(estimate_result,decision)){
					if(allocateFit(estimate_result,task,decision)){
						goto_adjust=true;
					}
				}
				else goto_adjust=true;
#ifdef SCHED_DEBUG
				std::cout << "AFTER checkFit: " << std::endl;
				printResults(decision);
#endif

				if(goto_adjust){ // if greedy fit failed
					// get empty partitions
					std::vector<NodePtr> empty_nodes;
					for(auto gpu_ptr : decision.vGPUList){
						for(auto node_ptr: gpu_ptr->vNodeList){
							if(node_ptr->vTaskList.empty()){
								empty_nodes.push_back(node_ptr);
							}
						}
					}
					if( !empty_nodes.empty()){
						// sort empty_nodes in descending order with respect to pntg
						sort(empty_nodes.begin(), empty_nodes.end(), cmp_nodeptr_dsc);
					}
					readjust(task,empty_nodes, decision);
#ifdef SCHED_DEBUG
					std::cout << "AFTER readjust: " << std::endl;
					printResults(decision);
#endif 

					if (!mergeResidue(task,decision)) return EXIT_SUCCESS;
#ifdef SCHED_DEBUG
					std::cout << "AFTER mergeResidue: " << std::endl;
					printResults(decision);
#endif

					failed=true;
					if(!_useSelfTuning){
						std::vector<NodePtr> empty_vec;
						empty_vec.clear(); // just to make sure
						if(!allocateTimeShare(task,decision, empty_vec)){
							failed=false;
						}
					}
					if(failed) return EXIT_FAILURE;
				}
			} // scheduled
			else{ // if this task was previously scheduled
				bool failed = false;
				std::vector<NodePtr> target_nodes;
				// nodes used when time sharing
				std::vector<NodePtr> ts_target_nodes;
				// Retrieve and clear nodesa
				// when clearing nodes that time share-task, be sure to just erase task only and not add to target_nodes
				std::cout << "AFTER SCHEDULED START: " << std::endl;
				printResults(decision);
				for(auto gpu_ptr : decision.vGPUList){
					for(auto node_ptr: gpu_ptr->vNodeList){
						printNodeInfo(node_ptr);
						if(node_ptr->vTaskList.empty()) target_nodes.push_back(node_ptr);
						else{
							bool found=false;
							for(int i =0; i < node_ptr->vTaskList.size(); i++){
								if(node_ptr->vTaskList[i]->id == task.id){
									node_ptr->vTaskList.erase(node_ptr->vTaskList.begin()+i);
									ts_target_nodes.push_back(node_ptr);
									found=true;
									break;
								}
							}
							// update occupancy
							float latency_sum=0;
							for(auto task_ptr: node_ptr->vTaskList){
								latency_sum += getLatency(node_ptr->type,task_ptr->id,task_ptr->batch_size,node_ptr,decision);
							}
							node_ptr->occupancy = (latency_sum / node_ptr->duty_cycle);
							// if "task" was the only task scheduled then add, if not then just continue(and leave it to allocateTimeShare if neccessasry)
							if(node_ptr->vTaskList.empty()) target_nodes.push_back(node_ptr); 
						}
					}
				}
				if(!target_nodes.empty()) sort(target_nodes.begin(), target_nodes.end(), cmp_nodeptr_dsc);

#ifdef SCHED_DEBUG
				std::cout <<"task: "<< task.id << " Len of target nodes: " << target_nodes.size() <<std::endl;
				std::cout <<"task: "<< task.id << " Len of ts target nodes: " << ts_target_nodes.size() <<std::endl;
				for(auto n_ptr : target_nodes){
					std::cout << "[ " << n_ptr->id << "," << n_ptr->resource_pntg << "," <<n_ptr->dedup_num<< "]" <<std::endl;
				}
				std::cout << "--------" << std::endl;
				if(!ts_target_nodes.empty())
					for(auto n_ptr : ts_target_nodes){
						std::cout << "[ " << n_ptr->id << "," << n_ptr->resource_pntg << "," <<n_ptr->dedup_num<< "]" <<std::endl;
					}

#endif 
				readjust(task,target_nodes,decision);
				std::cout << "AFTER SCHEDULED readjust: " << std::endl;
				printResults(decision);
				if(!mergeResidue(task,decision)) return EXIT_SUCCESS;
				std::cout << "AFTER SCHEDULED mergeResidue: " << std::endl;
				printResults(decision);

				failed=true;
				if(!_useSelfTuning){
					if(!allocateTimeShare(task,decision, ts_target_nodes)){
						failed=false;
					} 
				}
				if(failed) return EXIT_FAILURE;

			} 
			return EXIT_SUCCESS;
		}



	
} // namespace:Scheduling
