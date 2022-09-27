#include "backend_setup.h"
#include "json/json.h"
#include "json/json-forwards.h"
#include "backend_delegate.h"
#include "global_scheduler.h"
#include "device_spec.h"

#include <vector>
#include <iostream>
#include <fstream>

std::map<int,std::string> NodeIDtoCtrlAddr;
std::map<int,std::string> NodeIDtoDataAddr;
extern std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

int connectBackendList(std::string backend_list_file){
	Json::Value root;
	std::ifstream ifs;
	ifs.open(backend_list_file);
	if(!ifs){
		std::cout << __func__ << ": failed to open file: " << backend_list_file
			<<std::endl;
		return EXIT_FAILURE;
	}   
	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;
	if (!parseFromStream(builder, ifs, &root, &errs)) {
		std::cout << errs << std::endl;
		ifs.close();
		return EXIT_FAILURE;
	}
	for(int node_id =0; node_id< root["nodes"].size();node_id++){    
		// read control addr port
		std::string control_addr = root["nodes"][node_id]["control_addr"].as<std::string>();
		// read data addr port
		std::string data_addr = root["nodes"][node_id]["data_addr"].as<std::string>();
		// read possible sizes
		std::vector<int> sizes;
		for(auto size : root["nodes"][node_id]["sizes"]){
			sizes.push_back(size.as<int>());
		}
		DeviceSpec *pdev_spec  = new DeviceSpec(root["nodes"][node_id]["gpu_name"].as<std::string>(),\
				root["nodes"][node_id]["gpu_mem_size_mb"].as<int>(),\
				root["nodes"][node_id]["type"].as<std::string>());
		// store related addressd and instatntiate a backend delegate
		BackendDelegate *ptemp = new BackendDelegate(node_id,sizes,data_addr,pdev_spec);
		if(ptemp->connectCtrlChannel(control_addr) == -1){
			LOG(ERROR) << "Backend " << node_id << " Unreachable";
			LOG(ERROR) << "Backend " << node_id << " will be skipped";
			delete ptemp;
			continue;
		}
		NodeIDtoCtrlAddr[node_id] = control_addr;
		NodeIDtoDataAddr[node_id] = data_addr;
		ptemp->updateNGPUS();
		NodeIDtoBackendDelegate[node_id] = ptemp;
	}
	// for debugging
	for(auto iter : NodeIDtoBackendDelegate){        
		std::cout << __func__ <<" node " << iter.first << " has " << iter.second->getNGPUs() << " gpus connected" << std::endl;
	}

	ifs.close();
	return EXIT_SUCCESS;
}
