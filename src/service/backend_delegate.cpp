#include "backend_delegate.h"
#include "backend_control.h"
#include "socket.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <unistd.h>

BackendDelegate::BackendDelegate(int node_id, std::vector<int> &sizes, std::string backend_data_addr, DeviceSpec *ds)
	: _node_id(node_id), _backend_data_addr(backend_data_addr), _pDevSpec(ds)
{
	for(auto size : sizes){
		_sizes.push_back(size);
	}
}
BackendDelegate::~BackendDelegate(){}

void BackendDelegate::setEmulatingMode(bool flag){
	_emul_flag=flag;
}

int connectChannel(std::string backend_addr){
#ifdef FRONTEND_DEBUG
	std::cout << __func__ <<": received address for connection: " << backend_addr
		<< std::endl;
#endif
	std::stringstream temp_s(backend_addr);
	std::vector<std::string> results;
	// string = 'addr:portno'
	while(temp_s.good()){
		std::string substr;
		std::getline(temp_s,substr,':');
		results.push_back(substr);
	} 
	assert(results.size() == 2);
	// addr
	std::string addr =  results[0];
	//portno
	int portno = std::stoi(results[1]);
	//const char *c = _addr.c_str();
	int socketfd = client_init((char *)addr.c_str(), portno, true);
	if (socketfd < 0){
		std::cout << __func__ << ": connection to " << addr << ":" << portno << " failed!"
			<< std::endl;
		return -1;
	}
	char buf;
	buf=1;

	return socketfd;
}
int BackendDelegate::connectNewDataChannel(){
	int data_socket = connectChannel(_backend_data_addr);
#ifdef FRONTEND_DEBUG
	std::cout << __func__ << ": connected new data channel with socket: " << data_socket
		<<std::endl;
#endif

	return data_socket;
}
int BackendDelegate::connectCtrlChannel(std::string backend_addr){
	_control_socket_fd =  connectChannel(backend_addr);
	if(_control_socket_fd == -1){
		return -1;
	}
#ifdef FRONTEND_DEBUG
	std::cout << __func__ << ": connected new control channel with socket: " << _control_socket_fd
		<<std::endl;
#endif
	return _control_socket_fd;
}

// disconnect from backend address
int BackendDelegate::disconnectDataChannel(int data_socket_fd){
	assert(data_socket_fd);
	socket_close(data_socket_fd, false);
}

int BackendDelegate::disconnectCtrlChannel(){
	assert(_control_socket_fd);
	socket_close(_control_socket_fd, false);
}

int BackendDelegate::getControlFD()
{
	return _control_socket_fd;
}

int BackendDelegate::getNGPUs(){
	return _nGPUs;
} 
void BackendDelegate::setNGPUs(int ngpu){  
	assert(ngpu>=0);
	_nGPUs=ngpu;
}

std::vector<int> BackendDelegate::getSizes(){
	return _sizes;
}

DeviceSpec* BackendDelegate::getDeviceSpec(){
	return _pDevSpec;
}

std::string BackendDelegate::getType(){
	return _pDevSpec->getType();
}

int BackendDelegate::updateNGPUS(){
	const int PARAM_NUM=3;
	int dummy_arg1 = 0;
	int dummy_arg2 = 0;
	socket_txsize(_control_socket_fd,PARAM_NUM);
	socket_txsize(_control_socket_fd,BACKEND_GET_NGPUS);
	socket_txsize(_control_socket_fd,dummy_arg1);
	socket_txsize(_control_socket_fd, dummy_arg2);
	int ngpu=socket_rxsize(_control_socket_fd);
	_nGPUs= ngpu;
	return EXIT_SUCCESS;
}
