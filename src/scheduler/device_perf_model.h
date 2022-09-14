#ifndef _DEVICE_PERF_MODEL_H__
#define _DEVICE_PERF_MODEL_H__

#include <string>
#include "latency_model.h"
#include "interference_model.h"

class DevPerfModel{
	public:
		DevPerfModel();
		~DevPerfModel();
		int setup(std::string latency_info_file, std::string model_const_file,std::string util_file, int dev_mem);
		float getLatency(std::string model, int batch, int part);
		float getGPURatio(std::string model, int batch, int part);
		float getInterference(std::string my_model, int my_batch, int my_partition, std::string your_model, int your_batch, int your_partition);
		int getDevMem();
	private:
		int _devMem;
		InterferenceModeling::InterferenceModel _intModel;
		LatencyModel _latModel;
};

#else
#endif
