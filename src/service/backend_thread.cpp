#include "thread.h"
#include "proxy_ctrl.h"
#include "common_utils.h"
#include "backend_data.h"
#include "backend_control.h"
#include "gpu_proxy.h"
#include <torch/script.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <map>
#include <queue>
#include <condition_variable>
#include <mutex>

extern ProxyCtrl *pProxyCtrl;
extern SysMonitor ServerState;
extern std::map<proxy_info*, std::queue<BatchedRequest*>*> MapProxytoOutputQueue;
extern std::map<proxy_info*, std::condition_variable*> MapProxytoOutputCV;
extern std::map<proxy_info*, std::mutex*> MapProxytoOutputMtx;

// below is an array of string used for debugging 
// each string representes each control command defined in backend_control.h
std::string CMD_TYPES[] = {
	"BACKEND_GET_STATE",
	"BACKEND_SET_STATE",
	"BACKEND_LOAD_MODEL", 
	"BACKEND_UNLOAD_MODEL",
	"BACKEND_START_PROXY",
	"BACKEND_END_PROXY",
	"BACKEND_DISCONNECT_PROXY",
	"BACKEND_CONNECT_PROXY",
	"BACKEND_GET_MEM"
};

pthread_t initBackendDataThread(int socket){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initBackendData, (void*)socket) != 0)
		LOG(ERROR) << "Failed to create a fill queue thread.\n";
	return tid;

}
// receives data and sends it to proxy
void* initBackendData(void *args){
	int socket_fd = (intptr_t)args;
	int gpuid = socket_rxsize(socket_fd);
	int partition_index = socket_rxsize(socket_fd);	
	int msg = socket_rxsize(socket_fd);
	gpuid  = ServerState.getLocalIDToHostIDTable()->operator[](gpuid);
	proxy_info* pPInfo = ServerState.findProxy(gpuid, partition_index);	
	int ack=1;
	// * messsages are defined in backend_proxy_ctrl.cpp 
	// input
	if(msg == 0){
		if(MapProxytoOutputQueue.find(pPInfo) == MapProxytoOutputQueue.end()){
			MapProxytoOutputQueue[pPInfo] = new std::queue<BatchedRequest*>();
			MapProxytoOutputMtx[pPInfo]=new std::mutex();
			MapProxytoOutputCV[pPInfo] = new std::condition_variable();
		}
		int yes=1;
		if(setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int))==-1)	{
			perror("initBackendData,setsocketopt");
		}			
		socket_txsize(socket_fd,ack);
		while(true){
			if (recvBatchedTensor(socket_fd,pPInfo)){
				printf("Error in receiving tensor, Or Client closed for input data channel of proxy [%d, %d] \n", gpuid,partition_index);
				break;
			}
		}
	}
	// output
	else if(msg==1){
		std::queue<BatchedRequest*>* output_queue = MapProxytoOutputQueue[pPInfo];
		while(true){
			std::unique_lock<std::mutex> lk(*MapProxytoOutputMtx[pPInfo]);
			MapProxytoOutputCV[pPInfo]->wait(lk,[&output_queue]{return !output_queue->empty();});
			BatchedRequest* pbr = output_queue->front();
			output_queue->pop();
			lk.unlock();
			// pbr is freed within sendBatchedOutput
			if(sendBatchedOutput(socket_fd, pPInfo, pbr)){
				printf("Error in receiving tensor, Or Client closed for output data channel of proxy [%d, %d]\n",gpuid,partition_index);
				break;
			}
		}
	}
	else{
		printf("ERROR: backend data channel received strange message: %d \n", msg);

	}
	socket_close(socket_fd,false);
}

pthread_t initBackendControlThread(int socket){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initBackendControl, (void*)socket) != 0)
		LOG(ERROR) << "Failed to create a fill queue thread.\n";
	return tid;


}
// reveives control/sche:duling result through socket
void* initBackendControl(void *args){
	int socket_fd = (intptr_t)args;
	listen(socket_fd, 20);
	LOG(ERROR) << "Backend is listening for control connections on " << socket_fd << std::endl;
	//socket option to DISABLE delay
	const int iOptval = 1;
	while (true){
		// wait for frontend connection
		int client_sock=accept(socket_fd, (sockaddr*)0, (unsigned int*)0);
		if( client_sock == -1) {
			LOG(ERROR) << "Failed to accept.\n";
			continue;
		}              
		if(setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &iOptval, sizeof(int)))
		{
			LOG(ERROR) << "set socket option failed";
			continue;       
		}
		// Set the socket I/O mode: In this case FIONBIO  
		// enables or disables the blocking mode for the   
		// socket based on the numerical value of iMode.  
		// If iMode = 0, blocking is enabled;   
		// If iMode != 0, non-blocking mode is enabled.
		int iMode=0;
		ioctl(client_sock, FIONBIO, &iMode);  
		std::vector<int> recv_data;
		int buf=0;
		LOG(ERROR) << "Initating control connection with socket: " << client_sock;
		BackendControl control(client_sock);
		while(true){
			// receive and execute control
			if(control.receiveControl(recv_data)){
				std::cout << "ERROR in receiving for backend control thread!"
					<<std::endl;
				std::cout <<"BACKEND CLOSED AT: " <<timeStamp() << std::endl;
				break;
			}
			// execute in a non-blocking fashion
			if(control.executeControl(recv_data)){
				std::cout << "ERROR in executing for backend control thread!"
					<<std::endl;
				break;
			}
		}
	}
}
