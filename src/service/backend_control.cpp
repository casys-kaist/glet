#include "backend_control.h"
#include "common_utils.h"
#include "socket.h"
#include "global_scheduler.h"
#include "gpu_utils.h"

extern SysMonitor ServerState;
extern ProxyCtrl *pProxyCtrl;
extern GPUUtil *gpGPUUtil;
// receives command from backend
// All of commands are CALLED FROM BACKEND

BackendControl::BackendControl(int socket_fd) : 
	_socket_fd(socket_fd)
{
	std::cout<< "BackendContorl Initated with socket: " << _socket_fd
		<<std::endl;
}

BackendControl::~BackendControl(){};

int BackendControl::receiveControl(std::vector<int> &recv_data){
	int buf=0;
	recv_data.clear();
	// read messages and execute controls
	int rdsize = socket_rxsize(_socket_fd);
#ifdef BACKEND_DEBUG

	std::cout << "rdsize: " << rdsize << std::endl;

#endif
	if(rdsize == 0){
		printf("0 bytes read! check if socket is closed! \n");
		return EXIT_FAILURE;
	}
	if(rdsize < 0){  
		perror("Read Error:");
	}  
	assert(rdsize >= 3);
	// get action 
	buf=socket_rxsize(_socket_fd);
	recv_data.push_back(buf); 
	// get GPU ID, and convert it into host idx 
	buf=socket_rxsize(_socket_fd);
	buf = ServerState.getLocalIDToHostIDTable()->at(buf);
	recv_data.push_back(buf); 
	buf=socket_rxsize(_socket_fd);
	// get proxy index
	recv_data.push_back(buf); 
	// get optional parameters
	for(int i =3; i < rdsize;i++){
		buf=socket_rxsize(_socket_fd);
		recv_data.push_back(buf); 
	}

	return EXIT_SUCCESS;
}

// executes desired action, based on received code
int BackendControl::executeControl(std::vector<int> &params){
	assert(params.size() >=3 );
	auto cmd = static_cast<BACKEND_CONTROL_CMD>(params[0]);
	int dev_id = params[1];
	int part_num = params[2];
	// find the appropriate proxy for executing command
	proxy_info *pPInfo = ServerState.findProxy(dev_id, part_num);
	int original_idx=-1 ;
	if (cmd == BACKEND_GET_MEM){
		for(auto pair : *ServerState.getLocalIDToHostIDTable()){
			if(pair.second == dev_id){
				original_idx=pair.first;
			}
		}
		assert(original_idx != -1);
	}
	int ret=1;
	int state=3;
#ifdef BACKEND_DEBUG
	std::cout << __func__ << ": received command: " << cmd << " for dev_id: " << dev_id << " part_num: " << part_num
		<<std::endl;
#endif
	switch(cmd){
		case BACKEND_GET_STATE:
			//SYNC CALL (but just checking currnet state of pPInfo)
			ret=getState(pPInfo, state);
			socket_txsize(_socket_fd,state);
			break;	
		case BACKEND_SET_STATE:
			ret=setState(pPInfo, state);
			socket_txsize(_socket_fd,state);
			break;	
		case BACKEND_CONNECT_PROXY:
			ret=connectProxy(pPInfo);
			socket_txsize(_socket_fd,state);
			break;
		case BACKEND_DISCONNECT_PROXY:
			ret=disconnectProxy(pPInfo);
			socket_txsize(_socket_fd,state);
			break;
		case BACKEND_LOAD_MODEL:
			ret=loadModelBackend(pPInfo,params[3], params[4]);
			socket_txsize(_socket_fd,ret);
			break;
		case BACKEND_UNLOAD_MODEL:
			ret=unloadModelBackend(pPInfo,params[3]);
			socket_txsize(_socket_fd,ret);
			break;
		case BACKEND_START_PROXY:
			ret=startProxy(pPInfo);
			socket_txsize(_socket_fd,ret);
			break;
		case BACKEND_END_PROXY:
			ret=endProxy(pPInfo);
			socket_txsize(_socket_fd,ret);
			break;
		case BACKEND_GET_MEM:
			ret=gpGPUUtil->GetUsedMemory(original_idx);
			socket_txsize(_socket_fd,ret);
			if(ret) ret=EXIT_SUCCESS;
			else ret=EXIT_FAILURE;
			break;
		case BACKEND_GET_NGPUS:
			socket_txsize(_socket_fd,getNGPUs());
			// this is just returning a global variable, it will succeed anytime
			ret=EXIT_SUCCESS;
			break;
		default: // not supposed to happen
			break;
	};
	return ret;
}

// load model to proxy
int BackendControl::loadModelBackend(proxy_info *pPInfo, int model_id, int batch_size){
	pProxyCtrl->makeProxyLoadModel(pPInfo,model_id,batch_size);
	return EXIT_SUCCESS;
}

// unload model from proxy
int BackendControl::unloadModelBackend(proxy_info *pPInfo, int model_id){
	pProxyCtrl->makeProxyUnloadModel(pPInfo,model_id);
	return EXIT_SUCCESS;

}
int BackendControl::getState(proxy_info *pPInfo, int &state){
	state= pProxyCtrl->getProxyState(pPInfo);
	return EXIT_SUCCESS;
}

int BackendControl::setState(proxy_info *pPInfo, int state){
	pProxyCtrl->markProxy(pPInfo,static_cast<proxy_state>(state));
}

int BackendControl::startProxy(proxy_info *pPInfo){
	pProxyCtrl->startProxy(pPInfo,ServerState.getNGPUs(),ServerState.getNumProxyPerGPU());

	return EXIT_SUCCESS;
}
int BackendControl::endProxy(proxy_info *pPInfo){
	pProxyCtrl->endProxy(pPInfo);
	return EXIT_SUCCESS;
}

int BackendControl::connectProxy(proxy_info *pPInfo){
	pProxyCtrl->connectProxy(pPInfo);
	return EXIT_SUCCESS;
}

int BackendControl::disconnectProxy(proxy_info *pPInfo){
	pProxyCtrl->disconnectProxy(pPInfo);
	return EXIT_SUCCESS;
}

int BackendControl::getUsedMemory(int gpuid){
	return gpGPUUtil->GetUsedMemory(gpuid);
}

int BackendControl::getNGPUs(){
	return ServerState.getNGPUs();
}


