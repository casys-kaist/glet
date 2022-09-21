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
		_residueThreshold = root["Residue"].asFloat();
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

		assert(0<=_residueThreshold);
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



} // Scheduling
