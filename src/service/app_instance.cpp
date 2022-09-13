#include <string>
#include <stdio.h>
#include <deque> 
#include <map>
#include <mutex>
#include "common_utils.h"
#include "app_instance.h"

AppInstance::AppInstance(std::string name, AppSpec* pAppSpec){
	_name=name;
	_pAppSpec=pAppSpec;
	setupScoreBoard();
	_reported=false;
	_dropped=false;
} 

AppInstance::~AppInstance(){
}   

std::string AppInstance::getName(){ 
	return _name;
}   
void AppInstance::setStart(uint64_t time){
	start=time;
}   
void AppInstance::setStartExec(uint64_t time){
	startExec=time;
}   
void AppInstance::setEndExec(uint64_t time){
	endExec=time;
}   
void AppInstance::setTaskID(int id){
	_taskID = id; 
}   
int AppInstance::getTaskID(){
	return _taskID;
}   
void AppInstance::setSocketFD(int fd){
	_fd = fd; 
}   
int AppInstance::getSocketFD(){
	return _fd;
}   
void AppInstance::setupScoreBoard(){
	unsigned int len= _pAppSpec->getTotalNumofNodes();
	for(int i =0; i <len; i++){
		scoreBoard[i]=false;
	}

	outputCheck = _pAppSpec->getInputforOutput();

}
void AppInstance::writeToLog(FILE* fp){
	double queuetime=double(startExec-start);
	double exectime=double(endExec-startExec);
	queuetime = queuetime / 1000000;
	exectime = exectime / 1000000;
	if(_dropped){
		fprintf(fp,"%s,%s,%d,%lf,%lf\n",timeStamp(),_name.c_str(),-1, queuetime,exectime);
	}
	else{
		fprintf(fp,"%s,%s,%d,%lf,%lf\n",timeStamp(),_name.c_str(),_taskID, queuetime,exectime);
	}
	fflush(fp);
}

void AppInstance::makeInputTensor(torch::Tensor tensor, int input_id){
	inputMtx.lock();
	_input[input_id]=tensor;
	inputMtx.unlock();
}
unsigned int AppInstance::getSizeofInputs(){
	return _input.size();
}
void AppInstance::markFinished(int id){
	finishMtx.lock();
	scoreBoard[id]=true;
#ifdef DEBUG
	printf("[AppInstance] taskid: %d, cgid: %d, markedas: %d \n", _taskID, id, scoreBoard[id]);   
#endif
	finishMtx.unlock();

}
bool AppInstance::isFinished(){
	finishMtx.lock();
	if(_reported){ // if app is reported than just return false fast
		finishMtx.unlock();
		return false;
	}
	bool finished=true;
	for(std::vector<int>::iterator it = outputCheck.begin(); it != outputCheck.end(); it++){
#ifdef DEBUG
		printf("[AppInstance] taskid: %d, cgid: %d, finished?: %d \n", _taskID,*it,scoreBoard[*it]);   
#endif
		finished &=scoreBoard[*it];
	}
	if(finished){
		if(_reported) finished =false; // this is used to prevent race conditions. where multiple threads can check and report
		else
			_reported = true;
	}
	finishMtx.unlock();

	return finished;
}
int AppInstance::getAppSpecID(){ //returns globla ID for AppSpece
	return _pAppSpec->getGlobalVecID();
}
torch::Tensor AppInstance::getOutputTensor(int id){
	return _input[id];
}
AppSpec* AppInstance::getAppSpec(){
	return _pAppSpec;
}
bool AppInstance::isDropped(){
	return _dropped;

}
void AppInstance::setDropped(bool dropped){
	_dropped=dropped;
}
