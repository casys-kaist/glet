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

	bool IncrementalScheduler::incrementalScehduling(std::vector<Task> &session, SimState &decision){
		if(decision.vGPUList.empty()){
			initiateDevs(decision,_numMaxGPU);
		}
		std::map<int,float> task_to_org_rate_mapping;
		std::map<int,float> task_to_intf_retry_flag;
		if(_useInterference){
			for(auto task : session){
				task_to_intf_retry_flag[task.id]=false;
			}
		}
		bool good_to_go=false;
		while(!good_to_go){
			task_to_org_rate_mapping.clear();
			for(auto task : session){
				task_to_org_rate_mapping[task.id]=task.request_rate;
			}
			if(elasticPartitioning(session,decision)){
				return EXIT_FAILURE;
			}
			good_to_go=true;
			//check how much leftover requests were  stored after adjusting batches

			if(_useInterference){
				// 1. addup all throughputs for all tasks
				std::map<int,float> task_to_trpt;
				for(auto gpu_ptr : decision.vGPUList){
					for(auto node_ptr : gpu_ptr->vNodeList){
						for(auto task_ptr : node_ptr->vTaskList){
							task_to_trpt[task_ptr->id]+=task_ptr->throughput;
						}
					}
				}
				std::vector<Task> new_session;
				for(auto task:  session){
					const float INTF_THRESHOLD=0.85;
#ifdef SCHED_DEBUG
					std::cout << "[incrementalScheduling]: task_id: " << task.id << " task_trpt: "<<  task_to_trpt[task.id] << " task_org_rate: " << task_to_org_rate_mapping[task.id]
						<<std::endl;
#endif
					if(task_to_trpt[task.id] < task_to_org_rate_mapping[task.id] * INTF_THRESHOLD){
						if(task_to_intf_retry_flag[task.id]) return EXIT_FAILURE;
#ifdef SCHED_DEBUG
						std::cout << "[incrementalScheduling]: task_id: " << task.id << " remaining rate: " << task_to_org_rate_mapping[task.id] - task_to_trpt[task.id]
							<<std::endl;    
#endif
						task_to_intf_retry_flag[task.id]=true;
						good_to_go=false;
						// create new task in new task vector
						Task new_task;
						new_task.SLO=task.SLO;
						new_task.id=task.id;
						//new_task.request_rate=task_to_org_rate_mapping[task.id]*INTF_THRESHOLD;
						new_task.request_rate=task_to_org_rate_mapping[task.id];
						new_session.push_back(new_task);
					}
				}
				session.clear();
				copySession(new_session,session);
			}// _useInterference

		}// good_to_go

		// try scheduling without tightening batch size
		// uncomment this if tightening deems neccessary 
		/*  
		    for(auto task : session){
		    residueTightening(task, decision);
		    }
		    */
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
	void IncrementalScheduler::initSaturateTrp(Task &task)
	{
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
	// Time shares nodes, if ts_node_list is not empty this function will only consider nodes that are in the vector
	bool IncrementalScheduler::allocateTimeShare(Task &task, SimState &sim, std::vector<NodePtr> &ts_node_list){
#ifdef SCHED_DEBUG
		printf("[allocateTimeShare] called for task id: %d , rate: %d \n",task.id, task.request_rate );
#endif
		bool checkfirst=true;
		if(ts_node_list.empty()) checkfirst=false;
		std::vector<NodePtr> candidates;
		for(auto gpu_ptr : sim.vGPUList){
			//check whether there is room for task in GPU first
			for(auto node_ptr : gpu_ptr->vNodeList)
			{
#ifdef CHECK_MEM
				if(!doesFitMemLimit(gpu_ptr,task.id,node_ptr)) continue;
#endif
				if(checkfirst){ // search for node in ts_node_list and add pointer if and only if the node is in list
					bool found = false;
					for(auto node_ptr2 : ts_node_list){
						if(node_ptr->id == node_ptr2->id && node_ptr->dedup_num == node_ptr2->dedup_num && node_ptr->resource_pntg == node_ptr2->resource_pntg){
							found=true;
							break;
						}
					}
					if(found) candidates.push_back(node_ptr);
				}
				else{
					if(node_ptr->occupancy < 1 && node_ptr->vTaskList.size() < _numMaxModel) candidates.push_back(node_ptr);
				}
			}
		}

		sort(candidates.begin(), candidates.end(), cmp_nodeptr_occu_dsc);
		while(task.request_rate + task.additional_rate> TRP_SLACK){
			bool found = false;
			for(auto node_ptr : candidates)
			{
				if(checkContain(task.id, node_ptr)) continue;
				if(node_ptr->occupancy >= 1.0) continue;
#ifdef SCHED_DEBUG
				printf("[allocateTimeShare] occupancy: %lf \n" , node_ptr->occupancy);
#endif
				int max_batch = int((node_ptr->duty_cycle) * (task.request_rate / 1000.0));
#ifdef SCHED_DEBUG
				printf("[allocateTimeShare] max batch: %d for duty cycle: %lf, rate: %d \n", max_batch,node_ptr->duty_cycle, task.request_rate);
#endif 
				if(max_batch==0) continue;

				int local_batch_size;
				int duty_cycle;
				float latency;
				//get possible maximum batch size, (which leads to maximum througput)
				if(max_batch > _MAX_BATCH) max_batch=_MAX_BATCH;
				for(local_batch_size = max_batch; local_batch_size > 0; --local_batch_size){
					duty_cycle = local_batch_size * 1000.0  / (task.request_rate);
					latency = getLatency(node_ptr->type,task.id,local_batch_size,node_ptr, sim);
					if(duty_cycle + latency < task.SLO && (latency / duty_cycle) < (1.0 - node_ptr->occupancy)){
						break;
					}

				}
				if(duty_cycle > node_ptr->duty_cycle) continue;
				if(local_batch_size==0) continue;

#ifdef SCHED_DEBUG
				printf("[allocateTimeShare] duty cycle before: %lf, after : %d, batch_size: %d \n", node_ptr->duty_cycle, duty_cycle, local_batch_size);
				printf("[AllocateTineShare] latency: %lf ms , occupancy: %lf \n", latency,latency/duty_cycle);
#endif

				std::vector<int> SLOs;
				float latency_sum=0;
				bool skip=false;
				//first gather latency and SLO of new task
				latency_sum += latency;
				SLOs.push_back(task.SLO);
#ifdef SCHED_DEBUG
				printf("[allocateTimeShare] latency_sum: %lf, duty_cycle : %d, batch size: %d \n",latency_sum,duty_cycle, local_batch_size);
#endif
				// then gather latency and SLO of old tasks 
				for(auto task_ptr : node_ptr->vTaskList){
					int batch_size = int(duty_cycle / (1000.0 / task_ptr->request_rate));
					if(batch_size==0) batch_size=1;
					latency_sum += getLatency(node_ptr->type,task_ptr->id, batch_size, node_ptr, sim);
					SLOs.push_back(task_ptr->SLO);
#ifdef SCHED_DEBUG
					printf("[allocateTimeShare] latency_sum: %lf, duty_cycle : %d, batch_size: %d \n",latency_sum,duty_cycle, batch_size);
#endif
				}
				if(latency_sum / duty_cycle > 1.0) continue;
				latency_sum+=duty_cycle;
				for(auto slo : SLOs)
				{
					if(latency_sum > slo) {
						skip=true;
#ifdef SCHED_DEBUG
						printf("[AllocateTineShare] latency sum %lf exceeded slo: %d \n", latency_sum, slo);
#endif
					}
				}
				
				if(skip) continue;
				//update node
				max_batch=local_batch_size;
				float local_throughput = max_batch * (1000.0 / duty_cycle);
				node_ptr->vTaskList.push_back(createNewTaskPtr(task.id,task.request_rate,task.SLO,max_batch,local_throughput));
				// update remaining request rate left for task
				task.request_rate =  (task.request_rate - local_throughput >0)  ? task.request_rate - local_throughput : 0;
#ifdef CHECK_MEM
				addGPUMemUsage(sim.vGPUList[node_ptr->id],task.id,node_ptr);
#endif
				float new_occupancy=0.0;
				for(auto task_ptr : node_ptr->vTaskList){
					if(task_ptr->id == task.id){
						new_occupancy += getLatency(node_ptr->type,task_ptr->id,task_ptr->batch_size,node_ptr,sim) / duty_cycle;
#ifdef SCHED_DEBUG
						printf("[allocateTimeShare] new_occupancy: %lf , after adding %d \n",new_occupancy, task_ptr->id); 
#endif
					}
					else{
						task_ptr->batch_size = int(duty_cycle / (1000.0 / task_ptr->request_rate));
						if(task_ptr->batch_size==0) task_ptr->batch_size=1;
						if(task_ptr->batch_size>_MAX_BATCH) task_ptr->batch_size=_MAX_BATCH;
						task_ptr->throughput = (task_ptr->batch_size * 1000.0) / duty_cycle;
						new_occupancy += getLatency(node_ptr->type,task_ptr->id, task_ptr->batch_size, node_ptr,sim) / duty_cycle;
#ifdef SCHED_DEBUG
						printf("[allocateTimeShare] new_occupancy: %lf , after adding %d \n",new_occupancy, task_ptr->id); 
#endif
					}
				}
				node_ptr->duty_cycle=duty_cycle;
				node_ptr->occupancy=new_occupancy;
				found=true;
#ifdef SCHED_DEBUG
				node_ptr->id;
				printf("[AllocateTineShare] SUCCESS task : %d allocated to [%d,%d,%d] with trp: %lf, ending occupancy: %lf \n", task.id, \
						node_ptr->id, node_ptr->resource_pntg, node_ptr->dedup_num,\
						local_throughput,\
						node_ptr->occupancy);
#endif
				break;
			} // for: candidates
			if(!found) return EXIT_FAILURE; // faied to allocate with time sharing 
#ifdef SCHED_DEBUG
			printf("[allocateTimeShare] remaining rate: %d \n",task.request_rate);
#endif
		} // tsak.request_rate  
		return EXIT_SUCCESS;
	}

	void IncrementalScheduler::getEstimate(Task &task, std::vector<NodePtr> &output_vec, const int MAX_PART){
#ifdef SCHED_DEBUG
			std::cout << "[getEstimate] function called for task id : " << task.id<< "rate: " << task.request_rate<< std::endl;
			std::cout << "[getEstimate] max_part : " << MAX_PART<< std::endl;
#endif
			std::string most_cost_effective_type;
			getMinPartSum(task,most_cost_effective_type,MAX_PART);
			//getOneFirst(task,_most_cost_effective_type,MAX_PART);
			output_vec.clear();
			estimateTrp(most_cost_effective_type, task,task.request_rate,output_vec, MAX_PART);
	}



	void IncrementalScheduler::getEstimateTrpST(std::string device, const Task &task, int rate, std::vector<NodePtr> &output_vec, const int MAX_PART)
		{
			int _rate = rate;
			std::vector<int> parts = _availParts;
			sort(parts.begin(), parts.end());

			// 1. find min part which can satisfy rate
			while (_rate > 0)
			{
				int max_batch;
				int max_trp = 0;
				int max_part = 0;
				//
				for (int part : parts)
				{
					max_part = part;
					max_trp = getMaxSaturateTrp(task, max_batch, part, device);
					if (max_trp > _rate)
						break;
				}
				assert(max_trp != 0 && max_part != 0);
				NodePtr temp_node_ptr = makeEmptyNode(output_vec.size(), max_part, device);
				float latency = getLatency(device, task.id, max_batch, max_part);
#ifdef SCHED_DEBUG
				printf("[EstimateTrpST]node- latency: %lf, batch_size: %d, rate: %d, part: %d  \n", latency, max_batch, _rate, max_part );
#endif
				temp_node_ptr->duty_cycle= max_batch * (1000.0  / _rate);
				Task new_task;
				new_task.id=task.id;
				new_task.request_rate=_rate;
				new_task.batch_size=max_batch;
				new_task.SLO=task.SLO;
				new_task.throughput=max_trp;
				TaskPtr temp_task_ptr = std::make_shared<Task>(new_task);          
#ifdef SCHED_DEBUG
				printf("[EstimateTrpST] batch_size after allocating : %d, trpt: %lf \n", temp_task_ptr->batch_size, temp_task_ptr->throughput);
#endif
				temp_node_ptr->occupancy= latency / temp_node_ptr->duty_cycle;
				temp_node_ptr->vTaskList.push_back(temp_task_ptr);
				output_vec.push_back(temp_node_ptr);
				_rate-=max_trp;
			}

		}
		// receives task, and resource percentage the task will run as input and stores the maximum batch size it can support
	// returns maximum throughput for that throughput
	float IncrementalScheduler::getMaxSaturateTrp(const Task &task, int &output_batch, const int resource_pntg, std::string type){

		bool found=false;
		float trp;
		for(auto entry : *_perModelSatTable[task.id]){
			if(resource_pntg == entry.part && type == entry.type)
			{
				found=true;
				trp=entry.sat_trp;
				output_batch=entry.max_batch;
			}
		}
		assert(found); // if this is not found we have a problem
		return trp;
	}

	void IncrementalScheduler::setupNetworkChecker(std::string json_file){
			if(_NLC.setupPerTaskInputDimension(json_file)){
				_isNLCInitated=false;
			}
			else _isNLCInitated=true;
	}

	bool IncrementalScheduler::inspectNetworkBW(SimState &input){
			return _NLC.isBandwidthOK(input);
	}

	int IncrementalScheduler::getMaxReturnPart(const Task& task, std::string device){
#ifdef SCHED_DEBUG
			std::cout << "[getMaxReturnPart] received model id: " << task.id << std::endl; 
#endif 
			std::map<int,float> per_part_trp;
			int dummy;
			// get throughput of every possible partition
			assert(_availParts.size()>=1);
			if(_availParts.size() == 1) return *_availParts.begin();
			for(auto part : _availParts){
				per_part_trp[part]=getMaxSaturateTrp(task,dummy,part,device);
			}
			const float MIN_THRESHOLD=1.05;\
						  uint len = _availParts.size();
			float max_ret = 0;
			int max_part = _availParts[len-1];
#ifdef SCHED_DEBUG
			std::cout << "max_part initated to " << per_part_trp.end()->first << std::endl;  
#endif
			for(int i=len-1; i > 1; i--){
				int low_part=_availParts[i];
				int high_part=_availParts[i-1];
				float low_trp=per_part_trp[low_part];
				float high_trp=per_part_trp[high_part];

#ifdef SCHED_DEBUG
				//std::cout << "low_part: "<< low_part << " high_part: "<< high_part << " low_trp: "<< low_trp << " high_trp: " << high_trp<<endl;
#endif 

				if(high_trp/low_trp < MIN_THRESHOLD) continue;
				float ret = (high_trp-low_trp)/(high_part-low_part);
				if( ret > max_ret){
					max_ret=ret;
					max_part=high_part;
				} 
			}
#ifdef SCHED_DEBUG
			std::cout << "[getMaxReturnPart] returning max part:  " << max_part<< std::endl; 
#endif 

			return max_part;
		}
int IncrementalScheduler::getMinPart(std::string device, Task task, const NodePtr node_ptr, int &residue_rate ,int &result_batch){
		int max_part = 200;
		int given_pntg;
		int given_id;
		SimState dummy_sim;
		if(node_ptr == NULL){
			given_id=0;
			given_pntg=100;
		}
		else{
			given_id=node_ptr->id;
			given_pntg=node_ptr->resource_pntg;
		}
#ifdef SCHED_DEBUG
		std::cout << "[getMinPart] model_id: " << task.id << " given_pntg: " << given_pntg <<std::endl;
#endif
		int local_max_part = min(_availParts.front(),given_pntg);
#ifdef SCHED_DEBUG
		std::cout << "[getMinPart]  local_max_part: "  << local_max_part <<std::endl;
#endif

		Node temp_node;
		NodePtr temp_node_ptr=std::make_shared<Node>(temp_node);
		temp_node_ptr->id=given_id;
		temp_node_ptr->resource_pntg=local_max_part;
		temp_node_ptr->type = node_ptr->type;

		int max_batch = getMaxBatch(task,temp_node_ptr,dummy_sim,residue_rate,true,false);
#ifdef SCHED_DEBUG
		std::cout << "[getMinPart]  max_batch "  << max_batch <<std::endl;
#endif
		if(!max_batch) return 0;

		assert(_MIN_BATCH <= max_batch && max_batch <= _MAX_BATCH);
		int prev_part=local_max_part;
		for(auto part : _availParts){ // starting from highest partition
			if(part > local_max_part) continue;
			float latency = getLatency(device, task.id,max_batch,part);
			float duty_cycle = max_batch * 1000.0 / residue_rate;
#ifdef SCHED_DEBUG
			std::cout << "[getMinPart] part: " << part << "latency: " << latency << " duty_cycle: " << duty_cycle << " SLO: "<< task.SLO << std::endl;
#endif
			if(duty_cycle < latency){

				if(part == _availParts[0]){
					prev_part=part;
				}
				break; 
			}
			if(task.SLO < latency + duty_cycle){
				break;
			} 
			prev_part=part;
#ifdef SCHED_DEBUG
			std::cout << "prev_part: " << prev_part << std::endl;
#endif

		}
		result_batch=max_batch;
		max_part=prev_part;
#ifdef SCHED_DEBUG
		printf("[getMinPart] min_part: %d, for task id: %d \n", max_part,task.id);
#endif
		return max_part;
	}
	bool IncrementalScheduler::addGPUMemUsage(GPUPtr &gpu_ptr, int model_id, NodePtr &node_ptr){
#ifdef SCHED_DEBUG
			std::cout << __func__ << "called"  << std::endl;
#endif 
			int additional_mem = 0;
			if(isModelLoaded(gpu_ptr, node_ptr, model_id)) return EXIT_SUCCESS;        
			if(!isPartLoaded(gpu_ptr,node_ptr)){
				additional_mem=_DEFAULT_PYTORCH_MEM_USAGE;
			}
			if(doesFitMemLimit(gpu_ptr, model_id,node_ptr)){
				gpu_ptr->used_memory += (_mapModelIDtoMemSize[model_id] + additional_mem);
#ifdef SCHED_DEBUG
				std::cout << "gpu" << gpu_ptr->GPUID <<  " used_memory updated to " << gpu_ptr->used_memory << std::endl;
				std::cout << "gpu" << gpu_ptr->GPUID <<  " remaining memory: " << gpu_ptr->TOTAL_MEMORY - gpu_ptr->used_memory << std::endl;
#endif 
				MemNode temp;
				temp.dedup_num=node_ptr->dedup_num;
				temp.part=node_ptr->resource_pntg;
				gpu_ptr->vLoadedParts.push_back(temp);
#ifdef SCHED_DEBUG
				std::cout << "gpu" << gpu_ptr->GPUID <<  " loaded parts: ";
				for(auto mem_info : gpu_ptr->vLoadedParts){
					std::cout << "["<<mem_info.part << ", " << mem_info.dedup_num<< "]";
				}
				std::cout<<std::endl;
#endif
				return EXIT_SUCCESS;
			}
			return EXIT_FAILURE;
		}

	bool IncrementalScheduler::subtractGPUMemUsage(GPUPtr &gpu_ptr, int model_id, NodePtr &node_ptr){
#ifdef SCHED_DEBUG
			std::cout << __func__ << "called"  << std::endl;
#endif
			int additional_mem = 0;
			// if part is not even loaded... this is a huge error on the programmer's behalf
			assert(isPartLoaded(gpu_ptr, node_ptr));
			// if model is not loaded, no need to substract
			if (!isModelLoaded(gpu_ptr, node_ptr, model_id))
				return EXIT_SUCCESS;
			if (gpu_ptr->used_memory - _mapModelIDtoMemSize[model_id] < 0)
			{
				std::cout << __func__ << ": gpu " << gpu_ptr->GPUID << " used memory: " << gpu_ptr->used_memory << std::endl;
				std::cout << __func__ << ": CAN NOT subtract" << _mapModelIDtoMemSize[model_id] << std::endl;
				return EXIT_FAILURE;
			}
			gpu_ptr->used_memory -= _mapModelIDtoMemSize[model_id];
#ifdef SCHED_DEBUG
			std::cout << "gpu" << gpu_ptr->GPUID <<  "used_memory updated to " << gpu_ptr->used_memory << std::endl;
			std::cout << "gpu" << gpu_ptr->GPUID <<  "remaining memory: " << gpu_ptr->TOTAL_MEMORY - gpu_ptr->used_memory << std::endl;
#endif 
			return EXIT_SUCCESS;
		}
	bool IncrementalScheduler::doesFitMemLimit(GPUPtr &gpu_ptr, int model_id, NodePtr &node_ptr){
			assert(_mapModelIDtoMemSize[model_id] !=0);
			assert(gpu_ptr->TOTAL_MEMORY);
			int additional_memory=0;
			// if model is already loaded, part is also loaded no need to even check
			if(isModelLoaded(gpu_ptr, node_ptr, model_id)) return true;
			if(!isPartLoaded(gpu_ptr,node_ptr)){
				additional_memory=_DEFAULT_PYTORCH_MEM_USAGE;
			}
			bool result= ((gpu_ptr->TOTAL_MEMORY)*_MEM_ROOM - gpu_ptr->used_memory) > _mapModelIDtoMemSize[model_id] + additional_memory; 

#ifdef SCHED_DEBUG
			std::cout << __func__ << "called for gpu: " << gpu_ptr->GPUID << " searching for " << node_ptr->resource_pntg <<std::endl;
			std::cout << __func__ << ": comparing left memory: " << gpu_ptr->TOTAL_MEMORY*_MEM_ROOM - gpu_ptr->used_memory << " and model_mem: "<< _mapModelIDtoMemSize[model_id] << std::endl;
			std::cout << __func__ << ": with addtional " << additional_memory << std::endl; 
#endif

#ifdef SCALE_DEBUG
			if(!result){
				std::cout << __func__ << ": task id " << model_id << " DOES NOT fit on GPU " << gpu_ptr->GPUID
					<<std::endl;
			}
			else{
				std::cout << __func__ << ": task id " << model_id << " DOES fit on GPU " << gpu_ptr->GPUID
					<<std::endl;
			}

#endif
			return result;
	}

	// readjusting algorithm: migrates tasks and changes partition
		//if possible, try to squeeze taks into smaller partitions for given nodes
		bool IncrementalScheduler::readjust(Task &task, std::vector<NodePtr> &given, SimState &decision){
			int request_rate = task.request_rate;
			int max_batch;
			bool proceed_to_residue=false;
			sort(given.begin(), given.end(),cmp_nodeptr_dsc);
			if(_useInterference){
				task.request_rate += task.additional_rate;
				task.additional_rate=0;
			}
#ifdef SCHED_DEBUG
			printf("[readjust] Called for task id : %d and rate %d \n", task.id, task.request_rate);
#endif

			// readjusted saturate scheduling
			while(task.request_rate+task.additional_rate> TRP_SLACK){
#ifdef SCHED_DEBUG
				std::cout << "request_rate: " << task.request_rate << ", additional rate: " << task.additional_rate << std::endl; 
#endif
				if(given.empty()) break;
				NodePtr temp_node_ptr;

				//added 2022-06-11 
				//check if we need to find high-prioirity types
				std::string type_to_prioritize;
				if(checkTypePriority(type_to_prioritize)){
					bool successful=false;
					//if so find the node with prioritized type, if there are no more nodes just chose front 
					if(getNodewithPriorityType(task,type_to_prioritize,temp_node_ptr,given)){
						temp_node_ptr=given.front();
					}
				} 
				else{
					getMinPartSum(task,type_to_prioritize,getMaxPartSize(decision));
					if(getNodewithPriorityType(task,type_to_prioritize,temp_node_ptr,given)){
						temp_node_ptr=given.front();
					}
				}
				int temp_batch;
				// if allocated with 100% node and required node is below 100%, split it for further use
				if(temp_node_ptr->resource_pntg == 100 && _usePart && _useRepartition){
					std::cout << "Following node will be splitted" << std::endl;
					printNodeInfo(temp_node_ptr);
					std::vector<NodePtr> candidates;
					const float  MAX_PART = getMaxPartSize(decision);
					estimateTrp(temp_node_ptr->type,task,task.request_rate,candidates,MAX_PART);
					assert(!candidates.empty());
					int max_part = candidates[0]->resource_pntg;
					assert(candidates[0]->vTaskList.size() ==1);
					temp_batch=candidates[0]->vTaskList[0]->batch_size;
#ifdef SCHED_DEBUG
					std::cout << "[readjust] Splitting node into " << max_part << " and " << 100-max_part << std::endl;
#endif
					if(max_part < 100){
						printResults(decision);
						temp_node_ptr->resource_pntg=max_part;
						NodePtr new_node_ptr = makeEmptyNode(temp_node_ptr->id,100-max_part,temp_node_ptr->type);
						if(max_part == 50) new_node_ptr->dedup_num=1;
						decision.vGPUList[temp_node_ptr->id]->vNodeList.push_back(new_node_ptr);
						// also add to given resources for this task
						given.push_back(new_node_ptr);
						sort(given.begin()+1, given.end(),cmp_nodeptr_dsc);
					}
				}
				else{
					// get batch size for saturated partition
					temp_batch = getMaxBatch(task,temp_node_ptr,decision,task.request_rate,false,true);
					// assuming we can modify the batch size later, we fix the temporary max batch size to 1
					if(temp_batch == 0) temp_batch=1;
				}
				int max_batch=temp_batch;

				float latency = getLatency(temp_node_ptr->type,task.id,max_batch,temp_node_ptr,decision);
				float local_max_trp = max_batch * (1000.0 / latency);
				if(local_max_trp > task.request_rate){
					proceed_to_residue = true;
					break; 
				}
				TaskPtr temp_task_ptr = createNewTaskPtr(task.id,task.request_rate,task.SLO,max_batch,local_max_trp);
				// CHECK MEM BW 
#ifdef CHECK_NETBW
				if(_NLC.adjustBatchSizetoNetBW(temp_task_ptr,decision.vGPUList[temp_node_ptr->id])){
					return EXIT_FAILURE;
				}
#endif


#ifdef SCALE_DEBUG
				std::cout << __func__ << "(saturate): checking memory for " << task.id << " on " << temp_node_ptr->id << std::endl;
#endif
				if(!doesFitMemLimit(decision.vGPUList[temp_node_ptr->id],task.id,temp_node_ptr)){
#ifdef SCHED_DEBUG
					std::cout << "Mem check failed!"<<std::endl;
#endif
				}
				else{
					temp_node_ptr->occupancy=1;
					temp_node_ptr->vTaskList.push_back(temp_task_ptr);
					temp_node_ptr->duty_cycle = getLatency(temp_node_ptr->type,task.id,max_batch,temp_node_ptr,decision);
					// and then check if this is OK
					bool revert=checkNeedtoRevert(temp_node_ptr, task,decision);
					// revert decision if not OK
					if(revert ){
						revertNode(temp_node_ptr,task,decision);
					}
					else{
						task.request_rate -= temp_task_ptr->throughput;
#ifdef CHECK_MEM
						addGPUMemUsage(decision.vGPUList[temp_node_ptr->id],task.id, temp_node_ptr);
#endif 
#ifdef SCHED_DEBUG
						printf("[readjust] allocatd saturate node to [%d,%d,%d], remaining rate: %d \n", temp_node_ptr->id, temp_node_ptr->resource_pntg, temp_node_ptr->dedup_num,task.request_rate);
#endif
					}
				}
				auto it = find(given.begin(), given.end(),temp_node_ptr);
				//given.erase(given.begin(),given.begin()+1); // delete front node
				given.erase(it); // delete front node
			} // while loop

			if(proceed_to_residue){
				while(task.request_rate > TRP_SLACK){
					if(given.empty()) break;
				//NodePtr temp_node_ptr = given.front();
					std::string type_to_prioritize;
					NodePtr temp_node_ptr;
					if(checkTypePriority(type_to_prioritize)){
						bool successful=false;
						// 2. if so find the node with prioritized type, if there are no more nodes just chose front 
						if(getNodewithPriorityType(task,type_to_prioritize,temp_node_ptr,given)){
							temp_node_ptr=given.front();
						}
					} 
					// 3. if not, return the node which yields minimum partiton su,
					else{
						getMinPartSum(task,type_to_prioritize,getMaxPartSize(decision));
						if(getNodewithPriorityType(task,type_to_prioritize,temp_node_ptr,given)){
							temp_node_ptr=given.front();
						}
					}

					if(temp_node_ptr->resource_pntg == 100 && _usePart && _useRepartition){
						std::vector<NodePtr> candidates;
						const float  MAX_PART = getMaxPartSize(decision);
						estimateTrp(temp_node_ptr->type,task,task.request_rate,candidates,MAX_PART);
						assert(!candidates.empty());
						int max_part = candidates[0]->resource_pntg;
						assert(candidates[0]->vTaskList.size() ==1);
						int temp_batch=candidates[0]->vTaskList[0]->batch_size;
#ifdef SCHED_DEBUG
						std::cout << "[readjust] Splitting node into " << max_part << " and " << 100-max_part << std::endl;
#endif
						if(max_part < 100){
							printResults(decision);
							temp_node_ptr->resource_pntg=max_part;
							NodePtr new_node_ptr = makeEmptyNode(temp_node_ptr->id,100-max_part,temp_node_ptr->type);
							if(max_part == 50) new_node_ptr->dedup_num=1;
							decision.vGPUList[temp_node_ptr->id]->vNodeList.push_back(new_node_ptr);
							// also add to given resources for this task
							given.push_back(new_node_ptr);
							sort(given.begin()+1, given.end(),cmp_nodeptr_dsc);
						}
					}



					int max_pntg = temp_node_ptr->resource_pntg;
#ifdef SCHED_DEBUG
					std::cout << __func__ << ": proceed_to_residue, task.id: "<< task.id << " request_rate " << task.request_rate << " additional_rate" << task.additional_rate << std::endl; 
#endif
					max_batch=getMaxBatch(task,temp_node_ptr,decision,task.request_rate,true,true);
					if(!max_batch) return EXIT_FAILURE;
					temp_node_ptr->duty_cycle =  max_batch * (1000.0  / task.request_rate);
					float local_max_trp = (1000.0*max_batch) / temp_node_ptr->duty_cycle;
					TaskPtr temp_task_ptr = createNewTaskPtr(task.id,task.request_rate,task.SLO,max_batch,local_max_trp);
					// CHECK MEM BW 
#ifdef CHECK_NETBW
					if(_NLC.adjustBatchSizetoNetBW(temp_task_ptr,decision.vGPUList[temp_node_ptr->id])){
						return EXIT_FAILURE;
					}       
#endif
					float latency = getLatency(temp_node_ptr->type,task.id,temp_task_ptr->batch_size,temp_node_ptr,decision);
					temp_node_ptr->occupancy=latency / temp_node_ptr->duty_cycle;
#ifdef SCALE_DEBUG
					std::cout << __func__ << "(residue): checking memory for " << task.id << " on " << temp_node_ptr->id << std::endl;
#endif
					if(!doesFitMemLimit(decision.vGPUList[temp_node_ptr->id],task.id,temp_node_ptr)){
#ifdef SCHED_DEBUG
						std::cout << "Mem check failed!"<<std::endl;
#endif
					}
					else{

						temp_node_ptr->vTaskList.push_back(temp_task_ptr);

						// check if this is OK
						bool revert= checkNeedtoRevert(temp_node_ptr,task,decision);
						// revert decision if not OK
						if(revert){
							revertNode(temp_node_ptr,task,decision);
						}
						else{
							task.request_rate = (task.request_rate >= temp_task_ptr->throughput) ? task.request_rate - temp_task_ptr->throughput : 0;
#ifdef CHECK_MEM
							addGPUMemUsage(decision.vGPUList[temp_node_ptr->id],task.id,temp_node_ptr);
#endif
#ifdef SCHED_DEBUG
							printf("[readjust] allocatd residue node, remaining rate: %d \n", task.request_rate);
#endif
						} // if not revert
					}// check mem limit
					auto it = find(given.begin(), given.end(),temp_node_ptr);
					given.erase(it); // delete one node
				} // while task.reqeuest_rate 

			} // if (proceed_to_residue)

			if(task.request_rate > TRP_SLACK) return EXIT_FAILURE;
			else return EXIT_SUCCESS;
		}

	bool IncrementalScheduler::checkFit(std::vector<NodePtr> &candidate_nodes, SimState &decision){
			//check how much is required
			int required_pntg=0;
#ifdef SCHED_DEBUG
			printf("[checkFit] Recieved type %s %lu nodes for checking \n", candidate_nodes[0]->type.c_str(),candidate_nodes.size());
			printf("[ ");
			for(auto input_ptr : candidate_nodes){
				printf("%d",input_ptr->resource_pntg );
				printf(", ");
			}
			printf("]\n");

#endif
			std::vector<NodePtr> flagged;
			std::map<NodePtr, int> remain_pntg;

			for(auto input_ptr : candidate_nodes){
				NodePtr result;
#ifdef SCHED_DEBUG
				printf("[checkFit] checking for part: %d \n", input_ptr->resource_pntg);
#endif

				while(true){
					if(findBestFit(decision,input_ptr, flagged, result)){
#ifdef SCHED_DEBUG
						printf("[checkFit] FAILED \n");
#endif

						return EXIT_FAILURE;
					}
					if(remain_pntg.find(result) == remain_pntg.end()) // not in map
					{
						remain_pntg[result]=result->resource_pntg;
					}
					// the following case happens because BestFit does not consider tasks that were previously scheduled
					if(remain_pntg[result] < input_ptr->resource_pntg){
						flagged.push_back(result);
					}
					else break;
				}

				if(_useRepartition && remain_pntg[result]==100)
					remain_pntg[result]-=input_ptr->resource_pntg;
				else
					remain_pntg[result]=0;

				if (remain_pntg[result] == 0)
					flagged.push_back(result);
			}
#ifdef SCHED_DEBUG
			printf("[checkFit] SUCCEEDED \n");
#endif
			return EXIT_SUCCESS;
	}

	bool IncrementalScheduler::findBestFit(SimState &input_sim,NodePtr &input,std::vector<NodePtr> &exclude_vec,NodePtr &output){
			int min_diff = 200;
			NodePtr min_ptr;
			std::string type = input->type;
			for (auto gpu_ptr : input_sim.vGPUList)
			{
				if (gpu_ptr->TYPE != type)
					continue;
				for (auto node_ptr : gpu_ptr->vNodeList)
				{
					if (node_ptr->resource_pntg < input->resource_pntg || !node_ptr->vTaskList.empty())
						continue;
					std::vector<NodePtr>::iterator fit = find(exclude_vec.begin(), exclude_vec.end(), node_ptr);
					if (fit != exclude_vec.end())
						continue;
					bool skip = false;
					assert(input->vTaskList.size() == 1);
					TaskPtr task_ptr = input->vTaskList[0];
					// checkwhether memory is OK 
#ifdef CHECK_MEM
#ifdef SCALE_DEBUG
					std::cout << __func__ << ": checking memory for " << task_ptr->id << " on " << node_ptr->id << std::endl;
#endif

					if(!doesFitMemLimit(gpu_ptr,task_ptr->id, node_ptr)) 
					{
#ifdef SCHED_DEBUG
						std::cout << __func__ << ": failed mem check" << std::endl;
#endif
						skip=true;
					}
#endif
					// check whether interferences will be OK
					// check for input node
					float latency = getLatency(type,task_ptr->id,task_ptr->batch_size,node_ptr, input_sim);
					if(latency + input->duty_cycle > task_ptr->SLO ){
#ifdef SCHED_DEBUG
						std::cout << __func__ << ": failed SLO check" << std::endl;
#endif
						skip=true;
					} 
					// check for neighboring node
					if(!skip) skip=checkForInterference(type,input,node_ptr,input_sim);       
#ifdef SCHED_DEBUG
					if(skip) std::cout << __func__ <<": failed interference check"<<std::endl;
#endif
					if(skip) continue;

					// chose 100% nodes first 
					// check if the node is a 100% node, and partitoining is available
#ifdef SCHED_DEBUG
					std::cout << __func__ << ": comparing: "<< input->resource_pntg << " with node: " << node_ptr->resource_pntg
						<<std::endl;
#endif

					if(node_ptr->resource_pntg == 100 && (getUseParts() == true)){
						min_diff=0;
						min_ptr=node_ptr;
					}    
					if(min_diff >= node_ptr->resource_pntg - input->resource_pntg){
						min_diff = node_ptr->resource_pntg - input->resource_pntg;
						min_ptr=node_ptr;
					}

				}
			}
			if(min_diff == 200) return EXIT_FAILURE;
			output=min_ptr;
			return EXIT_SUCCESS;
	}


} // namespace:Scheduling
