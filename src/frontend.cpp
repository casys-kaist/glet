#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <map>
#include <time.h>
#include <queue>
#include <vector>
#include <netinet/tcp.h>
#include <glog/logging.h>
#include "global_scheduler.h"
#include "boost/program_options.hpp"
#include "socket.h"
#include "thread.h"
#include "request.h"
#include "batched_request.h"
#include "common_utils.h"
#include "input.h"
#include "backend_setup.h"
#include "backend_delegate.h"

#define MAX_TASK 3
#define THREAD_PER_TASK 2 * MAX_TASK

namespace po = boost::program_options;
SysMonitor ServerState; 
const int TOTAL_SENDTHREAD=8;
const int TOTAL_OUTPUT_THREAD_PER_PROXY=2;

// declared in router_setup.cpp
extern std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

GlobalScheduler gScheduler;
FILE* pLogFile;
FILE* pAppLogFile;
bool finishedMPSProxyInit;

po::variables_map ParseOpts(int argCount, char** argVar){
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "Produce help message")
    ("resource_dir, rd", po::value<std::string>()->default_value("../resource"), "directory which hold resource files" )
    ("portno,p", po::value<int>()->default_value(8080),"Port to open DjiNN on")
    ("scheduler,s",po::value<std::string>()->default_value("no"),"scheduling mode : no, greedy, dedicated, proposed")
    ("config_json, cj", po::value<std::string>()->default_value("config.json"), "json file which holds configuration for experiment" )
    ("proxy_json, pj", po::value<std::string>()->default_value("proxy_config.json"), "json file which holds configuration for models on proxy server" )
    ("model_list", po::value<std::string>()->default_value("ModelList.txt"), "list of model and configuration of proxys" )
    ("sim_config", po::value<std::string>()->default_value("sim-config.json"), "json file which holds configurations for scheduler" )
    ("mem_config", po::value<std::string>()->default_value("mem-config.json"), "json file which holds profiled memory usage of each model" )
    ("latency_profile", po::value<std::string>()->default_value("latency.csv"), "list of model and configuration of proxys" )
    ("init_rate", po::value<std::string>()->default_value("rates.csv"), "list of initial rate for each model" )
    ("backend_json", po::value<std::string>()->default_value("Backstd::endlist.json"), "list of backends" )
    ("device_config, dc", po::value<std::string>()->default_value("device-config.json"), "json file which holds profiled info of per-device performance" )
    ("drop", po::value<double>()->default_value(0.0), " amount of slack before it is dropped, lower means strict, minus=SLO violated " )
    ("emulation_node_number", po::value<int>()->default_value(0), "number of backend nodes the frontend will emulate" );


  po::variables_map vm;
  po::store(po::parse_command_line(argCount, argVar, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
          std::cout << desc << "\n";
          exit(1);
  }
  return vm;
}

int receiveClients(int socketfd){
  listen(socketfd, 20);
  LOG(ERROR) << "Server is listening for requests on " << socketfd <<std::endl;

  //socket option to DISABLE delay
  const int iOptval = 1;
  while (true){
    pthread_t new_thread_id;
    int client_sock=accept(socketfd, (sockaddr*)0, (unsigned int*)0);
    if( client_sock == -1) {
      LOG(ERROR) << "Failed to accept.\n";
      continue;
    }              
    if(setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &iOptval, sizeof(int)))
    {
        LOG(ERROR) << "set socket option failed" ;
        continue;       
    }
    new_thread_id = initRequestThread(client_sock);
  }
  return 0;
}

void initDropThread(po::variables_map &vm, SysMonitor &state){
    double drop_ratio = vm["drop"].as<double>();
    assert(0<= drop_ratio);
    if(drop_ratio) {// if drop ratio is not 0, initiate thread
       initDropThread(&drop_ratio);
    }
}

void initResultThreads(const int number_of_result_threads){
    for (int j=0; j<number_of_result_threads; j++){
        initSendResultsThread();
    }
}

void intitiatePerformanceLogs(){
  pLogFile=fopen("log.txt","w");
  fprintf(pLogFile,"timestamp,task,BatchID,ReqID,deviceID,QUEUEING_DELAY,WAIT_EXEC,BATCH_LAT,EXEC,TO CMPQ,CMPQ_WRITE\n");
  fflush(pLogFile);
  pAppLogFile=fopen("Applog.txt","w");
  fprintf(pAppLogFile,"timestamp,task,TaskID,REQ_TO_EXEC,EXEC\n");
  fflush(pAppLogFile);
}

// initiates and launches all performance moniotring related threads
void initiateMonitoring(SysMonitor &state){
  intitiatePerformanceLogs();
  initMonitorThread();
  initGPUMonitorThread();
	initUtilMonitorThread();
}

void initProxyThreads(int nGPUs){
  #ifdef FRONTEND_DEBUG
  std::cout << __func__ << ": called!"
  << std::endl;
  #endif

  for (int i =0; i<nGPUs;i++){           
      for(auto it : *ServerState.getDevMPSInfo(i)){
      #ifdef FRONTEND_DEBUG
        printf("[PROXY_SETUP][%d,%d,%d] number of threads: %u \n",it->dev_id, it->cap, it->dedup_num, THREAD_PER_TASK);
      #endif 
      for(int j =0; j<THREAD_PER_TASK ;j++){
        finishedMPSProxyInit=false;
        initProxyThread(it);
        while(!finishedMPSProxyInit) usleep(1*1000);
      }
      
    }       
  }
}

void initOutputThreads(int nGPUs){

  for (int i =0; i<nGPUs;i++){           
    for(auto it : *ServerState.getDevMPSInfo(i)){
        for(int i =0; i < TOTAL_OUTPUT_THREAD_PER_PROXY; i++){
            initOutputThread(it);               
        }
    }
  }
}

int configureProxys(BackendDelegate *backend_del, int node_id, SysMonitor *SysState){
  #ifdef FRONTEND_DEBUG
  std::cout << __func__ << ": called for node_id : " << node_id
  << std::endl;
  #endif
  gScheduler.createProxyInfos(SysState,backend_del->getSizes(),node_id,backend_del->getNGPUs(),backend_del->getType());
  #ifdef FRONTEND_DEBUG
	 for(int i =0; i < ServerState.getNGPUs(); i++){
        for(auto pPInfo : *ServerState.getDevMPSInfo(i)){
            std::cout << __func__ << ": configured " << proxy_info_to_string(pPInfo)
            << std::endl;
       }
  }
	#endif

  return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
  // Main thread for the server
  po::variables_map vm = ParseOpts(argc, argv);
  gScheduler.setupModelwithJSON(vm["config_json"].as<std::string>().c_str(),vm["resource_dir"].as<std::string>(),gScheduler);
  configureAppSpec(vm["config_json"].as<std::string>(),vm["resource_dir"].as<std::string>(),ServerState);
  // just wait for a few seconds, not sure why it prevent seg-faults but it works
  usleep(1*1000*1000);
  std::cout << "AppSpec configuration complete!" << std::endl;
  std::unordered_map<int, std::vector<uint64_t>> dummy;
  readInputDimsJsonFile(vm["proxy_json"].as<std::string>().c_str(), ServerState,dummy);
  usleep(1*1000*1000);
  std::cout << "model id to name mapping complete!" << std::endl;

  int num_of_emulated_nodes=vm["emulation_node_number"].as<int>();
  assert(num_of_emulated_nodes>=0);
  //ServerState.nEmulBackendNodes=num_of_emulated_nodes;
  // read backend end list, connect to backend control channels, get number of gpus,proxys
  
  if(connectBackendList(vm["backend_json"].as<std::string>())){
      std::cout<<"Failed to connect backends" << std::endl;
      exit(1);
  }
  std::cout << "==== CONNECTING BACKEND DONE! ==== " << std::endl; 
  
  // setup proxys 
  //ServerState.nGPUs=0;
  ServerState.setNGPUs(0);
  for(auto backend_pair : NodeIDtoBackendDelegate){
    configureProxys(backend_pair.second, backend_pair.first,&ServerState);
  }
  
  gScheduler.setupLatencyModel(vm["latency_profile"].as<std::string>());
	gScheduler.setupInitTgtRate(vm["init_rate"].as<std::string>());
  std::map<std::string,std::string> param_files;
  param_files["model_list"]=vm["model_list"].as<std::string>();
  param_files["sim_config"]=vm["sim_config"].as<std::string>();
  param_files["mem_config"]=vm["mem_config"].as<std::string>();
  param_files["proxy_json"] = vm["proxy_json"].as<std::string>();
  param_files["device_config"]=vm["device_config"].as<std::string>();
  gScheduler.setupScheduler(&ServerState, param_files,vm["scheduler"].as<std::string>(), vm["resource_dir"].as<std::string>());
  std::cout << "==== CONFIGURING SCHEDULER DONE! ====" << std::endl; 


  //run scheduling with intial rate and configured proxys
  // setup threads
  initProxyThreads(ServerState.getNGPUs());
  initOutputThreads(ServerState.getNGPUs()); 
  initiateMonitoring(ServerState);
  initDropThread(vm,ServerState);
  initResultThreads(TOTAL_SENDTHREAD);

  //for now just hard-code load balancing mode
  gScheduler.setLoadBalancingMode("wrr");
  gScheduler.setupBackendProxyCtrls();
  if(vm["scheduler"].as<std::string>() == "mps_static"){
    gScheduler.readAndBoot(&ServerState, vm["model_list"].as<std::string>());
  }
  else{
    gScheduler.oneshotScheduling(&ServerState);
  }

  std::cout << "==== INITIAL SCHEDULING DONE! ===="<< std::endl; 
   
  initServerThread(ServerState.getNGPUs());

// receive clients
  return receiveClients(server_init(vm["portno"].as<int>()));
}
