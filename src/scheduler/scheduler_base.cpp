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

	int BaseScheduler::setupScheduler(std::string sim_config_json_file){
#ifdef SCHED_DEBUG
		printf("Reading App JSON File: %s \n", sim_config_json_file.c_str());
#endif 
		Json::Value root;
		std::ifstream ifs; 
		ifs.open(sim_config_json_file);
		Json::CharReaderBuilder builder;
		JSONCPP_STRING errs;
		if (!parseFromStream(builder, ifs, &root, &errs)) {
			std::cout << errs << std::endl;
			ifs.close();
			return EXIT_FAILURE;
		}

		for(auto gpu : root["GPUs"]){
			_typeToNumofTypeTable[gpu["Type"].asString()] = gpu["Num"].asInt();            
		}
		_numMaxModel = root["Max Model"].asInt();
		_usePart = root["Part"].asBool();
		_latencyRatio = root["Latency Ratio"].asFloat();
		_useInterference = root["Interference"].asBool();
		_useIncremental=false;
		if(root.isMember("Incremental")){
			_useIncremental = root["Incremental"].asBool();
		}
		if(root.isMember("Self Tuning")){
			_useSelfTuning = root["Self Tuning"].asBool();
			_useInterference=false;
		}
		std::cout <<"Reading "<< root["Avail_Parts"].size()<< " sizes" << std::endl; 
		for(unsigned int i=0; i < root["Avail_Parts"].size(); i++){
			std::cout << "read part: " <<  root["Avail_Parts"][i].asInt() << std::endl;

			if(find(_availParts.begin(), _availParts.end(),root["Avail_Parts"][i].asInt()) == _availParts.end()){
				_availParts.push_back(root["Avail_Parts"][i].asInt());  
#ifdef SCHED_DEBUG
				std::cout << "pushed part: " <<  root["Avail_Parts"][i].asInt() << std::endl;
#endif
			}
		}
#ifdef SCHED_DEBUG
		std::cout << "ended"  << std::endl;
#endif
		sort(_availParts.begin(),_availParts.end(),std::greater<int>());

		assert(1<= _latencyRatio);
		ifs.close();
		return EXIT_SUCCESS;
	}

	int BaseScheduler::setupPerModelMemConfig(std::string mem_config_json_file){
#ifdef SCHED_DEBUG
		std::cout << __func__ << " called"
			<< std::endl;
#endif
		Json::Value root;
		std::ifstream ifs; 
		ifs.open(mem_config_json_file);
		Json::CharReaderBuilder builder;
		JSONCPP_STRING errs;
		if (!parseFromStream(builder, ifs, &root, &errs)) {
			std::cout << errs << std::endl;
			ifs.close();
			return EXIT_FAILURE;
		}
		for(unsigned int i =0; i< root["models"].size(); i++)
		{
			std::string model_name = root["models"][i]["name"].asString();
			int mem_size = root["models"][i]["mem"].asInt();
			_mapModelIDtoMemSize[getModelID(model_name)]=mem_size;
#ifdef SCHED_DEBUG
			std::cout << __func__ << " model: " << model_name << " mem_size: " << _mapModelIDtoMemSize[getModelID(model_name)]
				<< std::endl;
#endif

		}
		ifs.close();
		return EXIT_SUCCESS;
	}

	int BaseScheduler::setupDevicePerfModel(std::string device_config_json_file, std::string res_dir){
#ifdef SCHED_DEBUG
		std::cout << __func__ << ": called for " << device_config_json_file << std::endl;
#endif
		Json::Value root;
		std::ifstream ifs; 
		ifs.open(device_config_json_file);
		Json::CharReaderBuilder builder;
		JSONCPP_STRING errs;
		if (!parseFromStream(builder, ifs, &root, &errs)) {
			std::cout << errs << std::endl;
			ifs.close();
			return EXIT_FAILURE;
		}

		for (auto dev_spec : root["device_specs"])
		{
			std::string device = dev_spec["type"].asString();
			int dev_mem = dev_spec["mem_mb"].as<int>();
			std::string latency_config_json_file = res_dir +"/"+dev_spec["latency_prof_file"].asString();
			std::string int_model_constant_file = res_dir +"/"+dev_spec["interference_const_file"].asString();;
			std::string int_util_file = res_dir +"/"+dev_spec["interference_util_file"].asString();
			_nametoDevPerfModelTable[device].setup(latency_config_json_file,int_model_constant_file,int_util_file,dev_mem);
#ifdef SCHED_DEBUG
			std::cout << "device : "<< device <<" memory: " <<_nametoDevPerfModelTable[device].getDevMem() << std::endl;
#endif

		}
		return EXIT_SUCCESS;
	}

	void BaseScheduler::setupBatchLatencyTables(std::string res_dir, std::vector<std::string> &batch_filenames){
		for (auto it = batch_filenames.begin(); it != batch_filenames.end(); it ++){
			std::map<int,float> *the_table;

			if((*it).find("1_28_28.txt") != std::string::npos ){
				the_table = &_batch_latency_1_28_28;
			}
			else if ((*it).find("3_300_300.txt") != std::string::npos){
				the_table = &_batch_latency_3_300_300;
			}
			else if ((*it).find("3_224_224.txt") != std::string::npos ){
				the_table = &_batch_latency_3_224_224;
			}
			else {
				std::cout << "unrecognized file: " << *it << std::endl;
				exit(1);
			}
			std::string str_buf;
			std::fstream fs;
			std::string filename=*it;
			std::string batchfile = res_dir + "/" + filename;
			fs.open(batchfile, std::ios::in);
			// check whether file open has failed 
			if(!fs){            
				std::cout << "failed to open " << *it << std::endl;
				exit(1);
			}
			int batch;
			float latency;
			float variance; //for now we dont use variance 
			for(int j=0;j<_MAX_BATCH;j++)
			{ 
				getline(fs,str_buf,',');
				batch=stoi(str_buf);

				getline(fs,str_buf,',');
				latency=stof(str_buf);
				(*the_table)[batch]=latency;

			}
			fs.close();

		}
	}

	bool BaseScheduler::getUseParts(){
		return _usePart;
	}

	std::vector<int> BaseScheduler::getAvailParts(){
		return _availParts;
	}

	int BaseScheduler::getMaxGPUs(){
		int sum=0;
		for(auto type_num_pair : _typeToNumofTypeTable){
			sum += type_num_pair.second;
		}
		return sum; 
	}

	int BaseScheduler::setMaxGPUs(int num_gpu){

		_numMaxGPU=num_gpu;
	}

	int BaseScheduler::getMaxBatchSize(){
		return _MAX_BATCH;
	}

	std::string BaseScheduler::getModelName(int id){
		return this->_IDtoModelName[id];
	}

	int BaseScheduler::getModelID(std::string model_name){
		int id=-1;
		for( auto name : this->_IDtoModelName){
			id++;
			if(name == model_name){
				break;
			}
		}
		assert(id != -1);
		return id;
	}

 void BaseScheduler::setupTasks(std::string task_csv_file, std::vector<Task> *task_list){
                std::string str_buf;
                std::fstream fs;
                fs.open(task_csv_file, std::ios::in);

                getline(fs, str_buf);
                int num_of_task = stoi(str_buf);
                for(int j=0;j<num_of_task;j++)
                {
                        Task new_task;

                        getline(fs,str_buf,',');
                        new_task.id=stoi(str_buf);

                        getline(fs,str_buf,',');
                        new_task.request_rate=stoi(str_buf);
#ifdef ADD_RATE
                        new_task.request_rate = return99P(new_task.request_rate);
#endif

                        getline(fs,str_buf,',');
                        new_task.SLO=stoi(str_buf);

                        task_list->push_back(new_task);
                }
                fs.close();
#ifdef SCHED_DEBUG
                std::cout << "init_setup ------------------------------------------------" << std::endl;
                std::cout << "task (name : [req, SLO])" << std::endl;
                for(std::vector<Task>::iterator iter=task_list->begin(); iter != task_list->end(); ++iter)
                {
                        std::cout << iter->id << " : [" << iter->request_rate << ", " << iter->SLO << "]" << std::endl;
                        iter->SLO= iter->SLO;
                        std::cout << iter->id << "configured SLO: "<< iter->SLO <<std::endl;
                }
                std::cout << std::endl;
#endif

    }

	 void BaseScheduler::setupAvailParts(std::vector<int> input_parts){
                assert(input_parts.size() >=1);
                _availParts.clear();

                for(auto part : input_parts){
                        if(part !=0) _availParts.push_back(part);
                        int counter_part = 100-part;
                        if(counter_part !=0){
                                std::vector<int>::iterator it = find(_availParts.begin(), _availParts.end(),counter_part);
                                if(it == _availParts.end()){
                                        _availParts.push_back(counter_part);
                                }
                        }
                }
                // sort in descneding order
                sort(_availParts.begin(),_availParts.end(),std::greater<int>());
        }




	void BaseScheduler::initiateDevs(SimState &input, int nDevs){
#ifdef SCALE_DEBUG
		std::cout << __func__ << " initiating " << nDevs << " GPUs for simulator" << std::endl;
#endif
		input.vGPUList.clear();
		int idx=0;
		for(auto type : _typeToNumofTypeTable){
			std::string gpu_type = type.first ;
			int num_of_gpu = type.second;
			for(int i=0; i < num_of_gpu; i++){
				GPU new_gpu;
				GPUPtr new_gpu_ptr=std::make_shared<GPU>(new_gpu);
				new_gpu_ptr->GPUID=idx;
				new_gpu_ptr->TYPE=gpu_type;
				NodePtr new_node_ptr=makeEmptyNode(idx,100,gpu_type);
				new_gpu_ptr->vNodeList.push_back(new_node_ptr);
				input.vGPUList.push_back(new_gpu_ptr);
				idx++;
				if(idx >= nDevs) return;
			}
		}
	}

	int returnNDev(std::vector<int> &mem_per_dev, SimState &input){
		int ndevs=0;
		if(mem_per_dev.size() != input.vGPUList.size()){
			std::cout<<__func__<<":WARNING: number of devices to configure does not match number of devices for simstate" << std::endl;
			std::cout<<__func__<<": size of mem_vector: "<< mem_per_dev.size() <<" size of simstate.vGPUList: " << input.vGPUList.size() << std::endl;
			ndevs=std::min(mem_per_dev.size(), input.vGPUList.size());
		}
		else ndevs = mem_per_dev.size();
		return ndevs;
	}
	void BaseScheduler::initDevMems(SimState &input){
		int total_devs = input.vGPUList.size();
		assert(total_devs);
		int idx=0;
		for(auto type_num_pair : _typeToNumofTypeTable){
			int ndevs=type_num_pair.second;
			std::string type=type_num_pair.first;
			for(int i=0; i <ndevs; i++){
				initDevMem(input.vGPUList[idx], _nametoDevPerfModelTable[type].getDevMem()); 
#ifdef SCHED_DEBUG
				std::cout<< __func__ << ": "<< "dev" << idx << " will be setted up as " << _nametoDevPerfModelTable[type].getDevMem() << "MBs" << std::endl;
#endif
				idx++;
				if(idx>=total_devs) return;
			}
		}
	}

	void BaseScheduler::initDevMem(GPUPtr gpu_ptr, int mem){
		gpu_ptr->TOTAL_MEMORY=mem;
		gpu_ptr->used_memory=0;  
	}

	void BaseScheduler::updateDevMemUsage(std::vector<int> &used_mem_per_dev, SimState &input){
		int ndevs=returnNDev(used_mem_per_dev, input);
		for(int i=0 ; i <ndevs; i++){
			input.vGPUList[i]->used_memory=used_mem_per_dev[i];
#ifdef SCHED_DEBUG
			std::cout << __func__ << ": gpu " << i << " used memory updated to " << input.vGPUList[i]->used_memory << std::endl;
#endif
		}
	}

	
	float BaseScheduler::getInterference(std::string device, int a_id, int b_id, int a_batch, int b_batch,int partition_a, int partitoin_b){
		assert(!device.empty());
		std::string model_a= _IDtoModelName[a_id];
		std::string model_b = _IDtoModelName[b_id];
		float retAlpha=_nametoDevPerfModelTable[device].getInterference(model_a,a_batch,partition_a, model_b,b_batch,partitoin_b);
		return retAlpha > 1 ? retAlpha : 1.0;
	}

	float BaseScheduler::getInterference(std::string device, const int model_id,const int batch_size,const NodePtr node_ptr,const GPUPtr gpu_ptr){
		assert(!device.empty());
		float max_interference =1.0;
		for (auto node: gpu_ptr->vNodeList){
			if(isSameNode(node,node_ptr)) continue;
			for(auto task : node->vTaskList){
				float interference = getInterference(device, model_id,task->id,batch_size,task->batch_size,node_ptr->resource_pntg,node->resource_pntg);
				if (max_interference < interference) max_interference = interference;
			}
		}
		return max_interference;
	}

	//more simple version of getting latency without considering interference
	float BaseScheduler::getLatency(std::string device, int model_num, int batch, int part){
		assert(!device.empty());
		assert(1<= batch && batch <= _MAX_BATCH);
		float latency = _nametoDevPerfModelTable[device].getLatency(_IDtoModelName[model_num], batch, part);
		if(_useBatchingOverhead) latency += getBatchLatency(_IDtoModelName[model_num],batch);
		float lat_ratio=_latencyRatio;
		return latency* lat_ratio;
	}

	float BaseScheduler::getLatency(std::string device, const int model_num, const int batch, const NodePtr self_node, SimState &input){
		assert(!device.empty());
		assert(batch>=1);
		float pure_latency = _nametoDevPerfModelTable[device].getLatency(_IDtoModelName[model_num], batch, self_node->resource_pntg);
		float interference_overhead=0.0;
		float batch_overhead=0.0; // added to accomodate batching overhead (caused by PyTorch cat function)
		if(_useBatchingOverhead){
			batch_overhead = getBatchLatency(_IDtoModelName[model_num],batch);
		}

		if(_useInterference){
			// do not try to get interference of non-existent GPU
			if (input.vGPUList.size() > self_node->id){ 
				GPUPtr self_gpu= input.vGPUList[self_node->id];
				double int_overhead=getInterference(device,model_num,batch,self_node,self_gpu);
				interference_overhead=pure_latency*(int_overhead-1.0);
				//interference_overhead = (interference_overhead > batch_overhead) ? interference_overhead - batch_overhead : 0.0;
			}
		}
		float lat_ratio=_latencyRatio; 
		return (pure_latency + batch_overhead + interference_overhead) * lat_ratio;
	}

	bool BaseScheduler::isModelLoaded(GPUPtr gpu_ptr, NodePtr node_ptr, int model_id){
		// if part is not loaded, model is not loaded
#ifdef SCHED_DEBUG
		std::cout << __func__ << ": checking model " << model_id << " is loaded for gpu "
			<<"[" << gpu_ptr->GPUID << "," << node_ptr->dedup_num << "]"
			<<std::endl;
#endif
		if(!isPartLoaded(gpu_ptr, node_ptr)) return false;

		bool found=false;
		for(auto _node_ptr : gpu_ptr->vNodeList){
			if(_node_ptr->resource_pntg == node_ptr->resource_pntg && _node_ptr->dedup_num == node_ptr->dedup_num){
				for(auto task_ptr : _node_ptr->vTaskList){
					if(task_ptr->id == model_id) found=true;
				}
			}
		}
#ifdef SCHED_DEBUG
		std::cout << __func__ << ": checking model " << model_id << "found? " << found
			<<std::endl;
#endif

		return found;
	}

	bool BaseScheduler::isPartLoaded(GPUPtr gpu_ptr, NodePtr node_ptr){
		for (auto mem_node : gpu_ptr->vLoadedParts){
			if(mem_node.part == node_ptr->resource_pntg && mem_node.dedup_num == node_ptr->dedup_num)
				return true;

		}
		return false;
	}

	bool BaseScheduler::postAdjustInterference(SimState &input){
		for(auto gpu_ptr : input.vGPUList){
			for(auto node_ptr : gpu_ptr->vNodeList){
				if(adjustResNode(node_ptr,input)) return EXIT_FAILURE;
			}
		}
		return EXIT_SUCCESS;
	}
	bool BaseScheduler::adjustResNode(NodePtr &node_ptr, SimState &simulator){
#ifdef SCHED_DEBUG
		printf("[adjustResNode] called for [%d,%d,%d] \n", node_ptr->id, node_ptr->resource_pntg, node_ptr->dedup_num);
#endif 
		float latency_sum =0;
		bool good_to_go = true;
		for(auto task_ptr : node_ptr->vTaskList){
			latency_sum+=getLatency(node_ptr->type,task_ptr->id,task_ptr->batch_size,node_ptr, simulator);
		}
		for(auto task_ptr : node_ptr->vTaskList){ // considering how fast residue nodes can serve, we allow some slack
			if(latency_sum + node_ptr->duty_cycle > task_ptr->SLO * 1.05) good_to_go=false;
		}
		if(good_to_go) return EXIT_SUCCESS;
#ifdef SCHED_DEBUG
		printf("[adjustResNode] [%d,%d,%d] needs to be adjusted \n", node_ptr->id, node_ptr->resource_pntg, node_ptr->dedup_num);
#endif 
		std::vector<float> min_duty_cycles;
		std::vector<int> new_batches;

		for(auto task_ptr : node_ptr->vTaskList){
			int new_batch_size = task_ptr->batch_size;
			float new_duty_cycle=node_ptr->duty_cycle;
			float latency = getLatency(node_ptr->type,task_ptr->id,new_batch_size,node_ptr,simulator);
			float other_latency = latency_sum - latency;
#ifdef SCHED_DEBUG
			printf("other_latency: %lf, latency: %lf ms, new_duty_cycle: %lf ms \n", other_latency, latency, new_duty_cycle);
#endif 


			while(task_ptr->SLO < other_latency + latency + new_duty_cycle){
				new_batch_size--;
				if(new_batch_size ==0) return EXIT_FAILURE;
				latency = getLatency(node_ptr->type,task_ptr->id,new_batch_size,node_ptr, simulator);  
				new_duty_cycle = new_duty_cycle * (float(new_batch_size) / task_ptr->batch_size);
#ifdef SCHED_DEBUG
				printf("latency: %lf ms, new_batch_size: %d ,new_duty_cycle: %lf ms \n", latency, new_batch_size, new_duty_cycle);
#endif 

			}
			min_duty_cycles.push_back(new_duty_cycle);
			new_batches.push_back(new_batch_size);
		}
		// chose minimum among duty
		assert(min_duty_cycles.size() >= 1);
		sort(min_duty_cycles.begin(), min_duty_cycles.end());
		float min_duty_cycle = min_duty_cycles[0];
		int id=0;
		latency_sum=0;
		for(auto task_ptr : node_ptr->vTaskList){
			task_ptr->batch_size = new_batches[id];
			task_ptr->throughput= (1000 * task_ptr->batch_size) / min_duty_cycle;

			latency_sum += getLatency(node_ptr->type,task_ptr->id,task_ptr->batch_size,node_ptr,simulator);
			id++;
		}
		node_ptr->duty_cycle=min_duty_cycle;
		node_ptr->occupancy = latency_sum / min_duty_cycle;
		return EXIT_SUCCESS;
	}

void BaseScheduler::fillReservedNodes(SimState &input){
		// check how partitioned each GPU are
		for(auto gpu_ptr : input.vGPUList){
			int assigned_part =0;
			int part_num=0;
			for (auto it : gpu_ptr->vNodeList){
				if(!it->vTaskList.empty())
					assigned_part += it->resource_pntg;
				part_num++;
			}
#ifdef SCHED_DEBUG
			printf("[fillReserve] device id: %d, allocated_size: %d \n", gpu_ptr->GPUID , assigned_part);
#endif

			if(assigned_part==0){  // if that GPU was not used at all
				if(_usePart){
					Node new_node;
					std::shared_ptr<Node> new_node_ptr = std::make_shared<Node>(new_node);
					new_node_ptr->resource_pntg=50;
					new_node_ptr->reserved=true;
					new_node_ptr->id = gpu_ptr->GPUID;
					new_node_ptr->dedup_num=0;
					new_node_ptr->duty_cycle=0;
					gpu_ptr->vNodeList.push_back(new_node_ptr);
					Node new_node2;
					std::shared_ptr<Node> new_node_ptr2 = std::make_shared<Node>(new_node2);
					new_node_ptr2->resource_pntg=50;
					new_node_ptr2->reserved=true;
					new_node_ptr2->id = gpu_ptr->GPUID;
					new_node_ptr2->dedup_num=1;
					new_node_ptr2->duty_cycle=0;
					gpu_ptr->vNodeList.push_back(new_node_ptr2);

				}
				else{
					Node new_node;
					std::shared_ptr<Node> new_node_ptr = std::make_shared<Node>(new_node);
					new_node_ptr->resource_pntg=100;
					new_node_ptr->reserved=true;
					new_node_ptr->id = gpu_ptr->GPUID;
					new_node_ptr->dedup_num=0;
					new_node_ptr->duty_cycle=0;
					gpu_ptr->vNodeList.push_back(new_node_ptr);

				}
			}
			else if(0< assigned_part  && assigned_part < 100 && part_num < 2){ // if there is only one partition that is allocated
				Node new_node;
				std::shared_ptr<Node> new_node_ptr = std::make_shared<Node>(new_node);
				new_node_ptr->resource_pntg=100-assigned_part;
				if (new_node_ptr->resource_pntg ==50 ) new_node.dedup_num=1;
				else new_node_ptr->dedup_num =0;
				new_node_ptr->id = gpu_ptr->GPUID;
				new_node_ptr->reserved=true;
				new_node_ptr->duty_cycle=0;
				gpu_ptr->vNodeList.push_back(new_node_ptr);
			} 
		}    
	}

	NodePtr BaseScheduler::makeEmptyNode(int gpu_id, int resource_pntg, std::string type){
		Node NewNode;
		NodePtr NewNodePtr = std::make_shared<Node>(NewNode);
		NewNodePtr->id=gpu_id;
		NewNodePtr->resource_pntg = resource_pntg;
		NewNodePtr->reserved=false;
		NewNodePtr->dedup_num=0;
		NewNodePtr->occupancy=0;
		NewNodePtr->type=type;
		return NewNodePtr;
	}

	
	int BaseScheduler::getMaxBatch(Task &self_task, const NodePtr &self_node, SimState &input, int &req, const bool is_residue, bool interference){
#ifdef SCHED_DEBUG
		printNodeInfo(self_node);
#endif
		int ret_batch=0;
		int prev_batch=0;
		int mid_batch = 1 + (_MAX_BATCH)/2;
		int starting_point, under_limit;
		float latency;
		int model_num=self_task.id;
		int SLO = self_task.SLO;
#ifdef SCHED_DEBUG
		std::cout << __func__ << ": called for task id: " << model_num << "and SLO:  " << SLO << std::endl; 
		if(is_residue){
			std::cout << __func__ << ": for residue rate : " << req << std::endl; 
		}
		else{
			std::cout << __func__ << ": for saturated rate " << std::endl; 

		}
#endif
		int low_batch=1;
		int high_batch=_MAX_BATCH;
		bool check_and_exit=false;
		bool check_last=false;
		while(!check_and_exit){
			// check condition 
			if(low_batch == 1 && high_batch ==1){
				check_last=true;
			}
			int mid_batch = floor((high_batch + low_batch)/2);
			if(interference)
				latency = getLatency(self_node->type,model_num,mid_batch,self_node, input);
			else
				latency = getLatency(self_node->type,model_num,mid_batch,self_node->resource_pntg);
			if(is_residue){
				if(latency + 1000*float(mid_batch)/req <= SLO) {
					low_batch=mid_batch;
				}
				else {
					if(check_last){
						ret_batch=0;
						break;
					}
					high_batch=mid_batch;
				}
			}
			else{
				if(2*latency<= SLO){
					low_batch=mid_batch;
				}
				else{
					if(check_last){
						ret_batch=0;
						break;
					}
					high_batch=mid_batch;
				}
			}
			if(high_batch-low_batch<=1) high_batch=low_batch;
			if(high_batch == low_batch){
				if(low_batch!=1){
					ret_batch=low_batch;
					check_and_exit=true;
				}
				else{
					if(check_last){
						ret_batch=low_batch;
						check_and_exit=true;
					}
				}
			}
		}
#ifdef SCHED_DEBUG
		std::cout << "[getMaxBatch] original ret_batch is " << ret_batch << std::endl;
#endif
		// corner checking small request rates
		if(ret_batch == 0 && is_residue){
#ifdef SCHED_DEBUG
			printf("[getMaxBatch] model %s: need special care on resource partitoin: %d \n", _IDtoModelName[model_num].c_str(), self_node->resource_pntg);
#endif

			bool succeed=false;
			float b1_latency = getLatency(self_node->type,self_task.id,1,self_node,input);
			if(b1_latency < 1000.0 * 1/ req  && b1_latency < SLO){
				req = 1 * (1000 / (self_task.SLO - b1_latency))*_latencyRatio;
				//req = 1 * (1000 / (b1_latency))*_latencyRatio;
				ret_batch=1;
				succeed=true;
#ifdef SCHED_DEBUG
				printf("[getMaxBatch] model %s: changed request rate to %d req/s,  with resource partition: %d\n", _IDtoModelName[model_num].c_str(),req, self_node->resource_pntg);
#endif

			}
			if(!succeed){
#ifdef SCHED_DEBUG
				printf("[getMaxBatch] model %s: not possible to satisfy SLO %d ms, request rate: %d req/s,  with resource partition: %d\n", _IDtoModelName[model_num].c_str(),SLO,req, self_node->resource_pntg);
#endif
				return 0;
			}
		}
#ifdef SCHED_DEBUG
		std::cout << __func__ << ": returning max_batch: " << ret_batch << std::endl; 

#endif
		return ret_batch;
	}

	float BaseScheduler::getBatchLatency(std::string modelname, int batch){
		assert(batch>=1);
		float ret_latency=0;

		// I know this looks ugly and wierd. Batching latency is long in a arbitrary manner, so a constant is multiple to the average value
		// so that the scheduler can make conservative decisions. If this problem is fixed, this will not be required any more.
		if(modelname == "lenet1" || modelname == "lenet2" || modelname == "lenet3" || modelname == "lenet4" || modelname == "lenet5" || modelname == "lenet6"){
			ret_latency=0.4;
		}
		else if(modelname == "resnet50" || modelname == "googlenet" || modelname == "vgg16" || modelname == "mnasnet1_0" || modelname == "mobilenet_v2" || modelname=="densenet161"){
			ret_latency = 1.9* _batch_latency_3_224_224[batch]; 
		}
		else if(modelname == "ssd-mobilenetv1"){
			ret_latency = 2.5*_batch_latency_3_300_300[batch]; 
		}
		else if(modelname == "bert"){
			ret_latency=1;
		}
		else{
			std::cout<<"unrecognized model: "<< modelname<<", check your code! "<< std::endl;   
			exit(1);
		}

		assert(ret_latency !=0);
		return ret_latency;

	}
	bool BaseScheduler::adjustSatNode(NodePtr &node_ptr, SimState &simulator, Task &task_to_update){
#ifdef SCHED_DEBUG
		printf("[ajustSatNode] called for [%d,%d,%d] \n", node_ptr->id, node_ptr->resource_pntg, node_ptr->dedup_num);
#endif 
		assert(node_ptr->occupancy >=1 && node_ptr->vTaskList.size() == 1);
		bool good_to_go = true;
		// Saturate Node have only one task 
		TaskPtr task_ptr=node_ptr->vTaskList[0];
		float latency = getLatency(node_ptr->type,task_ptr->id,task_ptr->batch_size,node_ptr,simulator);
#ifdef SCHED_DEBUG
		printf("[ajustSatNode]  task: %d, latency: %lf SLO: %d \n", task_ptr->id, latency, task_ptr->SLO);

#endif

		if(2*latency < task_ptr->SLO){
			float org_througput  = task_ptr->throughput;
			task_ptr->throughput=(task_ptr->batch_size * 1000.0 )/ latency;
#ifdef SCHED_DEBUG
			std::cout << "task_to_update.additional_rate (before): " << task_to_update.additional_rate << std::endl;
#endif
			task_to_update.additional_rate+=(int(org_througput - task_ptr->throughput) > 0 )  ? int(org_througput - task_ptr->throughput) : 0;
#ifdef SCHED_DEBUG
			std::cout << "task_to_update.additional_rate (after): " << task_to_update.additional_rate << std::endl;
#endif
		}   
		else good_to_go=false;
		if(good_to_go) return EXIT_SUCCESS;
#ifdef SCHED_DEBUG
		printf("[ajustSatNode] [%d,%d,%d] needs to be adjusted \n", node_ptr->id, node_ptr->resource_pntg, node_ptr->dedup_num);
#endif 
		float min_duty_cycle=0.0;
		int min_batch=0;
		int org_batch_size = task_ptr->batch_size;
		int new_batch_size = task_ptr->batch_size;
		float new_duty_cycle=node_ptr->duty_cycle;
		float original_l_dc = 2*getLatency(node_ptr->type,task_ptr->id,new_batch_size,node_ptr->resource_pntg);
		while(task_ptr->SLO <  2*latency){
			new_batch_size--;
			if(new_batch_size ==0) return EXIT_FAILURE;
			latency = getLatency(node_ptr->type,task_ptr->id,new_batch_size,node_ptr,simulator);  
			new_duty_cycle = latency;
		} 
		min_duty_cycle = new_duty_cycle;
		min_batch=new_batch_size;
		assert(min_duty_cycle != 0.0);
		assert(min_batch !=0);

		// update node
#ifdef SCHED_DEBUG
		printf("[ajustSatNode] new_batch_size: %d ,new_duty_cycle: %lf ms \n", min_batch, min_duty_cycle);
#endif 

		task_ptr = node_ptr->vTaskList[0];
		task_ptr->batch_size=min_batch;
		node_ptr->duty_cycle=min_duty_cycle; // in order to give more oppurtunities to node, we chose minimum
		float org_throughput=task_ptr->throughput;
		float new_throughput=task_ptr->batch_size * (1000.0 /node_ptr->duty_cycle); 
		task_ptr->throughput=new_throughput;
#ifdef SCHED_DEBUG
		std::cout << "task_to_update.additional_rate (before): " << task_to_update.additional_rate  << std::endl;
#endif
		task_to_update.additional_rate +=(org_throughput > new_throughput) ? int(org_throughput-new_throughput) : 0;
#ifdef SCHED_DEBUG
		std::cout << "task_to_update.additional_rate (after): " <<  task_to_update.additional_rate  << std::endl;
#endif
#ifdef SCHED_DEBUG
		printf("[ajustSatNode] org_throughput: %lf ,new_throughput: %lf ms \n",org_throughput,new_throughput );
#endif   
		return EXIT_SUCCESS;
	} //adjustSATNode

	bool BaseScheduler::adjustSatNode(NodePtr &node_ptr, SimState &simulator){
		return adjustSatNode(node_ptr,simulator,*node_ptr->vTaskList[0]);
	} //adjustSATNode

	//returns whether NodePtr a and b are (conceptually) pointing to the same node
	bool BaseScheduler::isSameNode(NodePtr a, NodePtr b){
		return (a->id == b->id) && (a->resource_pntg == b->resource_pntg) && (a->dedup_num == b->dedup_num);
	}

	// assuming there are only two nodes per GPU, get the other node
	bool BaseScheduler::getOtherNode(NodePtr the_node, NodePtr &output_the_other_node, SimState &sim){
		for(auto gpu_ptr : sim.vGPUList)
		{
			if(gpu_ptr->GPUID == the_node->id){
				for(auto node_ptr: gpu_ptr->vNodeList){
					if(!isSameNode(the_node,node_ptr)){
						output_the_other_node=node_ptr;
						return EXIT_SUCCESS;
					}

				}
			}

		}
		return EXIT_FAILURE;
	}

		int BaseScheduler::return99P(const int mean){
		//added for fast returning values that might take too long
		// based on calculation value of 550: 10.02%
		std::cout << "received: " << mean << std::endl;
		if(mean > 550) return mean*1.1;
		double prob_sum=0.0;
		int new_mean=1;
		do{ 
			double prob=calcPoissProb(new_mean,mean);
			prob_sum+=prob;
			new_mean++;
		}while(prob_sum < 0.99 );
		std::cout << "returning new_mean: "<< new_mean << std::endl; 
		return new_mean;
	}

	double BaseScheduler::calcPoissProb(int actual, int mean){
		double t1 = exp(-mean);
		for(int i =0; i< actual; i++){
			t1 *=double(mean);
			t1 /=(i+1);  //check how much leftover requests were  

		}   
		return t1; 
	}

	// reset input according to node sizes vector
	// only use id, resource_pntg of nodes for this function
	void BaseScheduler::resetScheduler(SimState &input, std::vector<Node> node_sizes){
		std::vector<int> curr_gpu_node_idxs;
		for(auto type_num_pair : _typeToNumofTypeTable){
			int nGPU = type_num_pair.second;
			std::string type = type_num_pair.first;
			int idx=0;
			for(int j =0; j <nGPU ; j++){
				GPU new_gpu;
				GPUPtr new_gpu_ptr=std::make_shared<GPU>(new_gpu);
				new_gpu_ptr->GPUID=idx;
				new_gpu_ptr->TYPE = type;
				std::vector<Node> temp_nodes;
				for(auto node : node_sizes){
					if ((node.id-1) == idx){
						temp_nodes.push_back(node);
					}
				}
				for(auto the_node : temp_nodes){
					NodePtr new_node_ptr=makeEmptyNode(the_node.id-1,the_node.resource_pntg,type);
					new_node_ptr->dedup_num=the_node.dedup_num;
					new_gpu_ptr->vNodeList.push_back(new_node_ptr);
				}

				input.vGPUList.push_back(new_gpu_ptr);
				idx++;
			}
		}
#ifdef SCHED_DEBUG
	printResults(input);
#endif
		return;
	}

	void BaseScheduler::printNodeInfo(const NodePtr &node)
	{
		std::cout 
			<< "GPUID: " << node->id <<", "
			<< "type: " << node->type << std::endl;
	}




} // Scheduling
