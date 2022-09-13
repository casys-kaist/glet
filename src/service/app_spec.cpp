#include <iostream>
#include <fstream>
#include <algorithm>
#include <torch/script.h>
#include <torch/serialize/tensor.h> 
#include <torch/serialize.h>
#include "app_spec.h"
#include "json/json.h"
#include "custom_ops.h"
#include "socket.h"

AppSpec::AppSpec(std::string name){
	_name=name;
}

AppSpec::~AppSpec(){

}

int AppSpec::setupModelDependency(const char* AppJSON){
	Json::Value root;
	std::ifstream ifs;
	ifs.open(AppJSON);
	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;
	if (!parseFromStream(builder, ifs, &root, &errs)) {
		std::cout << errs << std::endl;
		return EXIT_FAILURE;
	}
	std::vector<int> toOutputIndex;
	CompDep *ptMD;

	//setup Root input(s)
	for(std::vector<TensorSpec>::iterator it = inputTensors.begin(); it != inputTensors.end(); it++){
		ptMD = new CompDep();
		ptMD -> type="Input";
		ptMD -> name="Input";
		ptMD -> id = it->id;
		ptMD -> output = it -> output;
		_ModelTable.push_back(*ptMD);

	}
	//set rest of dependency graph
	const int DEFAULT_MAX_BATCH=32;
	for(unsigned int i=0; i < root["Models"].size();i++){
		ptMD = new CompDep();
		ptMD->type="Model";
		ptMD->name=root["Models"][i]["model"].asString();
		ptMD->id=root["Models"][i]["ID"].asInt();
		// ids used as input
		for(unsigned int j=0; j < root["Models"][i]["input"].size(); j++){
			ptMD->input.push_back(root["Models"][i]["input"][j].asInt());
		}

		for(unsigned int j=0; j < root["Models"][i]["output"].size(); j++){
			ptMD->output.push_back(root["Models"][i]["output"][j].asInt());
		}
		_ModelTable.push_back(*ptMD);
	}
	//setup Root output(s)
	for(unsigned int i=0;  i< root["Output"].size();  i++){
		ptMD = new CompDep();
		ptMD->type="Output";
		ptMD->name=root["Output"][i]["Comp"].asString();
		ptMD->id=root["Output"][i]["ID"].asInt();
		for(unsigned int j=0; j < root["Output"][i]["input"].size(); j++){
			ptMD->input.push_back(root["Output"][i]["input"][j].asInt());
		}
		_ModelTable.push_back(*ptMD);

	}
	ifs.close();
	return EXIT_SUCCESS;
}

std::vector<CompDep> AppSpec::getModelTable(){
	return _ModelTable;
}

std::string AppSpec::getName(){
	return _name;
}

std::vector<TensorSpec> AppSpec::getInputTensorSpecs(){
	return inputTensors;
}
std::string AppSpec::getOutputComp(){
	return _outputComp;
}
void AppSpec::addInputSpec(TensorSpec &Tensor){
	inputTensors.push_back(Tensor);
}

void AppSpec::setOutputComp(std::string outputComp){
	_outputComp = outputComp;
}

torch::Tensor AppSpec::aggregateOutput(std::string &CompType, std::vector<torch::Tensor> &Inputs){
	torch::Tensor output;
	if(CompType == "Average" || CompType == "Avg"){
		for(uint64_t i =0; i < Inputs.size(); i++ ){
			output=output +Inputs[i];
		}
		int size = (int)Inputs.size();
		output.div(size);
	}
	else if (CompType == "Sum" || CompType == "Add"){
		for(uint64_t i =0; i < Inputs.size(); i++ ){
			output=output +Inputs[i];
		}
	}
	else if(CompType == "None"){
		output=Inputs[0];
	}
	else{
		printf("Uknown output operation: %s \n", CompType.c_str());
	}
	return output;
}

float AppSpec::getNodeSLO(int id){
	return _perNodeLatency[id];
}

void AppSpec::printSpecs(){
	printf("printing Specs of name: %s \n",_name.c_str());
	for(unsigned int i=0; i<_ModelTable.size();i++){
		printf("%dth model: %s, id: %d\n",i+1,_ModelTable[i].name.c_str(), _ModelTable[i].id);
		printf("input ids: ");
		for(unsigned int j=0; j<_ModelTable[i].input.size();j++){
			printf("%d,",_ModelTable[i].input[j]);
		}
		printf("\n");

		printf("output ids: ");
		for(unsigned int j=0; j<_ModelTable[i].output.size();j++){
			printf("%d,",_ModelTable[i].output[j]);
		}
		printf("\n");
	}
	printf("request intervals: ");
	for( std::map<int,double>::iterator it = _RequestIntervals.begin(); it != _RequestIntervals.end(); it++ ){
		printf("%lf,", it->second);
	}
	printf("\n");

}


int AppSpec::sendOutputtoClient(int socketFD, int  taskID){

	socket_txsize(socketFD,taskID);
	return EXIT_SUCCESS;
}

std::vector<int> AppSpec::getNextDsts(int curr_id){
	std::vector<int> dsts =  _ModelTable[curr_id].output;
	return dsts;
}

std::string AppSpec::getModelName(int id){
	return _ModelTable[id].name;
}

bool AppSpec::isOutput(int id){
	if(_ModelTable[id].type == "Output"){
		return true;
	}
	return false;
}
std::vector<int> AppSpec::getInputforOutput(){
	std::vector<int> ret_vector;
	for(std::vector<CompDep>::iterator it = _ModelTable.begin(); it != _ModelTable.end(); it++){
		if(it->type=="Output"){
			for(std::vector<int>::iterator it2 = it->input.begin(); it2 != it->input.end(); it2++){
				ret_vector.push_back(*it2);
			}
		}
	}
	return ret_vector;

}
unsigned int AppSpec::getTotalNumofNodes(){
	return _ModelTable.size();
}

int AppSpec::getGlobalVecID(){
	return _globalVecId;
}

void AppSpec::setGlobalVecID(int id){
	_globalVecId = id;
}

