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


void PyTorchInit(){
	uint64_t total_end, total_start;
	std::vector<torch::jit::IValue> inputs;
	std::vector<int64_t> sizes={1};
	torch::TensorOptions options;
	options = torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCUDA,0).requires_grad(false);
	total_start = getCurNs();
	torch::Tensor dummy1 = at::empty(sizes,options);
	torch::Tensor dummy2 = at::empty(sizes,options);
	torch::Tensor dummy3 = dummy1 + dummy2;
	cudaDeviceSynchronize();
	total_end = getCurNs();
	std::cout << double(total_end - total_start)/1000000 << " PyTorchInit total ms "<<std::endl;
	return;
}

void getInputs(const char* netname, std::vector<torch::jit::IValue> &inputs, int batch_size){
	torch::Tensor input;
	torch::Device gpu_dev(torch::kCUDA,0);
#ifdef DEBUG
	printf("get input for %s \n", netname);
#endif 
	std::string c_str_bert = "bert";
	std::string str_name = std::string(netname);        
	// assume this model is for profiling random model
	if(g_nameToInputInfo.find(str_name) == g_nameToInputInfo.end()){
		std::string c_str_fp32 = "FP32";
		std::vector<int> DIMS = {3,224,224};
		//input = getRandomImgInput(DIMS,c_str_fp32,batch_size);
	}
	else{
		for(unsigned int i=0; i < g_nameToInputInfo[str_name]->InputDims.size(); i++){
			if(str_name.find(c_str_bert) != std::string::npos)
				//input = getRandomNLPInput(*(g_nameToInputInfo[str_name]->InputDims[i]), g_nameToInputInfo[str_name]->InputTypes[i]);
			//else
				//input = getRandomImgInput(*(g_nameToInputInfo[str_name]->InputDims[i]), g_nameToInputInfo[str_name]->InputTypes[i], batch_size);
		}
	}
	input = input.to(gpu_dev);
	inputs.push_back(input);
	return;
}

void computeRequest(){
	#ifdef DEBUG
	std::cout<<"started copmuting thread"<<std::endl;
#endif
	torch::Tensor input;
	std::vector<torch::jit::IValue> inputs;
	torch::Device gpu_dev(torch::kCUDA,0);
	uint64_t total_end, total_start;
	const char *netname = g_task.c_str();
	int i;
	PyTorchInit();
	std::cout<< "waiting for 3 seconds after PyTorchInit" << std::endl;
	usleep(3*1000*1000);
	uint64_t t1,t2,t3,t4;
	t1 = getCurNs();
	std::shared_ptr<torch::jit::script::Module> module = std::make_shared<torch::jit::script::Module>(torch::jit::load(g_taskFile.c_str(),gpu_dev));
	t2 = getCurNs();
	module->to(gpu_dev);
	module->eval();
	cudaDeviceSynchronize();
	t3= getCurNs();       
	// warmup
	for(int batch_size = 32; batch_size >=1; batch_size--){
		getInputs(netname, inputs,batch_size);
		module->forward(inputs);
		cudaDeviceSynchronize();
		inputs.clear();
	}
	t4 = getCurNs();
	std::cout<< "waiting for 3 seconds after warmup" << std::endl;
	usleep(3*1000*1000);

	std::cout << "main jit-load: " << double(t2-t1)/1000000 <<\
		"warmup: " << double(t3-t4)/1000000 << \
	std::endl;

	for (int i =0; i < g_numReqs; i++){
		usleep(g_mean * 1000* 1000);
		getInputs(netname,inputs,g_batchSize);
		printTimeStampWithName(netname, "START EXEC");
		inputs.clear();
	}
}
