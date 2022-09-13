#ifndef _LAT_MODEL_H__
#define _LAT_MODEL_H__

#include <string> 
#include <vector>
#include <map>
#include <unordered_map>

typedef struct _Entry
{
  int batch;
  int part;
  float latency;
  float gpu_ratio;
} Entry;

class LatencyModel {
	public:
	void setupTable(std::string TableFile);
	float getLatency(std::string model, int batch, int part);
        float getGPURatio(std::string model, int batch, int part);
        int makeKey(int batch, int part);
        Entry* parseKey(int key);
	private:
        float getBatchPartInterpolatedLatency(std::string model, int batch, int part);
        float getBatchInterpolatedLatency(std::string model, int batch, int part);
	std::map<std::string, std::unordered_map<int,float>*> _perModelLatnecyTable;
	std::map<std::string, std::unordered_map<int,float>*> _perModelGPURatioTable;
	std::map<std::string, std::map<int,std::vector<int>>> _perModelBatchVec;
};

#else
#endif
