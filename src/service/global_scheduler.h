#ifndef _ROUTER_H__
#define _ROUTER_H__

#include <deque>
#include <vector>
#include <queue>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <boost/lockfree/queue.hpp>
#include <thread>
#include "batched_request.h"
#include "concurrentqueue.h"
#include "gpu_proxy.h"
#include "interference_model.h"
#include "latency_model.h"
//#include "scheduler_incremental.h"
#include "EWMA.h"
#include "proxy_ctrl.h"
#include "gpu_utils.h"
#include "load_balancer.h"
#include "backend_proxy_ctrl.h"
#include "sys_monitor.h"

class GlobalScheduler
{

	public: 
		GlobalScheduler();
		~GlobalScheduler();
		//below are methods related to scheduling
		std::vector<std::shared_ptr<TaskSpec>> executeScheduler(SysMonitor *SysState);
		void doMPSScheduling(SysMonitor* SysState);
		// below are schedulers called according to _mode
		std::vector<std::shared_ptr<TaskSpec>> staticMPSScheduler(SysMonitor *SysState);
		std::vector<std::shared_ptr<TaskSpec>> OracleScheduler(SysMonitor *SysState);
		std::vector<std::shared_ptr<TaskSpec>> STScheduler(SysMonitor *SysState);
		/*methods for setting up*/
		int setSchedulingMode(std::string mode, SysMonitor *SysState);
		void setLoadBalancingMode(std::string mode);
		// mps_static specific load balance setup, called in 
		void setupLoadBalancer(std::map<proxy_info*, std::vector<std::pair<int,double>>> &mapping_trps);
		void setupScheduler(SysMonitor *SysState, std::map<std::string,std::string> &param_file_list, std::string scheduler, std::string res_dir);
		void setupSBPScheduler(SysMonitor *SysState, std::map<std::string,std::string> &param_file_list, std::string res_dir);
		void setupProxys(SysMonitor *SysState, std::string model_list_file);
		void setupInitTgtRate(std::string rate_file);
		void setupLatencyModel(std::string latency_file);
		void setupEstModels();
		void setupMPSInfo(SysMonitor *SysState, std::string capfile);
		void createProxyInfos(SysMonitor *SysState, std::vector<int> parts, int node_id, int ngpus, std::string type);
		void setupProxyTaskList(std::string list_file, SysMonitor *SysState);
		void setupPerModelLatencyVec(SysMonitor *SysState);
		void setupModelSLO(std::string name, int SLO);
		void bootProxys(SysMonitor *SysState);
		proxy_info* getProxyInfo(int dev_id, int thread_cap, int dedup_num, SysMonitor *SysState);
		std::vector<Task> getListofTasks(SysMonitor *SysState);
		std::vector<int> getListofParts(SysMonitor *SysState);
		std::vector<std::vector<int>> getParts(SysMonitor *SysState);
		void loadAllProxys(SysMonitor *SysState);
		void loadModelstoProxy(proxy_info* pPInfo, SysMonitor *SysState);
		void unloadModelsfromProxy(proxy_info* pPInfo, SysMonitor *SysState);   
		void setMaxBatch(std::string name, std::string type, int max_batch);
		void setMaxDelay(std::string name, std::string type, int max_delay);  
		float getTaskTgtRate(std::string model_name);  
		/*methods for updating*/
		void initReqRate(std::string ReqName, int monitor_interval_ms, int query_interval_ms, float init_rate);
		void updateAvgReqRate(std::string ReqName, uint64_t timestamp);
		float getTaskArivRate(std::string ReqName);
		void addtoNetNames(std::string name);
		void removeMaxRates();  
		// conduct scheduling
		bool doScheduling(std::vector<Task> &task_list, SysMonitor *SysState,SimState &curr_decision, SimState &new_decision);
		// recovers fron scheduliong failure
		void recoverFromScheduleFailure(SysMonitor *SysState, std::map<std::string,float> &backup_rate, SimState &backup);
		//setup only tasks that need reschediling
		void setupTasks(SysMonitor *SysState, bool include_downsize, std::vector<Task> &output_vec);
		//setup all tasks for scehduling
		void setupAllTasks(SysMonitor *SysState, std::vector<Task> &output_vec);
		// fill task_list with current configured rate
		void refillTaskList(SysMonitor *SysState, std::vector<Task> &task_list);
		//updates history of rates and returns maximum rate
		float updateAndReturnMaxHistRate(std::string model_name, const float rate);
		void applyReorganization(SysMonitor *SysState, const SimState &output);
		void RevertTgtRates(SimState &backup); 
		/*methods for getting numbers */
		float getMaxDelay(proxy_info *pPInfo);
		std::vector<std::string>* getNetNames();
		int getSLO(std::string name);
		float getEstLatency(std::string task, int batch, int cap, std::string type);
		float getBeaconInterval();
		int getModelID(std::string name);
		bool detectRepart(SysMonitor *SysState, GPUPtr &gpu_ptr);
		/*methods for updating latency related info*/
		float getTailLatency(proxy_info* pPInfo, std::string modelname);
		float getAvgLatency(proxy_info* pPInfo, std::string modelname);
		void clearPerModelLatency(proxy_info *pPInfo, std::string model);
		void addLatency(proxy_info *pPInfo, std::string modelname, float latency);
		//void initPerModelLatEWMA(SysMonitor *SysState, int monitor_interval_ms, int query_interval_ms);
		float getAvgLatency(std::string modelname);
		void clearPerModelLatency(std::string model);
		void addLatency(std::string modelname, float latency);
		void initPerModelTrptEWMA(SysMonitor *SysState, int monitor_interval_ms, int query_interval_ms);
		float getAvgTrpt(std::string modelname);
		void clearPerModelTrpt(std::string model);
		void addTrpt(std::string modelname, float tprt);
		void UpdateBatchSize(SysMonitor *SysState, std::string model_name, int new_batch_size);
		void initPerModelPartSum(SysMonitor *SysState, std::unordered_map<int,int> &sum_of_parts);

		void additionalLoad(GPUPtr &gpu_ptr, SysMonitor *SysState);
		void unloadModels(GPUPtr &gpu_ptr, SysMonitor *SysState);
		void unloadModels(SimState &prev_state, SimState &curr_state, SysMonitor *SysState);
		bool checkIfLoaded(proxy_info *pPInfo, std::string modelname);
		bool needToUnload(NodePtr &node_ptr, _proxy_info* pPInfo);
		bool needToLoad(NodePtr &node_ptr, _proxy_info* pPInfo);
		void updateGPUMemState(SimState &input, SysMonitor *SysState);
		void updateGPUMemState(SimState &input, SysMonitor *SysState, int num_of_gpus_to_update);
		bool checkLoad(int model_id, proxy_info* pPInfo);
		//below are scale related functions!!
		// checks and reduce the number of schedulable GPUs, returns true if reduced, false it was not reduced
		bool checkandReduceGPU(SimState &decision, SysMonitor *SysState);
		// totally flushes and updates memory configuration
		void flushandUpdateDevs(SimState &input, const int num_of_gpus);
		// functions used for retrieving memory information of GPU on remote node
		int getTotalMemory_backend(int gpu_id, int node_id);
		int getUsedMemory_backend(proxy_info *pPInfo);
		// usually called with update updateGPUMemState
		void updateGPUModelLoadState(SimState &input, SysMonitor *SysState);
		void* loadModel_sync_wrapper(void *args);
		void loadModel_sync(proxy_info *pPInfo, std::vector<std::pair<int,int>> &model_ids);
		void* unloadModel_async(void *args);
		void applyRouting_sync(SimState &output, SysMonitor *SysState);
		void bootProxy_sync(proxy_info* pPInfo);
		void shutdownProxy_sync(proxy_info* pPInfo);
		int FullRepartition(SimState &input, SimState &output, SysMonitor *SysState);
		int NonRepartition(SimState &input, SimState &output, std::vector<Task> &session, SysMonitor *SysState);
		int oneshotScheduling(SysMonitor *SysState);
		int readAndBoot(SysMonitor *SysState, std::string model_list_file);
		void setupBackendProxyCtrls();
		int setupModelwithJSON(const std::string &config_file, const std::string &res_dir,GlobalScheduler &scheduler);
		int setupModelforSchduling(const std::string &app_json_file, const std::string &res_dir,GlobalScheduler &scheduler);

	private: 
		scheduler mSchedulerMode;
		//repartition_policy mRepartitionMode;
		LoadBalancer mLoadBalancer;
		std::vector<std::string> mNetNames; // vector containing names of network that can run on the server
		std::map<std::tuple<std::string, std::string>, int> mMaxBatchTable;//max batch table 
		std::map<std::tuple<std::string, std::string>, int> mMaxDelayTable; //max delay table
		// used in self-tuning scheduler 
		std::map<std::string, int> mPerModelLastBatch; 
		std::map<std::string, std::deque<float>> mPerTaskAvgArivInterval;  // used in get/set arriving intervals
		std::map<std::string, std::shared_ptr<EWMA::EWMA>> mPerTaskEWMARate; // EWMA value of rate
		std::map<std::string, std::shared_ptr<EWMA::EWMA>> mPerTaskEWMALat; // EWMA value of avg latency
		std::map<std::string, std::shared_ptr<EWMA::EWMA>> mPerTaskEWMATrpt; // EWMA value of avg throughput
		std::map<std::string, std::deque<float>> mPerTaskRateHist;  // used in get/set arriving intervals
		std::map<std::string, int> mPerTaskDownCnt;  // use
		std::map<std::string, std::mutex*> PerTaskArrivUpdateMtx; 
		std::map<std::string, std::mutex*> PerTaskLatUpdateMtx; 
		std::map<std::string, std::mutex*> PerTaskTrptUpdateMtx; 
		std::map<std::string,uint64_t> mPerTaskLastTimestamp; 
		std::map<std::string, float> mPerTaskTgtRate;  //  used in scheduling
		/*tables used for STScheduler only */
		std::map<std::string, int> mPerTaskSLOSuccess; 
		std::unordered_map<int, int> mPerTaskSumofPart;

		InterferenceModeling::InterferenceModel mInterferenceModel;   
		LatencyModel mLatencyModel;
		std::map<std::string,int> mPerModelSLO;
		// the state currently scheduled
		SimState mCurrState;
		// store state that needs to be done before the start of the next epoch
		SimState mNextState;
		//store previously loaded state, which gives information of unneeded models
		SimState mPrevState;

		bool mNeedUpdate=false;
		Scheduling::IncrementalScheduler SBPScheduler;
		SelfTuning *pSTScheduler;
		BackendProxyCtrl* mProxyCtrl;
		const unsigned int MA_WINDOW=10; //used for tracking moving average, this is the number of records to keep when getting average
		const float SLO_GUARD=0.05;
		const int SKIP_CNT=100; // used for skipping tasks, especially useful for preventing resource osciliation
		const float LATENCY_ROOM=1.05;
		uint64_t BEACON_INTERVAL; 
		uint64_t EPOCH_INTERVAL;
		uint64_t REORG_INTERVAL;
		float HIST_LEN; // used with mPerTaskRateHist, initiated in EpochScheduler limits the number of past history the deque will hold
		float DOWN_COOL_TIME; // used and setted in SetupTasks, based on BEACON_INTERVAL
		bool ALLOW_REPART=false;

};



#endif 
