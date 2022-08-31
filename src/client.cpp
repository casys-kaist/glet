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

std::vector<int> readTraceFile(std::string path_to_trace_file){
	std::ifstream infile(path_to_trace_file);
	std::vector<int> ret_vec;
	std::string line;
	while(std::getline(infile,line))
	{
		std::istringstream iss(line);
		float a;
		if (! (iss >> a)) {break;}
		ret_vec.push_back(int(a));

	}
	return ret_vec;
}


void setupGlobalVars(po::variables_map &vm){
	g_skipResize = vm["skip_resize"].as<bool>(); 
	g_rate =  vm["rate"].as<int>();
	assert(g_rate!=0);
	g_randMean = double(1.0)/ g_rate;
	g_model  = vm["task"].as<std::string>(); 
	gc_charModelName_p = g_model.c_str();
	g_useFluxFile=false;
	g_useFlux = vm["flux"].as<bool>();
	if(g_useFlux){
		if(vm["flux_file"].as<std::string>() != "no_file"){
			g_useFluxFile=true;
			std::cout << "USING flux file: " << vm["flux_file"].as<std::string>() << std::endl;
			g_randRate=readTraceFile(vm["flux_file"].as<std::string>());
		}
		else{
			std::cout<<"Must specify flux file when flag flux is set!" << std::endl;
			exit(1);
		}

	}
	g_standRate=0;

	g_reqDist= vm["dist"].as<std::string>();
	g_numReqs=vm["requests"].as<int>();
	if(g_model == "lenet1"){
		g_useIMG=0;
		g_useMNIST=1;
		g_useNLP=0;
	}
	else if(g_model == "game"){
		g_useIMG=1;
		g_useMNIST=1;
		g_useNLP=0;
	}
	else if(g_model == "bert"){
		g_useIMG=0;
		g_useMNIST=0;
		g_useNLP=1;   
	}
	else { // traffic, ssd-mobilenetv1, and etc.
		g_useIMG=1;
		g_useMNIST=0;
		g_useNLP=0;   

	}
	g_batchSize= vm["batch"].as<int>();
	assert(g_batchSize!=0);
	g_hostName = vm["hostname"].as<std::string>();
	g_portNo = vm["portno"].as<int>();
}


std::vector<cv::Mat> readImgData(const char *path_to_txt, int batch_size, std::string data_root_dir){
	std::ifstream file(path_to_txt);
	std::string img_file;
	std::vector<cv::Mat> imgs;
	for (int i = 0; i < batch_size; i++)
	{
		if (!getline(file, img_file))
			break;
		cv::Mat img;
		std::string full_name = data_root_dir + "/" + img_file;
		img = cv::imread(full_name, cv::IMREAD_COLOR);
		if (img.empty())
			LOG(ERROR) << "Failed to read  " << full_name << "\n";
		#ifdef DEBUG
		LOG(ERROR) << "dimensions of " << full_name << "\n";
		LOG(ERROR) << "size: " << img.size() << "\n"
			<< "row: " << img.rows << "\n"
			<< "column: " << img.cols << "\n"
			<< "channel: " << img.channels() << "\n";
		#endif
		imgs.push_back(img);
	}
	if (g_batchSize < 1) {LOG(FATAL) << "No images read!"; exit(1);}

	std::vector<cv::Mat>::iterator it;
#ifdef DEBUG
	LOG(ERROR) << "read " <<g_batchSize << "images \n";
#endif
	return imgs;    
}

std::vector<torch::Tensor> readMNISTData(std::string data_root){
	std::vector<torch::Tensor> ret_vector;
	auto mnist_data_loader = torch::data::make_data_loader(
			torch::data::datasets::MNIST(data_root+"/mnist").map(
				torch::data::transforms::Stack<>()),
			/*batch_size=*/1);
	for (torch::data::Example<>& batch : *mnist_data_loader) {
		torch::Tensor temp = batch.data;
		ret_vector.push_back(temp);
	}
	return ret_vector;
}

void readInputData(po::variables_map &vm){
	/*reads image data*/
	// reads BATCH_BUFFER times more images than it will be read
	if(g_useIMG){
		std::string path_to_txt = vm["input"].as<std::string>();
		g_inputData = readImgData(path_to_txt.c_str(), g_batchSize*BATCH_BUFFER, vm["root_data_dir"].as<std::string>());
		std::cout << "Read " << g_inputData.size() << " images " << std::endl;
		std::vector<cv::Mat> input_images;
		for (unsigned int i = 0; i < g_inputData.size(); i++)
		{
			input_images.clear();
			torch::Tensor input;
			input_images.push_back(g_inputData[i]);
			if (!g_skipResize)
				input = serialPreprocess(input_images, IMAGENET_ROW, IMAGENET_COL);
			else
				input = convert_images_to_tensor(input_images);
			g_inputImgTensor.push_back(input);
		}
	}

	/*reads mnist data set*/
	if(g_useMNIST){
#ifdef  DEBUG
		std::cout << "READ MNIST" << std::endl;
#endif
		std::string DATA_ROOT= vm["root_data_dir"].as<std::string>();
		g_MNISTData = readMNISTData(DATA_ROOT);
	}

	/*generates NLP data*/
	// for now we generate a random vector for input data(simulating token IDs)
	if(g_useNLP){
		generateTokenIDs(g_tokenData, g_model);
	}


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