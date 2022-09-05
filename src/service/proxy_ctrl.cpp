#include "proxy_ctrl.h"
#include "global_scheduler.h"
#include "socket.h"
#include "gpu_utils.h"
#include "gpu_proxy.h"
#include <iostream>
#include <unistd.h>

#define WAIT_MILLI 1 // interval for sleeping, when busy-waiting.

typedef struct _boot_args {
	proxy_info* pPInfo;
	int ngpu;
	int nparts;
	std::string proxy_full_dir;
	std::string proxy_sh;
	const char *MODEL_LIST="../ModelList.txt";
} boot_args;



void bootProxy(int devid, int cap, int dedup_num, int part_index, int ngpu, int nparts, std::string PROXY_DIR, std::string PROXY_SH, const char* MODEL_LIST){
	pid_t pid = 0;
	int   status;
	std::string proxy_sh = PROXY_DIR+"/"+PROXY_SH;
	std::string shell_cmd = proxy_sh;
	shell_cmd = shell_cmd + " "+std::to_string(devid)+" "+std::to_string(cap)+" "+std::to_string(dedup_num) + " ";
	shell_cmd = shell_cmd + std::string(MODEL_LIST) + " " + std::to_string(part_index) + " "; 
	shell_cmd = shell_cmd + std::to_string(ngpu) + " " + std::to_string(nparts);
	chdir(PROXY_DIR.c_str());
#ifdef DEBUG
	std::cout << "[bootProxy] shell cmd : " << shell_cmd << std::endl;
#endif
	//the following DOES NOT RETURN, unless the proxy server exits
	system(shell_cmd.c_str());
	return;
}

void* bootProxy(void *args){
	boot_args *pargs = (boot_args*)args;
	proxy_info *pPInfo = pargs->pPInfo;
	int ngpu = pargs->ngpu;
	int nparts = pargs->nparts;

	//the following DOES NOT RETURN, unless the proxy server exits
	bootProxy(pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num,pPInfo->partition_num,ngpu,nparts,pargs->proxy_full_dir,pargs->proxy_sh,pargs->MODEL_LIST);
	return (void*)0;
}

// thread used for boooting proxy server, thread is NOT waited for return, so use it wisely 
pthread_t bootProxyThread(proxy_info* pPInfo, int ngpu, int nparts, std::string proxy_full_dir, std::string proxy_sh){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future for now set to max * 1MB
	pthread_t tid;
	boot_args *pargs = new boot_args();
	pargs->pPInfo = pPInfo;
	pargs->nparts=nparts;
	pargs->ngpu=ngpu;    
	pargs->proxy_full_dir=proxy_full_dir;
	pargs->proxy_sh=proxy_sh;
	if (pthread_create(&tid, &attr, bootProxy, (void *)pargs) != 0)
		LOG(ERROR) << "Failed to create a batching thread.\n";
	return tid;
}

ProxyCtrl::ProxyCtrl(bool clear_memory){
	_shmanager = new ShMemManager();
	if(_shmanager->initShMem(clear_memory)){
		std::cout << "[ProxyCtrl] initiating shared memory failed " << std::endl;
		exit(1);
	}
	else{
		std::cout << "[ProxyCtrl] shared memory initiated! " << std::endl;
	}
	_org_dir = getenv("PWD");
	mGPUUtil = new GPUUtil();
}
ProxyCtrl::~ProxyCtrl(){
}

int ProxyCtrl::startProxy(proxy_info* pPInfo, int ngpus, int nparts){
	proxy_state curr_state;
	// wait if the proxy has not finished yet, or in another state
	_shmanager->getProxyState(curr_state,pPInfo->dev_id,pPInfo->partition_num);
#ifdef BACKEND_DEBUG
	std::cout << "[startProxy] [" << pPInfo->dev_id << "," << pPInfo->cap << "," << pPInfo->dedup_num << "] current state: " << curr_state
		<<std::endl;
#endif
	if(curr_state == BOOTING){
		std::cout << "[startProxy] [" << pPInfo->dev_id << "," << pPInfo->cap << "," << pPInfo->dedup_num << "]" << "is already booting"<< std::endl;
		return EXIT_SUCCESS;
	}
	while(curr_state != COLD){ // if exiting, then wait until it becomes cold
		usleep(WAIT_MILLI*1000);
		_shmanager->getProxyState(curr_state,pPInfo->dev_id,pPInfo->partition_num);
	}

	while(true){
		if(_chdir_mtx.try_lock()){
			std::cout << "[startProxy] booting gpu " << pPInfo->dev_id << ", " << pPInfo->partition_num << "th partition" << std::endl;
			pthread_t tid = bootProxyThread(pPInfo,ngpus,nparts,_PROXY_FULL_DIR,_PROXY_SH);
			// detach since we will not join this thread in future
			pthread_detach(tid);
			//wait unitl proxy starts to boot to prevent race conditions for MPS control daemon
			waitProxy(pPInfo,BOOTING);
			chdir(_org_dir);
			_chdir_mtx.unlock();
			std::cout<<"[startProxy] ended calling boot"  << std::endl;
			break;
		}
		else{
			usleep(1000*1000);
		}
	}
	return EXIT_SUCCESS;
}

int ProxyCtrl::endProxy(proxy_info* pPInfo){
	markProxy(pPInfo,EXITING);
	std::cout << "[endProxy] ending [" << pPInfo->dev_id << ", " << pPInfo->cap << "," << pPInfo->dedup_num << "]"<< std::endl;
	pPInfo->isBooted=false;
	return EXIT_SUCCESS;
}

int ProxyCtrl::waitProxy(proxy_info* pPInfo, proxy_state state){
	proxy_state curr_state;
	_shmanager->getProxyState(curr_state,pPInfo->dev_id,pPInfo->partition_num);
	while(curr_state != state){
		usleep(WAIT_MILLI*1000);
		_shmanager->getProxyState(curr_state,pPInfo->dev_id,pPInfo->partition_num);
	}
	return EXIT_SUCCESS;
}

int ProxyCtrl::connectProxy(proxy_info* pPInfo){
	if(pPInfo->isConnected){ // if already connected then just return
		return EXIT_SUCCESS;
	}
	int GPUProxy_in=connectGPUProxyIn(pPInfo->dev_id, pPInfo->cap,pPInfo->dedup_num);                                                                 
	int GPUProxy_out=connectGPUProxyOut(pPInfo->dev_id,pPInfo->cap,pPInfo->dedup_num);
#ifdef DEBUG
	printf("Device %d. inFD: %d, outFD; %d\n",pPInfo->dev_id,GPUProxy_in,GPUProxy_out);
#endif 
	pPInfo->in_fd=GPUProxy_in;
	pPInfo->out_fd=GPUProxy_out;
	pPInfo->isConnected=true;
	return EXIT_SUCCESS;
}

int ProxyCtrl::disconnectProxy(proxy_info* pPInfo){
	const int MSG = CLOSE_SOCKET;
	socket_send(pPInfo->in_fd,(char*)&MSG, sizeof(int), false);
	socket_close(pPInfo->in_fd, true);
	socket_close(pPInfo->out_fd,true);
	pPInfo->isConnected=false;
	return EXIT_SUCCESS;
}

int ProxyCtrl::makeProxyLoadModel(proxy_info* pPInfo, int model_id, int batch_size){
	const int MSG = LOAD_MODEL;
	markProxy(pPInfo,LOADING);
	pPInfo->sendMtx->lock();
	socket_send(pPInfo->in_fd,(char*)&MSG, sizeof(int), false);
	socket_send(pPInfo->in_fd,(char*)&model_id, sizeof(int), false);
	socket_send(pPInfo->in_fd,(char*)&batch_size, sizeof(int), false);
	pPInfo->sendMtx->unlock();
}

int ProxyCtrl::makeProxyUnloadModel(proxy_info* pPInfo, int model_id){
	const int MSG = UNLOAD_MODEL;
	markProxy(pPInfo,LOADING);
	pPInfo->sendMtx->lock();
	socket_send(pPInfo->in_fd,(char*)&MSG, sizeof(int), false);
	socket_send(pPInfo->in_fd,(char*)&model_id, sizeof(int), false);
	pPInfo->sendMtx->unlock();
}
int ProxyCtrl::markProxy(proxy_info *pPInfo, proxy_state state){
	return _shmanager->setProxyState(state,pPInfo->dev_id,pPInfo->partition_num);
}

proxy_state ProxyCtrl::getProxyState(proxy_info *pPInfo){
	proxy_state output_state;
	int ret=_shmanager->getProxyState(output_state,pPInfo->dev_id,pPInfo->partition_num);
	if(ret == EXIT_SUCCESS) return output_state;
	else{
		return COLD;     
	} 
}

int ProxyCtrl::getTotalMem(proxy_info *pPInfo){
	return mGPUUtil->GetTotalMemory(pPInfo->dev_id);
}

int ProxyCtrl::getUsedMem(proxy_info *pPInfo){
	return mGPUUtil->GetUsedMemory(pPInfo->dev_id);
}

void ProxyCtrl::setProxyConstants(std::string proxy_full_dir, std::string proxy_sh){
	_PROXY_FULL_DIR=proxy_full_dir;
	_PROXY_SH=proxy_sh;
}
