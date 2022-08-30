#include <torch/script.h> // One-stop header
#include <torch/torch.h>
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
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <opencv2/opencv.hpp>
#include <boost/program_options.hpp>

#include "socket.h"
#include "img_utils.h"
#include "torch_utils.h"
#include "common_utils.h" //printTimeStamp moved to here
#include "exp_model.h"

#define IMAGENET_ROW 224
#define IMAGENET_COL 224
#define BATCH_BUFFER 10 // buffer used when reading, and chosing random data
#define FLUX_INTERVAL 20 // interval of changing random interval when fluctuating reqeust rate

namespace po = boost::program_options; 

// logging related variables
uint64_t *gp_reqStartTime;
uint64_t *gp_reqEndTime;
std::string g_reqDist;

bool g_useFlux;
bool g_useFluxFile;
std::vector<int> g_randRate;
double g_randMean;
int g_rate;
int g_standRate;
uint64_t g_numReqs;
int g_batchSize;
std::string g_hostName;
int g_portNo;
int g_socketFD;
std::string g_model;

std::vector<cv::Mat>g_inputData;
std::vector<torch::Tensor> g_inputImgTensor;
/*input related global variables*/
bool g_skipResize = false;
bool g_useIMG; // flag indicating whether to use custom image loader, imagenet, camera data use this
bool g_useMNIST; // flag indicating whether to use mnist data loader or not.
bool g_useNLP; // flag indicating whether to use nlp data or not FOR NOW we just use random data

//threads and initFunctions
pthread_t initRecvThread();  // inits thread for receiving results from the server
pthread_t initSendThread(); // sends to the server

void *recvRequest(void *vp);  
void *sendRequest(void *vp);

const char* gc_charModelName_p;

std::vector<torch::Tensor> g_MNISTData; // global vector storing mnist data
std::vector<torch::Tensor> g_tokenData;

po::variables_map parse_opts(int ac, char** av) {
	po::options_description desc("Allowed options");
	desc.add_options()("help,h", "Produce help message")
		("task,t", po::value<std::string>()->default_value("alexnet"),
		 "name of task: game, traffic, model name")
		("portno,p", po::value<int>()->default_value(8080), "Server port")
		("hostname,o",po::value<std::string>()->default_value("localhost"),"Server IP addr")
		("batch,b", po::value<int>()->default_value(1),"size of batch to send") 
		("requests,r",po::value<int>()->default_value(1),"how many requests are going to be issued to the server" ) 
		("rate,m,",po::value<int>()->default_value(100)," rate (in seconds)")
		("input,i",po::value<std::string>()->default_value("input.txt"),"txt file that contains list of inputs")
		("skip_resize,sr", po::value<bool>()->default_value(false), "skip resizing?")
		("root_data_dir", po::value<std::string>()->default_value("./"), "root directory which holds data for generator")
		("dist", po::value<std::string>()->default_value("uni"), "option specifying which dist. to use e.g.) uni, exp, zipf")
		("flux", po::value<bool>()->default_value(false), "flag value indicating whether to fluctuate or not")
		("std_rate",po::value<int>()->default_value(0)," standard rate when flux is enabled (in seconds)")
		("flux_file", po::value<std::string>()->default_value("no_file"), "file holding randon rates to experiment");

	po::variables_map vm;
	po::store(po::parse_command_line(ac, av, desc), vm); 
	po::notify(vm); 
	if (vm.count("help")) {
		std::cout << desc << "\n"; exit(1);   
	} 
	return vm;
}

torch::Tensor getInput(const char* net_name){
	torch::Tensor input;
#ifdef DEBUG
	printf("get input for %s \n", net_name);
#endif
	if (strcmp(net_name, "lenet1") == 0)
	{
		input = torch::randn({g_batchSize,1,28, 28});
	}
	else if(strcmp(net_name,"ssd-mobilenetv1")==0){
		input = torch::randn({g_batchSize,3,300, 300});
	}
	else if(strcmp(net_name,"resnet50")==0 ||strcmp(net_name,"googlenet")==0 ||strcmp(net_name,"vgg16")==0 ){
		input = torch::randn({g_batchSize,3,224, 224});
	}
	else if(strcmp(net_name, "bert")==0){
		auto options = torch::TensorOptions().dtype(torch::kInt64);
		input = torch::randint(/*high=*/500, {g_batchSize, 14},options);
	}
	else{
		printf("unrecognized task: %s \n", net_name);
		exit(1);
	}
	return input;

}

void setupGlobalVars(po::variables_map &vm){
}

void readInputData(po::variables_map &vm){

}



pthread_t initSendThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);   
	pthread_attr_setstacksize(&attr, 1024 * 1024); 
	pthread_t tid;
	if (pthread_create(&tid, &attr, sendRequest, NULL) != 0)   
		LOG(ERROR) << "Failed to create a request handler thread.\n";   
	return tid;
}


void *sendRequest(void *vp){
	return (void*)0;
    
}

pthread_t initRecvThread() {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, recvRequest, NULL) != 0)
		LOG(ERROR) << "Failed to create a request handler thread.\n";
	return tid;

}
void* recvRequest(void *vp)
{
	return 0;
}

int main(int argc, char** argv) {
        printTimeStampWithName(gc_charModelName_p, "START PROGRAM");

        /*get parameters for this program*/
        po::variables_map vm = parse_opts(argc, argv);
        setupGlobalVars(vm);
#ifdef DEBUG
        std::cout << "Finished setting up global variables" << std::endl;
#endif
        readInputData(vm);
#ifdef DEBUG
        std::cout << "Finished reading input data" << std::endl;
#endif

        float *preds = (float*)malloc(g_batchSize * sizeof(float));
        /*start threads*/
        pthread_t sendThreadID;
        pthread_t recvThreadID;
        g_socketFD = client_init((char*)g_hostName.c_str(), g_portNo, false);
        if (g_socketFD < 0) exit(0);
        int opt_val=1;
        setsockopt(g_socketFD,IPPROTO_TCP,TCP_NODELAY, (void*)&opt_val, sizeof(int));
        printf("connected to server!\n");
        gp_reqStartTime = (uint64_t *)malloc(g_numReqs * sizeof(uint64_t));
        gp_reqEndTime = (uint64_t *)malloc(g_numReqs * sizeof(uint64_t));
        sendThreadID = initSendThread();
        recvThreadID = initRecvThread();
        pthread_join(sendThreadID, NULL);
        pthread_join(recvThreadID, NULL);
        socket_close(g_socketFD, false);
        for( int i=0; i<g_numReqs; i++){
                std::cout << "Respond Time: " << std::to_string(double(gp_reqEndTime[i]-gp_reqStartTime[i])/1000000) << " for Request ID "<< i<<std::endl;
        }
        printTimeStampWithName(gc_charModelName_p, "END PROGRAM");
        return 0;
}

