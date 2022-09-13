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
}
int BackendDelegate::connectNewDataChannel(){
}
int BackendDelegate::connectCtrlChannel(std::string backend_addr){
}

// disconnect from backend address
int BackendDelegate::disconnectDataChannel(int data_socket_fd){
}

int BackendDelegate::disconnectCtrlChannel(){
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
