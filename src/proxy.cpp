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
	free(q->p_reqProf);
	q->output.reset();
	free(q);
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
	mtx.lock();
	queue.push(input_elem);
	mtx.unlock();
	cv.notify_all();
}


void* recvInput(void* vp){
	printTimeStampWithName(cp_str_recv, cp_str_start);
	int server_sock, rc;
	socklen_t len;
	int i;
	int bytes_rec = 0;
	struct sockaddr_un server_sockaddr;
	struct sockaddr_un client_sockaddr;
	int cur_read;
	int backlog = 10;
	bool waiting_input_conn=false;
	QueueElem* q;

	memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
	memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

	std::stringstream sockname;
	sockname<<"/tmp/gpusock_input_"<<g_devID<<"_"<<g_threadCap<<"_"<<g_dedup;

	server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_sock == -1){
		printf("SOCKET ERROR: %s\n", strerror(errno));
		exit(1);
	}   
	server_sockaddr.sun_family = AF_UNIX;
	strcpy(server_sockaddr.sun_path, sockname.str().c_str());
	len=sizeof(server_sockaddr);
	setsockopt(server_sock, SOL_SOCKET,SO_REUSEADDR, NULL, 1);
	unlink(sockname.str().c_str());
	rc = bind(server_sock, (struct sockaddr *) &server_sockaddr, len);
	if (rc == -1){
		printf("BIND ERROR: %s\n", strerror(errno));
		close(server_sock);
		exit(1);
	}

	rc = listen(server_sock, backlog);
	if (rc == -1){ 
		printf("LISTEN ERROR: %s\n", strerror(errno));
		close(server_sock);
		exit(1);
	}
	printTimeStampWithName(server_sockaddr.sun_path, cp_str_listen);
	g_readyFlag_input=true;
	while (1){
		waiting_input_conn=true;
		int input_client_sock = accept(server_sock, (struct sockaddr *) &client_sockaddr, &len);
		if (input_client_sock == -1){
			printf("ACCEPT ERROR: %s\n", strerror(errno));
			close(server_sock);
			close(input_client_sock);
			exit(1);
		}
		waiting_input_conn=false;
		g_input_closed=false;
		printTimeStampWithName( server_sockaddr.sun_path, cp_str_accepted);


		while(1){
			int ret;
			int dimlen=0;
			int buf=0;
			int datalen=0;
			if (ret=read(input_client_sock,&dimlen, sizeof(int)) <=0){

				printf("client Disconnected  OR timed out\n");
				//printTimeStamp("CLOSED INPUT SOCKET");
				std::cout <<"CLOSED PROXY SOCKET AT: " << timeStamp() << std::endl;
				break;
			}
			if(dimlen == CLOSE_SOCKET){
				printf("received close socket from client \n");
				break;
			}
			else if(dimlen == LOAD_MODEL){
				int model_id;
				int batch_size;
				read(input_client_sock,&model_id, sizeof(int));
				read(input_client_sock,&batch_size, sizeof(int));
				printf("received load model %d from client \n", model_id);
				q=new QueueElem();
				q->e_act=LOAD;
				q->job_ID=model_id;
				q->batch_size=batch_size;
				pushToQueue(q, g_ControlQueue, g_CqueueMtx, g_CqueueCV);
				continue;
			}
			else if(dimlen ==UNLOAD_MODEL){
				int model_id;
				read(input_client_sock,&model_id, sizeof(int));
				printf("received unload model %d from client \n", model_id);
				q=new QueueElem();
				q->e_act=UNLOAD;
				q->job_ID=model_id;
				pushToQueue(q, g_ControlQueue, g_CqueueMtx, g_CqueueCV);
				continue;
			}
			else{
				printf("received dimlen: %d \n", dimlen);
			}
			g_receivingFlag=true;
			if(dimlen > 4){
				printf("STRANGE dimlen: %d \n", dimlen);
				printf("continuing execution! \n");
				continue;
			}
			uint64_t start = getCurNs();

			if(dimlen!=0){
				q=new QueueElem();
				q->e_act=RUN;
				if (ret=read(input_client_sock, &buf, sizeof(int)) >0){
					q->job_ID=buf;
				}


				for(int i =0; i <dimlen; i++){
					if ((ret=read(input_client_sock,&buf,sizeof(int))) > 0){
						q->dims.push_back(buf);
					}
				}
				buf=0;
				if (ret=read(input_client_sock, &buf, sizeof(int)) >0){
					q->req_ID=buf;
				}
				buf=0;
#ifdef PROFILE
				q->p_reqProf=new ReqProfile(q->req_ID, q->job_ID);
				q->p_reqProf->setInputStart(getCurNs());
#endif

				uint64_t start2 = getCurNs();           
#ifndef NO_NET
				ret=read(input_client_sock,&datalen,sizeof(int));
				if(g_mappingIDtoInputDataType[q->job_ID] == "FP32"){
					q->p_float32_indata=(float*)malloc(sizeof(float)*datalen);
					if (ret=socket_receive(input_client_sock, (char*)q->p_float32_indata, datalen*sizeof(float), false) <=0){
						printf("ERROR in receiving input data\n ");
					}
				}
				else if(g_mappingIDtoInputDataType[q->job_ID] == "INT64"){
					q->p_int64_indata=(int64_t*)malloc(sizeof(int64_t)*datalen);
					if (ret=socket_receive(input_client_sock, (char*)q->p_int64_indata, datalen*sizeof(int64_t), false) <=0){
						printf("ERROR in receiving input data\n ");
					}
				}
#else
#endif
#ifdef PROXY_LOG
				std::cout <<__func__ <<": " <<timeStamp() << " received input as following: " << std::endl;
				printf("req_ID: %d, job_ID: %d \n", q->req_ID, q->job_ID);
#endif 

				uint64_t end2 = getCurNs();
#ifdef PROXY_LOG
				std::cout << __func__ <<": finished receiving input data" << std::endl; 
#endif
#ifdef PROFILE
				q->p_reqProf->setInputEnd(getCurNs());
#endif 
				g_receivingFlag=false;
				uint64_t end = getCurNs();

				pushToQueue(q, g_InputQueue, g_IqueueMtx, g_IqueueCV);
			}
			else{
				printf("read returned 0. stop reading \n");
				break;
			}
		}// inner loop
		socket_close(input_client_sock,true);
		g_input_closed=true;
		g_OqueueCV.notify_all();
		g_IqueueCV.notify_all();
		g_CqueueCV.notify_all();

		if(g_exitFlag) break;

	}//outer loop
	std::cout << "exiting input thread" << std::endl;  

}

void* loadControl(void *args){
		while(1){
		std::unique_lock<std::mutex> lock(g_CqueueMtx);
		g_CqueueCV.wait(lock, []{return g_ControlQueue.size() || g_exitFlag;});
		if(g_exitFlag){
			break;
		}
		QueueElem *q=g_ControlQueue.front();
		g_ControlQueue.pop();
		if(q->e_act == LOAD) loadModel(q,g_gpu_dev,true);
		else if(q->e_act == UNLOAD) unloadModel(q->job_ID);
		// should not happen
		else{ 
			printf("ERROR! CHECK your code for loadControl \n");
		}
		if(g_ControlQueue.size() == 0 && (gp_proxyCtrl->getProxyState(gp_PInfo) != EXITING) )
		{
			//print time stamp for debugging purposes
#ifdef PROXY_LOG
			std::cout << __func__ << ": marking proxy as RUNNING at " << timeStamp()
				<<std::endl; 
#endif
			gp_proxyCtrl->markProxy(gp_PInfo, RUNNING);
		}
		freeMemory(q);
	}
	std::cout << "exiting loadControl thread" << std::endl;
	
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
	printTimeStampWithName(cp_str_send, cp_str_start);
	int server_sock,rc;
	socklen_t len;
	int i;
	int bytes_rec = 0;
	struct sockaddr_un server_sockaddr; 
	struct sockaddr_un client_sockaddr;
	int cur_read;
	int backlog = 10;

	memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
	memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

	std::stringstream sockname;

	sockname<<"/tmp/gpusock_output_"<<g_devID<<"_"<<g_threadCap<<"_"<<g_dedup;


	server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_sock == -1){
		printf("SOCKET ERROR: %s\n", strerror(errno));
		exit(1);
	}   
	server_sockaddr.sun_family = AF_UNIX;
	strcpy(server_sockaddr.sun_path, sockname.str().c_str());
	len=sizeof(server_sockaddr);

	unlink(sockname.str().c_str());
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,NULL,1) ;
	rc = bind(server_sock, (struct sockaddr *) &server_sockaddr, len);
	if (rc == -1){
		printf("BIND ERROR: %s\n", strerror(errno));
		close(server_sock);
		exit(1);
	}

	rc = listen(server_sock, backlog);
	if (rc == -1){ 
		printf("LISTEN ERROR: %s\n", strerror(errno));
		close(server_sock);
		exit(1);
	}
	printTimeStampWithName(server_sockaddr.sun_path, cp_str_listen);
	g_readyFlag_output=true;
	while(1){
		g_waiting_output_conn=true;
		int output_client_sock = accept(server_sock, (struct sockaddr *) &client_sockaddr, &len);
		if (output_client_sock == -1){
			printf("ACCEPT ERROR: %s\n", strerror(errno));
			close(server_sock);
			close(output_client_sock);
			exit(1);
		}
		g_waiting_output_conn=false;
		printTimeStampWithName(server_sockaddr.sun_path, cp_str_accepted);
		while(1){
			std::unique_lock<std::mutex> lock(g_OqueueMtx);
			g_OqueueCV.wait(lock, []{return g_OutputQueue.size() || g_exitFlag;});
			std::cout << "receiving: " <<  g_receivingFlag << " computing: "<< g_computingFlag <<   " g_InputQueue: " << g_InputQueue.size() 
				<< " g_OutputQueue.size: " << g_OutputQueue.size() << " g_exitFlag: " << g_exitFlag 
				<<std::endl;
			if(!g_receivingFlag && g_InputQueue.size() ==0 && g_OutputQueue.size() ==0 && g_exitFlag){
				break;
			}
			if(g_OutputQueue.empty()){
				lock.unlock();
				continue;
			} 
			 QueueElem* q =g_OutputQueue.front();
#ifdef PROFILE
			q->p_reqProf->setOutputStart(getCurNs());
#endif 
			g_OutputQueue.pop();
			lock.unlock();
			torch::Tensor otensor = q->output;
			int dim = otensor.dim();

			int len=1;
			write(output_client_sock, (void *)&dim,sizeof(int));
			for(int i=0; i<dim; i++){
				int size = otensor.size(i);
				write(output_client_sock, (void *)&size,sizeof(int));
				len*=size;
			}
#ifndef NO_NET
			float *raw_data=(float*)(q->output).data_ptr();
			socket_send(output_client_sock, (char*)raw_data, len*sizeof(float), false); 
#else
#endif
#ifdef PROFILE
			q->p_reqProf->setOutputEnd(getCurNs());
			q->p_reqProf->printTimes();
#endif 

#ifdef PROXY_LOG
			std::cout << __func__ <<": output sent for req_id: " << q->req_ID <<" at: " << timeStamp() << std::endl;
#endif
			freeMemory(q);

		} // inner infinite loop
		socket_close(output_client_sock, true);
		if(g_exitFlag) break;
	}// outer infinit loop
	std::cout << "exiting output thread" << std::endl;
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
	c10::InferenceMode guard;
	uint64_t com_start, com_end;
	com_start=getCurNs();
	printTimeStampWithName(cp_str_comp, cp_str_start);

	std::vector<torch::jit::IValue> inputs;
	torch::Tensor input;
	torch::Tensor t;
	initPyTorch();

	g_readyFlag_compute=true;
	while(!g_readyFlag_input || !g_readyFlag_output || !g_readyFlag_compute){}
	printTimeStampWithName(cp_str_comp, cp_str_done);
	gp_proxyCtrl->markProxy(gp_PInfo, RUNNING);
	com_end = getCurNs();
#ifdef PROXY_LOG
	std::cout << "computing thread took " << double(com_end - com_start)/1000000 << " ms to initiate" << std::endl; 
#endif

	while(1){
		//compute here
		std::unique_lock<std::mutex> lock(g_IqueueMtx);
		g_IqueueCV.wait(lock, []{return g_InputQueue.size() || g_exitFlag;});
		if (g_InputQueue.size() ==0 && !g_receivingFlag && g_exitFlag ){
			lock.unlock();
			break;
		}
		g_computingFlag=true;
		 QueueElem* q=g_InputQueue.front();
		g_InputQueue.pop();
		lock.unlock();
#ifdef PROFILE
		q->p_reqProf->setCPUCompStart(getCurNs());
#endif 
#ifdef PROXY_LOG
		printf("started to execute request ID: %d \n", q->req_ID);
#endif 

#ifndef NO_NET
		if(g_mappingIDtoInputDataType[q->job_ID] == "FP32"){
			auto options(torch::kFloat32);
			t=convert_rawdata_to_tensor(q->p_float32_indata, q->dims, options);
		}
		else if(g_mappingIDtoInputDataType[q->job_ID] == "INT64"){
			auto options(torch::kInt64);
			t=convert_rawdata_to_tensor(q->p_int64_indata, q->dims, options);
		}
#else 
		t=getRandInput(q->job_ID,q->dims[0]);   
#endif


#ifdef PROXY_LOG
		std::cout << __func__ <<": type of tensor " << t.dtype()
			<<std::endl;
		std::cout << __func__ <<": sizes " << t.sizes()
			<<std::endl;
#endif 
		t=t.to(g_gpu_dev);
		std::vector<torch::jit::IValue> inputs;
		inputs.push_back(t);
		{
			int input_batch_size = t.size(0);
#ifdef PROFILE
			printf("setting gpu_start of req_id: %d \n", q->req_ID);
			q->p_reqProf->setGPUStart(getCurNs());
#endif

			torch::IValue temp_output= g_modelTable[q->job_ID]->forward(inputs);
			cudaDeviceSynchronize();
#ifdef PROFILE
			printf("setting gpu_end of req_id: %d \n", q->req_ID);
			q->p_reqProf->setGPUEnd(getCurNs());
			q->p_reqProf->setBatchSize(input_batch_size);
#endif

			torch::Tensor temp;
			if (temp_output.isTensor()){
				temp= temp_output.toTensor();
			}
			else if(temp_output.isTuple() && (q->job_ID == g_mappingFiletoID["ssd-mobilenetv1.pt"])){
				temp=custom::getBoxforTraffic(temp_output.toTuple()->elements().at(0).toTensor(), 
						temp_output.toTuple()->elements().at(1).toTensor(),
						t);
			}
			else if(temp_output.isTuple() && (q->job_ID == g_mappingFiletoID["bert.pt"])){
				// pick the only tuple for output
				temp = temp_output.toTuple()->elements().at(0).toTensor();
			}
			else{ // not supposed to happen
				printf("LOGIC ERROR \n ");
				exit(1);
			}
			temp = temp.to(torch::kCPU);
			temp.detach();
			q->output = temp;

			int output_batch_size = temp.size(0);
			assert(input_batch_size == output_batch_size);
#ifdef PROFILE
			q->p_reqProf->setCPUPostEnd(getCurNs());
#endif
			//send output
			pushToQueue(q,g_OutputQueue, g_OqueueMtx,g_OqueueCV);
			g_computingFlag=false;

		}// dummy scope for lowering shared_ptr use_counts 

	}//infinite loop
	if(g_exitFlag){
		g_OqueueCV.notify_one();
	}
	std::cout << "exiting compute thread" << std::endl;
	
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
#ifdef PROXY_LOG
	std::cout << "[" << g_devID << "," << g_partIdx << "]" << "starting control thread" << std::endl;
#endif 
	while(true){
		proxy_state curr_state;
		curr_state=gp_proxyCtrl->getProxyState(gp_PInfo);
		while(curr_state != EXITING){
			std::cout << "STATE: " << curr_state << " at "<< timeStamp() <<std::endl;
			usleep(1*1000*1000);
			curr_state=gp_proxyCtrl->getProxyState(gp_PInfo);
		}
		g_exitFlag=true;
		g_IqueueCV.notify_one();
		g_OqueueCV.notify_one();
		g_CqueueCV.notify_one();
		break;
	}
	std::cout << "exiting control thread" << std::endl;
	return (void*)0;
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
    std::cout << __func__ << ": reading JSON File: " << configJSON
    <<std::endl;
    Json::Value root;
    std::ifstream ifs;
    ifs.open(configJSON);
    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << __func__ << ": Failed parsing from stream" << std::endl;
        std::cout << errs << std::endl;
        ifs.close();
        return EXIT_FAILURE;
    }
    for(unsigned int i=0; i< root["Models"].size(); i++){
        std::string name = root["Models"][i]["name"].asString();
        int id = root["Models"][i]["proxy_id"].asInt();
		mapping[name]=id;
        #ifdef DEBUG
        std::cout << __func__ <<": setted up " << name.c_str() << " as " << id
        <<std::endl;
        #endif
         for(unsigned int j=0; j<root["Models"][i]["input_dim"].size(); j++ ){
            InputDimMapping[id].push_back(root["Models"][i]["input_dim"][j].asInt());
        }
    }
    ifs.close();
    return EXIT_SUCCESS;
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
