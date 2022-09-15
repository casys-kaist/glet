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
#include <mutex>
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>
#include <glog/logging.h>
#include <c10/cuda/CUDACachingAllocator.h>
#include "socket.h"
#include "json/json.h"
#include "torch_utils.h"
#include "common_utils.h"
#include "custom_ops.h"
#include "config.h"
#include "profile.h"
#include "proxy_ctrl.h"
#include "shmem_ctrl.h"
#include "gpu_proxy.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <execinfo.h>
#include <torch/csrc/jit/runtime/graph_executor.h>

#define INIT_MALLOC_SIZE 32*3*300*300
#define MAX_BATCH 32

namespace po=boost::program_options;

enum action {LOAD=0, RUN=1, UNLOAD=2}; 

typedef struct _QueueElem{
	int req_ID;
	int job_ID;
	int batch_size;
	std::vector<int64_t> dims;
	float* p_float32_indata;
	int64_t* p_int64_indata;
	torch::Tensor output;
	action e_act;
	ReqProfile *p_reqProf;
} QueueElem;


std::mutex g_IqueueMtx;
std::condition_variable g_IqueueCV;
std::queue< QueueElem*> g_InputQueue;
std::mutex g_OqueueMtx;
std::condition_variable g_OqueueCV;
std::queue< QueueElem*> g_OutputQueue;
std::mutex g_CqueueMtx;
std::condition_variable g_CqueueCV;
std::queue< QueueElem*> g_ControlQueue;
ProxyCtrl *gp_proxyCtrl;
proxy_info *gp_PInfo;

// below are flags for controlling proxy 
bool g_exitFlag = false;
bool g_computingFlag = false;
bool g_receivingFlag = false;
bool g_readyFlag_input=false;
bool g_readyFlag_output=false;
bool g_readyFlag_compute=false;
bool g_waiting_output_conn=true;
bool g_input_closed=true;


int g_devID;
int g_threadCap;
int g_dedup;
std::string g_commonDir;
int g_partIdx;
torch::Device g_gpu_dev(torch::kCUDA,0);


const char *cp_str_send="SEND";
const char *cp_str_recv="RECV";
const char *cp_str_comp="COMP";
const char *cp_str_listen="LISTENING";
const char *cp_str_accepted="ACCEPTED";
const char *cp_str_done="INIT DONE";
const char *cp_str_start="INIT START";

std::vector<int> g_loadedModelIDs; 
std::map<std::string, int> g_mappingNametoID;
std::unordered_map<std::string, int> g_mappingFiletoID;
std::map<int, std::shared_ptr<torch::jit::script::Module>> g_modelTable;
std::unordered_map<int, std::vector<uint64_t>> g_inputDimMapping;
std::unordered_map<int,std::string> g_mappingIDtoInputDataType;
std::unordered_map<int,std::string> g_mappingIDtoOutputDataType;


void freeMemory(QueueElem* q){
}
torch::Tensor getRandInput(int id, int batch_size){
}


void warmupModel(int id, int max_batch_size, torch::Device &gpu_dev)
{

}

po::variables_map parseOpts(int ac, char** av) {
	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()("help,h", "Produce help message")
		("common,com", po::value<std::string>()->default_value("../../pytorch-common/"),"Directory with configs and weights")
		("devid,d", po::value<int>()->default_value(-1),"Device ID")
		("threadcap,tc", po::value<int>()->default_value(100),"thread cap(used for designation)")
		("dedup,dn", po::value<int>()->default_value(0),"identifier between same device and cap")
		("config,cj", po::value<std::string>()->default_value("../proxy_config.json"),"file for configuring protocol")
		("model_list", po::value<std::string>()->default_value("../ModelList.txt"),"file of a list of models to load")
		("partition", po::value<int>()->default_value(0),"index of proxy in GPU")
		("ngpu", po::value<int>()->default_value(2),"the total number of GPUs in this node(used in shared memory)")
		("npart", po::value<int>()->default_value(7),"the total number of possible partitions(used in shared memory)");
	po::variables_map vm;
	po::store(po::parse_command_line(ac, av, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		exit(1);
	}
	return vm;
}

void initPyTorch(){
}

void unloadModel(int id){
}

// you can use this after you called unload too, just like "reload"
void loadModel(QueueElem*  q, torch::Device gpu_dev,bool warmup=false){ 

}


void pushToQueue(QueueElem* input_elem, std::queue<QueueElem*> &queue, std::mutex &mtx, std::condition_variable  &cv){
}


void* recvInput(void* vp){

}

void* loadControl(void *args){
	
}


pthread_t initLoadControlThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024);
	pthread_t tid;

	if(pthread_create(&tid, &attr, loadControl, NULL)!=0){
		printf("initInputThread: Error\n");
	}
	return tid;
}

pthread_t initInputThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024);
	pthread_t tid;

	if(pthread_create(&tid, &attr, recvInput, NULL)!=0){
		printf("initInputThread: Error\n");
	}
	return tid;
}

void* send_output(void* vp){

}


pthread_t initOutputThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024);
	pthread_t tid;

	if(pthread_create(&tid, &attr, send_output, NULL)!=0){
		printf("initOutputThread: Error\n");
	}
	return tid;
}

void* compute(void* vp){
	
}

pthread_t initCopmuteThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024);
	pthread_t tid;

	if(pthread_create(&tid, &attr, compute, NULL)!=0){
		printf("initCopmuteThread: Error\n");
	}
	return tid;
}
void* control(void *args){

}
pthread_t initControlThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024);
	pthread_t tid;
	if(pthread_create(&tid, &attr, control, NULL)!=0){
		printf("initControlThread: Error\n");
	}
	return tid;
}

int readInputDimsJsonFile(const char *configJSON, std::map<std::string, int> &mapping, std::unordered_map<int, std::vector<uint64_t>> &InputDimMapping){
  
}


int main(int argc, char** argv){    
	torch::jit::getBailoutDepth() == false;
	uint64_t main_start, main_end;
	torch::jit::getProfilingMode();
	main_start=getCurNs();
	pthread_t compute,control, send, recv, load_control;
	po::variables_map vm=parseOpts(argc, argv);
	g_devID=vm["devid"].as<int>();
	g_dedup=vm["dedup"].as<int>();
	g_threadCap=vm["threadcap"].as<int>();
	g_commonDir=vm["common"].as<std::string>() + "models/";
	g_partIdx = vm["partition"].as<int>();
	gp_PInfo = new proxy_info();
	gp_PInfo->dev_id=g_devID;
	gp_PInfo->partition_num = g_partIdx;

	gp_proxyCtrl= new ProxyCtrl(/*clear_memory=*/false);
	gp_proxyCtrl->markProxy(gp_PInfo, BOOTING);

	readInputDimsJsonFile(vm["config"].as<std::string>().c_str(), g_mappingNametoID, g_inputDimMapping);
	readInputTypesJSONFile(vm["config"].as<std::string>().c_str(), g_mappingIDtoInputDataType, g_mappingIDtoOutputDataType);

	for(auto pair : g_mappingNametoID ){
		std::string file_name = pair.first + ".pt";
		g_mappingFiletoID[file_name] = pair.second;
	}
	std::stringstream ss;
	ss<<"/tmp/nvidia-mps";
	if(g_devID<4){
		setenv("CUDA_MPS_PIPE_DIRECTORY", ss.str().c_str(), 1);
	}
	recv=initOutputThread();
	send=initInputThread();
	pthread_detach(send);
	main_end=getCurNs();
#ifdef PROXY_LOG
	std::cout << "main to initCopmuteThread took: " << double(main_end-main_start)/1000000 <<" ms" <<std::endl;
#endif

	compute=initCopmuteThread();
	load_control = initLoadControlThread(); 
	pthread_detach(load_control);
	control=initControlThread();
	pthread_join(control, NULL);    
	pthread_join(compute, NULL);
	if(!g_waiting_output_conn) pthread_join(send, NULL);
	gp_proxyCtrl->markProxy(gp_PInfo,FLUSHED);
	while(!g_input_closed){usleep(1*1000);}
	gp_proxyCtrl->markProxy(gp_PInfo, COLD);
	std::cout << "MAIN thread reached return" << std::endl;
	return 0;
}
