#include <vector>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "batched_request.h"

batched_request::batched_request() //makes empty batched_request
{
	mBatchNum=0;
	mGPUID=-1;
}

batched_request::~batched_request(){
} 

torch::Tensor batched_request::getBatchedTensor(){
	return batched_tensor;
}
std::vector<std::shared_ptr<request>>& batched_request::getRequests(){
	return taskRequests;
}

void batched_request::setBatchID(int id){
	mBatchID = id;
}

int batched_request::getBatchID(){
	return mBatchID;
}

char* batched_request::getReqName(){
	return _req_name; 
}

void batched_request::setStrName(std::string name){
	_StrReqName = name;
}

std::string batched_request::getStrName(){
	return _StrReqName;
}

void batched_request::setReqName(const char *net_name){
	strcpy(_req_name, net_name);
}

unsigned int batched_request::getBatchNum(){
	return mBatchNum;
}


int batched_request::getGPUID(){
	return mGPUID;
}

void batched_request::setGPUID(int gid){
	mGPUID=gid;
}

void batched_request::setMaxBatch(unsigned int size){
	_max_batch=size;
}

unsigned int batched_request::getMaxBatch(){
	return _max_batch;
}

int batched_request::getNTask(){
	return taskRequests.size();
}
void batched_request::addRequestInfo(std::shared_ptr<request> &t){
	if (taskRequests.size() > _max_batch){ 
		printf("number of batch exceeded MAX %d \n", _max_batch);
		exit(1);
	}
	taskRequests.push_back(t);
	mBatchNum += t->getBatchNum();
	if(mBatchNum==1)
		batched_tensor = t->_inputTensors[0];
	else
		batched_tensor = torch::cat({batched_tensor,t->_inputTensors[0]},0);
}


void batched_request::allocateOutput(torch::Tensor tensor){
	unsigned int size=1;
#ifdef DEBUG
	printf("[batchedRequest] tensor dimension before split: ");
	for(int i =0; i<tensor.dim(); i++ )
		printf("%lu, ",tensor.size(i));
	printf("\n");
#endif 

	unsigned int i=0;
	// split tensor among 1-dim
	std::vector<torch::Tensor> splitted_tensors = tensor.split(/*split size*/1,/*dim*/0);
	for(std::vector<std::shared_ptr<request>>::iterator it = taskRequests.begin(); it != taskRequests.end(); it++ ){
#ifdef DEBUG
		torch::Tensor temp = splitted_tensors[i];

		printf("[batchedRequest] sliced tensor dimension: ");
		for(int j =0; j<temp.dim(); j++ )
			printf("%lu, ",temp.size(j));
		printf("\n");
#endif 

		(*it)->setOutputTensor(splitted_tensors[i]);
		i++;
	}
}
