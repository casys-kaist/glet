#include "backend_proxy_ctrl.h"
#include "backend_control.h"
#include "socket.h"
#include "gpu_proxy.h"
#include <unistd.h>
#include <map>

extern std::map<int,BackendDelegate*> NodeIDtoBackendDelegate;

BackendProxyCtrl::BackendProxyCtrl(){}

BackendProxyCtrl::~BackendProxyCtrl(){}

int BackendProxyCtrl::addBackendDelegate(int node_id, BackendDelegate *backend_delegate){
	if ( mMapBackendDelegate.find(node_id) == mMapBackendDelegate.end() ) {
		mMapBackendDelegate[node_id]=backend_delegate;
		mPerBackendNodeDevNum[node_id] = backend_delegate->getNGPUs();
		return EXIT_SUCCESS;
	} else {
		std::cout << "ERROR: node_id " << node_id << " is already found!!" << std::endl;
		return EXIT_FAILURE;
	}    
}

void BackendProxyCtrl::setEmulatedMode(bool flag){
	_emul_flag=flag;
}

// starts proxy, called from server
int BackendProxyCtrl::startProxy(proxy_info *pPInfo, int ngpus, int nparts){
	const int PARAM_NUM=3;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
#ifdef FRONTEND_DEBUG
	std::cout << __func__ << ": starting booting proxy on " << pPInfo->node_id
		<< ": dev_id:  " << dev_id << std::endl; 
#endif
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_START_PROXY);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);
	int ret=socket_rxsize(control_socketfd);
	return EXIT_SUCCESS;
}

// ends proxy, called from server
int BackendProxyCtrl::endProxy(proxy_info *pPInfo){
	const int PARAM_NUM=3;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_END_PROXY);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);

	int ret=socket_rxsize(control_socketfd);
	pPInfo->isBooted=false;
	return EXIT_SUCCESS;
}

// wait until the proxy becomes state(USE IT AT YOUR OWN RISK)
int BackendProxyCtrl::waitProxy(proxy_info *pPInfo, proxy_state state){
	const int WAIT_MILLI=500;
	proxy_state curr_state;
	curr_state = getProxyState(pPInfo);
	while(curr_state != state){
		usleep(WAIT_MILLI*1000);
		curr_state = getProxyState(pPInfo);
	}
	return EXIT_SUCCESS;
}

// connects proxy_info to corresponding proxy
int BackendProxyCtrl::connectProxy(proxy_info *pPInfo){
	if(pPInfo->isConnected){ // if already connected then just return
		return EXIT_SUCCESS;
	}
	const int PARAM_NUM=3;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_CONNECT_PROXY);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);
	int ret=socket_rxsize(control_socketfd);

	//connect and register fd
	if(!pPInfo->isConnectedtoBackend){
		const int OUT_FD_MSG=1;
		const int IN_FD_MSG=0;
		pPInfo->in_fd=mMapBackendDelegate[pPInfo->node_id]->connectNewDataChannel();
		socket_txsize(pPInfo->in_fd,dev_id);
		socket_txsize(pPInfo->in_fd,pPInfo->partition_num);
		socket_txsize(pPInfo->in_fd,IN_FD_MSG);
		// wait for ACK
		ret = socket_rxsize(pPInfo->in_fd);
		// connect and register fd 
		pPInfo->out_fd=mMapBackendDelegate[pPInfo->node_id]->connectNewDataChannel();
		socket_txsize(pPInfo->out_fd,dev_id);
		socket_txsize(pPInfo->out_fd,pPInfo->partition_num);
		socket_txsize(pPInfo->out_fd,OUT_FD_MSG);
		pPInfo->isConnectedtoBackend=true;
	}
	pPInfo->isConnected=true;
#ifdef FRONTEND_DEBUG
	printf("Device %d, part_num:%d, inFD: %d, outFD; %d\n",pPInfo->dev_id,pPInfo->partition_num,pPInfo->in_fd,pPInfo->out_fd);
#endif 
	return EXIT_SUCCESS;
}

// disconnects proxy_info from corresponding proxy
int BackendProxyCtrl::disconnectProxy(proxy_info *pPInfo){
	const int PARAM_NUM=3;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_DISCONNECT_PROXY);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);
	int ret=socket_rxsize(control_socketfd);
	lk.unlock();
	pPInfo->isConnected=false;
#ifdef FRONTEND_DEBUG
	printf("Disconnected proxy on device %d, part_num:%d \n",\
			pPInfo->dev_id,pPInfo->partition_num);
#endif 
	return EXIT_SUCCESS;
}

// loads model with 'id'
int BackendProxyCtrl::makeProxyLoadModel(proxy_info *pPInfo, int model_id, int batch_size){
	const int PARAM_NUM=5;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_LOAD_MODEL);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);
	socket_txsize(control_socketfd,model_id);
	socket_txsize(control_socketfd,batch_size);
	int ret=socket_rxsize(control_socketfd);

	return EXIT_SUCCESS;
}

// unloads model with 'id'
int BackendProxyCtrl::makeProxyUnloadModel(proxy_info *pPInfo, int model_id){
	const int PARAM_NUM=4;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_UNLOAD_MODEL);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);
	socket_txsize(control_socketfd,model_id);
	int ret=socket_rxsize(control_socketfd);
	return EXIT_SUCCESS;
}

// marks proxy as 'state', called from proxy
int BackendProxyCtrl::markProxy(proxy_info* pPInfo, proxy_state state){
	const int PARAM_NUM=4;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_SET_STATE);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd,pPInfo->partition_num);
	socket_txsize(control_socketfd,static_cast<int>(state));
	int ret=socket_rxsize(control_socketfd);    
	return EXIT_SUCCESS;
}

proxy_state BackendProxyCtrl::getProxyState(proxy_info *pPInfo){
	const int PARAM_NUM=3;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_GET_STATE);
	socket_txsize(control_socketfd,dev_id);
	socket_txsize(control_socketfd, pPInfo->partition_num);
	int state=socket_rxsize(control_socketfd);
	return static_cast<proxy_state>(state);
}

int BackendProxyCtrl::getTotalMem(proxy_info* pPInfo){
	BackendDelegate *pbd=NodeIDtoBackendDelegate[pPInfo->node_id];    
	return pbd->getDeviceSpec()->getTotalMem();
}

int BackendProxyCtrl::getUsedMem(proxy_info *pPInfo){
	const int PARAM_NUM=3;
	int control_socketfd=mMapBackendDelegate[pPInfo->node_id]->getControlFD();
	int dev_id = lookupFrontDevIDtoBackDevID(pPInfo);
	std::unique_lock<std::mutex> lk(_mtx);
	socket_txsize(control_socketfd,PARAM_NUM);
	socket_txsize(control_socketfd,BACKEND_GET_MEM);
	socket_txsize(control_socketfd,dev_id);
	// the next really doesnt matter, just dummy number to fill 3 arguments
	socket_txsize(control_socketfd,0);
	int used_mem = socket_rxsize(control_socketfd);
	return used_mem;
}


int BackendProxyCtrl::lookupFrontDevIDtoBackDevID(proxy_info *pPInfo){
	int sum_of_dev=0;
	for(int i =0; i <pPInfo->node_id; i++){
		sum_of_dev += mPerBackendNodeDevNum[i];
	}
	return pPInfo->dev_id - sum_of_dev;
}
