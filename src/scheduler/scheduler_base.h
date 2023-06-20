#ifndef _SCHEDULER_BASE_H__
#define _SCHEDULER_BASE_H__

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "device_perf_model.h"



typedef struct _Task
{
  int id; 
  int request_rate; // mutable rate, will be chnged during scheduling
  int ORG_RATE; // the original rate, 
  int additional_rate; // additional rate due to interference, only used during scheduling
  int SLO;
  int batch_size;
  float throughput;
} Task;

typedef std::shared_ptr<Task> TaskPtr;

typedef struct _MemNode
{
	int part;
	int dedup_num; //dedup_num added for discerning between 50:50 partitions
} MemNode;

typedef struct _Node
{
  std::vector<TaskPtr> vTaskList;
  float duty_cycle;
  float occupancy;
  int resource_pntg;
  int id; // do not confuse with task ID, this is GPU node id;
  bool reserved; // flag indicating whether is reserved or not
  int dedup_num; // this is to distinguish between same partitions e.g) 50-50 nodes
  std::string type; // the type of GPU that this node is on, empty if not allocated to a GPU, same as TYPE
} Node;

typedef std::shared_ptr<Node> NodePtr;

typedef struct _GPU
{
    int GPUID;
  	int TOTAL_MEMORY;
  	int used_memory;
	std::string TYPE;
	std::vector<MemNode> vLoadedParts;
   	std::vector<NodePtr> vNodeList;
} GPU;

typedef std::shared_ptr<GPU> GPUPtr;

typedef struct _SimState
{
  std::vector<GPUPtr> vGPUList;
  std::vector<int> parts;
  int next_dummy_id;
} SimState;

typedef struct _SatTrpEntry
{
	int part;
	int max_batch;
	float sat_trp;
	std::string type;
} SatTrpEntry;

namespace Scheduling{

class BaseScheduler {

	public:
		BaseScheduler();
		~BaseScheduler();
		bool initializeScheduler(std::string sim_config_json_file, \
								std::string mem_config_json_file,std::string device_config,std::string res_dir, \
								std::vector<std::string> &batch_filenames);
		void setupTasks(std::string task_csv_file, std::vector<Task> *task_list);
		void setupAvailParts(std::vector<int> input_parts);
		void initDevMems(SimState &input);
		void updateDevMemUsage(std::vector<int> &used_mem_per_dev, SimState &input);
		void initiateDevs(SimState &input, int nDevs);
//		bool SquishyBinPacking(std::vector<Task> *task_list, SimState &output);
		void resetScheduler(SimState &input,std::vector<Node> node_vec);
		bool getUseParts();
		std::vector<int> getAvailParts();
		int getMaxGPUs();
		int setMaxGPUs(int num_gpu);
		int getMaxBatchSize();
		int getModelMemUSsage(int model_id);
		std::string getModelName(int id);
		int getModelID(std::string model_name);
		NodePtr makeEmptyNode(int gpu_id, int resource_pntg, std::string type);
		float getLatency(std::string device, int model_num, int batch, int part); // used for non-interference version of getting latency
		float getLatency(std::string device, int model_num, int batch, NodePtr self_node, SimState &input); // used for getting latency also considering interference	
	protected:
		bool postAdjustInterference(SimState &input);
		bool adjustResNode(NodePtr &node_ptr, SimState &simulator);
		bool adjustSatNode(NodePtr &node_ptr, SimState &simulator);
		bool adjustSatNode(NodePtr &node_ptr, SimState &simulator, Task &task_to_update);

		int setupScheduler(std::string sim_config_json_file);
		int setupDevicePerfModel(std::string devcice_config_json_file, std::string res_dir);
		int setupPerModelMemConfig(std::string mem_config_json_file);
		void setupBatchLatencyTables(std::string res_dir, std::vector<std::string> &filenames);
		void initDevMem(GPUPtr gpu_ptr, int mem);
		bool getOtherNode(NodePtr input_the_node, NodePtr &output_the_other_node, SimState &sim);
		bool isSameNode(NodePtr a, NodePtr b);
		bool isPartLoaded(GPUPtr gpu_ptr, NodePtr node_ptr);
		bool isModelLoaded(GPUPtr gpu_ptr, NodePtr node_ptr, int model_id);
		void fillReservedNodes(SimState &input);
		bool getNextEmptyNode(NodePtr &new_node, SimState &input);
		float getInterference(std::string device, int a_id, int b_id, int a_batch, int b_batch, int partition_a, int partitoin_b);
		float getInterference(std::string device, int model_id, int batch_size, NodePtr node_ptr, GPUPtr gpu_ptr);
		float getBatchLatency(std::string modelname, int batch);
		int getMaxBatch(Task &self_task, const NodePtr &self_node, SimState &input, int &req, bool is_residue, bool interference);
		// return 99%ile of Poisson CDF for given rate
		int return99P(const int mean);
		double calcPoissProb(int actual, int mean);
		void printNodeInfo(const NodePtr &node);

		bool _useBatchingOverhead = 1;
		int _numMaxGPU;
		const int _MIN_BATCH = 1;
		const int _MAX_BATCH = 32;
		int _numMaxModel;
		bool _usePart;
		float _latencyRatio;
		bool  _useInterference;
		bool  _useIncremental;
		bool  _useSelfTuning=false;
		// flag indicating whether to allow repartitioning, true by default
		bool _useRepartition = true;
		std::vector<int> _availParts = {100};

		// TODO: let a function  and recieve inputs as parameters/file
		// for now we hard code
		std::map<int, float> _batch_latency_1_28_28;
		std::map<int, float> _batch_latency_3_224_224;
		std::map<int, float> _batch_latency_3_300_300;
		std::map<int, int> _mapModelIDtoMemSize;
		std::map<std::string, DevPerfModel> _nametoDevPerfModelTable;
		std::map<std::string, int> _typeToNumofTypeTable;


		std::vector<std::string> _IDtoModelName = {"lenet1", "lenet2", "lenet3",
											"lenet4", "lenet5", "lenet6", "googlenet",
											"resnet50", "ssd-mobilenetv1", "vgg16",
											"mnasnet1_0", "mobilenet_v2", "densenet161", "bert"};
		
	};
} //Scheduling


#else
#endif
