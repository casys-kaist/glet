#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <torch/script.h>
#include "app_instance.h"         
#include "request.h"
//#include "torch_utils.h"
#include "common_utils.h"

request::request(int rid, int fd, int bnum){
	mReqID = rid;
	mClientFD= fd;
	mBatchNum = bnum;
	_drop=false;
}

request::~request(){
	for(int i=0; i > _inputTensors.size(); i++){
		_inputTensors[i].reset();
	}
} 


void request::setupAppInstance(std::shared_ptr<AppInstance> task){
	_task = task;

}
void request::setOutputTensor(torch::Tensor tensor){
	_task->makeInputTensor(tensor, _cgid);
	// set finished as soon as tensor is put as input for next model, also for synch control
	setFinished(); 
}
void request::setFinished(){
	_task->markFinished(_cgid);
}
void request::setReqName(const char * net_name){
	strcpy(mReqName, net_name);
}

char* request::getReqName(){
	return mReqName; 
}

int request::getTaskID(){
	return mTaskID;
}
int request::getClientFD(){
	return mClientFD;
}
int request::getCGID(){
	return _cgid;
}
unsigned int request::getBatchNum(){
	return mBatchNum;
}

int request::setDeviceID(int id){
	mDeviceID = id;
	return 0;
}

void request::setTaskID(int id){
	mTaskID=id;
}

void request::setCGID(int id){ 
	_cgid=id;
}

int request::getBatchID(){
	return mBatchID;
}

void request::setBatchID(int id){
	mBatchID = id;
}


void request::pushInputData(torch::Tensor &input){
	_inputTensors.push_back(input);
}

bool request::isAppFinished(){
	return _task->isFinished();
}

std::shared_ptr<AppInstance> request::getApp(){
	return _task;
}

void request::setStart(uint64_t time){
	start = time;
}
void request::setendBatch(uint64_t time){
	endBatch = time;
}
void request::setstartExec(uint64_t time){
	startExec = time;
}
void request::setendExec(uint64_t time){
	endExec = time;
}
void request::setendSend(uint64_t time){
	endSend = time;
}

void request::setendReq(uint64_t time){
	endReq = time;
}

void request::setendCmpq(uint64_t time){
	endCmpq = time;
}

void request::setstartBatch(uint64_t time){
	startBatch=time;

}
uint64_t request::getStart(){ // used in age-based scheduler
	return start;
}

void request::setDropped(bool drop){
	_drop = drop;
}

bool request::isDropped(){
	return _drop;
}


double request::getLatency(){ // returns latency 
	double reqTime = double(endReq - start);
	double execTime = double(endExec - startExec);
	double batchTime = double(endBatch-endReq);
	double prepTime = double(startExec - endBatch);
	return (reqTime + execTime + batchTime + prepTime)/1000000;
}

#ifdef DEBUG
void request::printTimeStamps(){
	printf("Task ID : %d, start: %lu, endBatch: %lu, startExec: %lu, endExec: %lu, endSend: %lu \n", mTaskID, start, endBatch, startExec, endExec, endSend);
}
#endif
void request::writeToLog(FILE* fp){
	if(_drop){
		mTaskID=-1;
		mReqID=-1;
	}
	double reqTime = double(endReq - start); //queueing delay
	double batchTime = double(endBatch - endReq); //  batching overhead
	double prepTime = double( startExec- endBatch); // waiting for execution
	double execTime = double(endExec - startExec); // execution latency
	double cmpTime = double(endCmpq - endExec);
	double sendTime = double(endSend - endCmpq);
	reqTime = reqTime / 1000000;
	prepTime = prepTime / 1000000;
	execTime = execTime / 1000000;
	batchTime = batchTime / 1000000;
	cmpTime = cmpTime / 1000000;
	sendTime = sendTime / 1000000;
	fprintf(fp,"%s,%s,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf\n",timeStamp(),mReqName,mBatchID,mReqID,mDeviceID,reqTime,prepTime,batchTime,execTime,cmpTime,sendTime);
	fflush(fp);
	return;
}

