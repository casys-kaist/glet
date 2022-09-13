#ifndef _APP_SPEC_H__
#define _APP_SPEC_H__
#include <vector>
#include <string>
#include <map>
#include <torch/script.h>
#include <torch/serialize/tensor.h> 
#include <torch/serialize.h>

enum TensorDataOption {KFLOAT32=0,KINT64=1};

typedef struct _CompDep{
	std::string type;
	std::string name;
	int id; 
	int SLO; // node-wise SLO
	int predecessor; // used for finding critical path
	std::vector<int> input; 
	std::vector<int> output;
} CompDep;

typedef struct _TensorSpec{
	std::vector<int> dim;
	std::vector<int> output;
	int id;
	TensorDataOption dataType;
} TensorSpec;

class AppSpec{
	public:
		AppSpec(std::string name);
		~AppSpec();
		std::string getName();
		float getNodeSLO(int id);
		void setGlobalVecID(int id);
		int getGlobalVecID();
		int setupModelDependency(const char* AppJSON); //recieves filename as input
		std::string getOutputComp();
		std::vector<CompDep> getModelTable();
		std::vector<TensorSpec> getInputTensorSpecs();
		void addInputSpec(TensorSpec &Tensor);
		void setOutputComp(std::string outputComp);       
		torch::Tensor aggregateOutput(std::string &CompType, std::vector<torch::Tensor> &Inputs);
		void calcCriticalPath(); // compute CP and allocates SLO budget
		int sendOutputtoClient(int socketFD, int  taskID);
		std::vector<int> getNextDsts(int curr_id);
		std::string getModelName(int id);
		bool isOutput(int id);
		std::vector<int> getInputforOutput();
		unsigned int getTotalNumofNodes();
		void printSpecs();

#ifdef DEBUG
		void printCritPath();
#endif
	private:
		std::string _name;
		std::string _outputComp;
		std::vector<TensorSpec> inputTensors;
		std::vector<CompDep> _ModelTable;
		std::vector<CompDep> _CriticalPath;
		std::map<int,double> _RequestIntervals;
		std::map<int,int> _perNodeSLO; // per node SLO, should be allocated after call to 'calcCriticalPath'
		std::map<int, float> _perNodeLatency; // per node latency, reads profiled data before hand and set with 'setNodeLatency'
		double _SyncGran;
		int _globalVecId; // used for indexing which AppSpec, in the AppSpecVec global vector
};
#endif 
