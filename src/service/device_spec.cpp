#include "device_spec.h"

DeviceSpec::DeviceSpec(std::string name, int total_mem, std::string type)
	: _name(name), _total_mem(total_mem), _type(type)
{}
DeviceSpec::~DeviceSpec(){}

int DeviceSpec::getTotalMem(){
	return _total_mem;
}

std::string DeviceSpec::getName(){
	return _name;
}

std::string DeviceSpec::getType(){
	return _type;
}


