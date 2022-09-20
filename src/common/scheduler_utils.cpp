#include "scheduler_utils.h"
#include "config.h"
#include <assert.h>
#include <iostream>

//deep copies input to output
void copyToOutput(SimState &input, SimState &output){
	output.vGPUList.clear();
	for(auto gpu_ptr : input.vGPUList){
		GPU new_gpu;
		GPUPtr new_gpu_ptr=std::make_shared<GPU>(new_gpu);
		new_gpu_ptr->GPUID = gpu_ptr->GPUID;
		new_gpu_ptr->TYPE = gpu_ptr->TYPE;
		for(auto node_ptr: gpu_ptr->vNodeList){
			Node new_node;
			NodePtr new_node_ptr=std::make_shared<Node>(new_node);
			new_node_ptr->id = node_ptr->id;
			new_node_ptr->resource_pntg = node_ptr->resource_pntg;
			new_node_ptr->dedup_num = node_ptr->dedup_num;
			new_node_ptr->duty_cycle = node_ptr->duty_cycle;
			new_node_ptr->reserved = node_ptr->reserved;
			new_node_ptr->occupancy = node_ptr->occupancy;
			new_node_ptr->type = node_ptr->type;
			if(!node_ptr->vTaskList.empty()){
				MemNode new_mem_node;
				new_mem_node.dedup_num=node_ptr->dedup_num;
				new_mem_node.part=node_ptr->resource_pntg;
				gpu_ptr->vLoadedParts.push_back(new_mem_node);
			}         
			for(auto task_ptr : node_ptr->vTaskList){
				Task new_task;
				TaskPtr new_task_ptr= std::make_shared<Task>(new_task);
				new_task_ptr->id=task_ptr->id;
				new_task_ptr->batch_size=task_ptr->batch_size;
				new_task_ptr->request_rate=task_ptr->request_rate;
				new_task_ptr->SLO=task_ptr->SLO;
				new_task_ptr->throughput=task_ptr->throughput;
				new_node_ptr->vTaskList.push_back(new_task_ptr);
			}
			new_gpu_ptr->vNodeList.push_back(new_node_ptr);            
		}
		output.vGPUList.push_back((new_gpu_ptr));
	}
	output.parts.clear();
	for(auto part : input.parts){
		output.parts.push_back(part);
	}
	return;
}

void recoverScheduler(const SimState &backup, SimState &output){
	output.vGPUList.clear();
	for (auto gpu_ptr : backup.vGPUList){
		output.vGPUList.push_back(gpu_ptr);
	}
}

void printResults(SimState &input){
	std::cout << "results ------------------------------------------------" << std::endl;
	for(auto iter1 : input.vGPUList){
		std::cout<< "<GPU "<< iter1->GPUID << ", " << iter1->TYPE<< ">"<<std::endl;
		std::cout<<"[";
		for( auto iter2 : iter1->vNodeList){
			std::cout<<iter2->resource_pntg<<",";

		}
		std::cout<<"]"<<std::endl;
		for(auto iter2 : iter1->vNodeList ){
			if(iter2->vTaskList.empty()){
				std::cout << "Reserved Node" << std::endl;
				std::cout << std::endl;
				continue;
			}
			std::cout << "ID: " << iter2->id << std::endl;
			std::cout << "occupancy : " << iter2->occupancy << std::endl;
			std::cout << "partition: " << iter2->resource_pntg<< std::endl;
			std::cout << "dedup_num: " << iter2->dedup_num << std::endl;
			std::cout << "duty cycle: "<< iter2->duty_cycle << std::endl;
			for(auto iter3 : iter2->vTaskList){
				std::cout << "model_id : " << iter3->id << ", throughput : " << iter3->throughput<<", request rate: "<<iter3->request_rate << ", SLO : " << iter3->SLO << ", batch_size : " << iter3->batch_size << std::endl;
				std::cout << "expected latency(ms): "<< iter2->duty_cycle * iter2->occupancy << std::endl;
			}
			std::cout << std::endl;
		}
	}
}

void copySession( std::vector<Task> &org_session, std::vector<Task> &dst_session){
	dst_session.clear();
	for(auto task : org_session){
		dst_session.push_back(task);
	}
}

int getNumofUsedGPUs(SimState &input){
	int used_gpus=0;
	for(auto gpu_ptr: input.vGPUList){
		bool used=false;
		for(auto node_ptr : gpu_ptr->vNodeList){
			if(!node_ptr->vTaskList.empty()){
				used=true;
			}
		}
		if(used) used_gpus++;
	}
#ifdef SCALE_DEBUG
	std::cout<< __func__ << ": numbeor of used GPUS: " << used_gpus
		<<std::endl;
#endif 
	return used_gpus;
}

// for debugging/checking whether gpu_ptr has correct state of loaded memory
void printMemState(const GPUPtr &gpu_ptr){
	std::cout << __func__ <<": called for gpu id : " << gpu_ptr->GPUID 
		<< std::endl;
	std::cout << "Total Memory / Used memory: " << gpu_ptr->TOTAL_MEMORY << " / " << gpu_ptr->used_memory
		<< std::endl;
	std::cout << "loaded parts: "
		<< std::endl;
	for(auto part : gpu_ptr->vLoadedParts){
		std::cout << "[" << part.part << " , " << part.dedup_num << "]"
			<< std::endl;
	}
	std::cout << "loaded models: "
		<< std::endl;
	for(auto node_ptr : gpu_ptr->vNodeList){
		for(auto task_ptr : node_ptr->vTaskList){
			std::cout << "Model ID: "<<task_ptr->id << " "
				<<std::endl;
		}
	}
}


TaskPtr createNewTaskPtr(int id/*=0*/, int request_rate/*=0*/, int SLO/*=0*/, int batch_size/*=0*/, float throughput/*=0*/){
	return createNewTaskPtr(id,request_rate,0,0,SLO,batch_size,throughput);
}

TaskPtr createNewTaskPtr(int id/*=0*/, int request_rate/*=0*/, int ORG_RATE/*=0*/, int additiional_rate/*=0*/, \
		int SLO/*=0*/, int batch_size/*=0*/, float throughput/*=0*/){
	Task NewTask;
	NewTask.id=id;
	NewTask.request_rate=request_rate;
	NewTask.ORG_RATE=ORG_RATE;
	NewTask.additional_rate=additiional_rate;
	NewTask.SLO=SLO;
	NewTask.batch_size=batch_size;
	NewTask.throughput=throughput;
	return createNewTaskPtr(NewTask);
}

TaskPtr createNewTaskPtr(Task &task){
	return std::make_shared<Task>(task);
}

int getMaxPartSize(const SimState &input){
	if(input.vGPUList.empty()) return 100;
	int max_part=-1;
	for(auto gpu_ptr : input.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList)
			if(node_ptr->resource_pntg > max_part) max_part = node_ptr->resource_pntg;
	}
	return max_part;
}

int getMinPartSize(const SimState &input){
	if(input.vGPUList.empty()) return 0;
	int min_part=100;
	for(auto gpu_ptr : input.vGPUList){
		for(auto node_ptr : gpu_ptr->vNodeList)
			if(node_ptr->resource_pntg < min_part) min_part = node_ptr->resource_pntg;
	}
	return min_part;
}
