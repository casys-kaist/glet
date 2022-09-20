#ifndef _SCHEDULER_INCREMENTAL_H__
#define _SCHEDULER_INCREMENTAL_H__

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "scheduler_base.h"
#include "network_limit.h"

namespace Scheduling{
	class IncrementalScheduler : public  BaseScheduler{
		public:
			IncrementalScheduler();
			~IncrementalScheduler();
			bool runScheduling(std::vector<Task> *task_list, SimState &prev_output, SimState &new_output, bool allow_repart=false);
			void initSaturateTrp(Task &task);
			void setupNetworkChecker(std::string json_file);
			bool inspectNetworkBW(SimState &input);
			// also used outside of scheduler
			bool doesFitMemLimit(GPUPtr &gpu_ptr, int model_id, NodePtr &node_ptr);
			bool addGPUMemUsage(GPUPtr &gpu_ptr, int model_id, NodePtr &node_ptr);
		protected:
			bool incrementalScehduling(std::vector<Task> &session, SimState &decision);
			bool elasticPartitioning(std::vector<Task> &session, SimState &decision);
			bool addModeltoSchedule(Task &task, SimState &decision); 
			void residueTightening(const Task &task, SimState &decision);
			bool checkFit(std::vector<NodePtr> &candidate_nodes, SimState &decision);
			// gets list of nodes for allocating task
			void getEstimate(Task &task, std::vector<NodePtr> &output_vec, const int MAX_PART);
			bool allocateFit(const std::vector<NodePtr> &estimate, Task &task, SimState &decision);
			bool readjust(Task &task, std::vector<NodePtr> &given, SimState &decision); 
			bool mergeResidue(Task &task, SimState &input_sim); 
			bool allocateTimeShare(Task &task, SimState &sim, std::vector<NodePtr> &target_nodes);
			int getMinPart(std::string device, Task task, const NodePtr node_ptr, int &residue_rate, int &result_batch); 
			void estimateTrp(std::string device, Task &task, int rate, std::vector<NodePtr> &output_vec, const int MAX_PART);
			float getMaxSaturateTrp(const Task &task, int &output_batch, const int resource_pntg, std::string type);
			bool findBestFit(SimState &input_sim,NodePtr &input,std::vector<NodePtr> &exclude_vec ,NodePtr &output);
			bool checkForInterference(std::string device, NodePtr the_node, NodePtr the_other_node, SimState &sim);
			int getMaxReturnPart(const Task &task, std::string device);
			bool subtractGPUMemUsage(GPUPtr &gpu_ptr, int model_id, NodePtr &node_ptr);
			void revertNode(NodePtr &node_ptr, Task &task, SimState &input);
			bool checkNeedtoRevert(NodePtr &node_ptr, Task &task, SimState &input);
			void getEstimateTrpST(std::string device, const Task &task, int rate, std::vector<NodePtr> &output_vec, const int MAX_PART);
			int getMaxReturnPartST(const Task &task, const int rate);

			//added 2022-06-07, functions for trying out heterogeneous schedulers
			void getMinPartSum(Task &task, std::string &output, const int MAX_PART);
			void getOneFirst(Task &task, std::string &output, const int MAX_PART);
			bool checkTypePriority(std::string &eoutput_type);
			int getNodewithPriorityType(Task &task, std::string type, NodePtr &output_node_ptr, std::vector<NodePtr> given_vector);
			bool _priorityCheck=false;
			std::string _priorityType;
			
			std::map<int, std::vector<SatTrpEntry>*> _perModelSatTable;
			// the amount of memory whenever a new gpu-let needs to be added
			const int _DEFAULT_PYTORCH_MEM_USAGE = 1230; 	
			// amount of memory available actually used, exists for stable exeucution
			// check Memory with a slightly tighter standard(0.9 of capacity) due to engineering issues
			const float _MEM_ROOM = 0.9;
			NetworkLimitChecker _NLC;
			bool _isNLCInitated=false;
	};
} //Scheduling


#else
#endif
