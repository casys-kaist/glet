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

#else
#endif
