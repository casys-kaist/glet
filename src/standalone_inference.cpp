#include <torch/script.h> // One-stop header.
#include <cuda_runtime.h>

#include <iostream>
#include <string>
#include <memory>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <queue>
#include <condition_variable>
#include <cuda_profiler_api.h>
#include <c10/cuda/CUDACachingAllocator.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "json/json.h"
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include "socket.h"
#include "torch_utils.h"
#include "common_utils.h" //printTimeStamp moved to here
#include <torch/csrc/jit/runtime/graph_executor.h>
#define IMAGENET_ROW 224
#define IMAGENET_COL 224

namespace po = boost::program_options; 
using namespace cv;

bool g_warmupFlag = true; // flag for indicating to do warmup

// mean interval between inference (in seconds)
double g_mean;

// total number of interference executions
int g_numReqs;

// batch size
int g_batchSize;

// name of model
std::string g_task;

// directory to model (.pt) file
std::string g_taskFile;

typedef struct _InputInfo {
	std::vector<std::vector<int>*> InputDims;
	std::vector<std::string> InputTypes;
} InputInfo;

std::map<std::string,InputInfo*> g_nameToInputInfo;

void computeRequest();

po::variables_map parse_opts(int ac, char** av) {
	po::options_description desc("Allowed options");
	desc.add_options()("help,h", "Produce help message")
		("task,t", po::value<std::string>()->default_value("resnet50"), "name of model")
		("taskfile",po::value<std::string>()->default_value("resnet50.pt"), "dir/to/model.pt")
		("batch,b", po::value<int>()->default_value(1),"size of batch to send") 
		("requests,r",po::value<int>()->default_value(1),"how many requests are going to be issued to the server" ) 
		("mean,m,",po::value<double>()->default_value(0.3),"how long is the average time between each request(in seconds)")
		("input,i",po::value<std::string>()->default_value("input.txt"),"txt file that contains list of inputs")
		("input_config_json", po::value<std::string>()->default_value("input_config.json"), "json file for input dimensions");
	po::variables_map vm;
	po::store(po::parse_command_line(ac, av, desc), vm); 
	po::notify(vm); 
	if (vm.count("help")) {
		std::cout << desc << "\n"; exit(1);   
	} 
	return vm;
}


int readInputJSONFile(const char* input_config_file, std::map<std::string, InputInfo*> &mapping){
#ifdef DEBUG
	printf("Reading App JSON File: %s \n", input_config_file);
#endif
	Json::Value root;
	std::ifstream ifs;
	ifs.open(input_config_file);

	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;
	if (!parseFromStream(builder, ifs, &root, &errs)) {
		std::cout << errs << std::endl;
		ifs.close();
		return EXIT_FAILURE;
	}
	for(unsigned int i=0; i < root["ModelInfoSpecs"].size(); i++){
		std::string model_name = root["ModelInfoSpecs"][i]["ModelName"].asString();
		mapping[model_name]=new InputInfo();
		for(unsigned int j=0; j< root["ModelInfoSpecs"][i]["Inputs"].size(); j++){
			mapping[model_name]->InputDims.push_back(new std::vector<int>());
			for(unsigned int k=0; k<root["ModelInfoSpecs"][i]["Inputs"][j]["InputDim"].size(); k++){
				mapping[model_name]->InputDims[j]->push_back(root["ModelInfoSpecs"][i]["Inputs"][j]["InputDim"][k].asInt());
			}
			mapping[model_name]->InputTypes.push_back(root["ModelInfoSpecs"][i]["Inputs"][j]["InputType"].asString());
		}
	}
	ifs.close();
	return EXIT_SUCCESS;
}


void setupGlobalVars(po::variables_map &vm){
	g_task = vm["task"].as<std::string>();
	g_mean = vm["mean"].as<double>();
	g_numReqs=vm["requests"].as<int>();
	g_batchSize= vm["batch"].as<int>();
	assert(g_batchSize!=0);
	g_taskFile = vm["taskfile"].as<std::string>();
	if(readInputJSONFile(vm["input_config_json"].as<std::string>().c_str(), g_nameToInputInfo))
	{
		printf("Failed reading json file: %s \n", vm["input_config_json"].as<std::string>().c_str());
		exit(1);
	}
	return;
}

int main(int argc, char** argv) {
	torch::jit::getBailoutDepth() = 0;
	torch::jit::getProfilingMode() = false;
	/*get parameters for this program*/
	po::variables_map vm = parse_opts(argc, argv);
	setupGlobalVars(vm);
	printTimeStamp("START PROGRAM");
	computeRequest();   
	printTimeStamp("END PROGRAM");
	return 0;
}

void computeRequest(){
}
