#include "device_perf_model.h"
#include <iostream>
#include "json/json.h"
#include "json/json-forwards.h"

DevPerfModel::DevPerfModel(){
}

DevPerfModel::~DevPerfModel(){
}

int DevPerfModel::setup(std::string latency_info_file, std::string model_const_file,std::string util_file, int dev_mem){
	_latModel.setupTable(latency_info_file);
	_intModel.setup(model_const_file,util_file);    
	_devMem=dev_mem;
	return EXIT_SUCCESS;

}

float DevPerfModel::getLatency(std::string model, int batch, int part){
	return _latModel.getLatency(model, batch, part);
}

float DevPerfModel::getGPURatio(std::string model, int batch, int part){
	return _latModel.getGPURatio(model, batch, part);
}

float DevPerfModel::getInterference(std::string my_model, int my_batch, int my_partition, \
		std::string your_model, int your_batch, int your_partition){
	return _intModel.getInterference(my_model,my_batch,my_partition,your_model,your_batch,your_partition);
}

int DevPerfModel::getDevMem(){
	return  _devMem;
}



