#ifndef _SYS_MONITOR_H__
#define _SYS_MONITOR_H__

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
#include "scheduler_incremental.h"
#include "EWMA.h"
#include "proxy_ctrl.h"
#include "gpu_utils.h"
#include "self_tuning.h" 
#include "load_balancer.h"
#include "backend_proxy_ctrl.h"


typedef struct _TaskSpec{
	int device_id;
	std::string ReqName;
	int BatchSize;
	int dedup_num;
	int CapSize; // maximum amount of cap for this task, used in MPS environment
	proxy_info* proxy;
} TaskSpec; // mostly used for scheduling

typedef struct _load_args{
	proxy_info *pPInfo;
	std::vector<std::pair<int,int>> model_ids_batches;
} load_args;


class SysMonitor{
	public:
		SysMonitor();
		~SysMonitor();
		void setupProxy(proxy_info* pPInfo);

		std::map<std::string, std::queue<std::shared_ptr<request>>>* getReqListHashTable();
		std::queue<std::shared_ptr<request>>* getRequestQueueofNet(std::string str_req_name);
		bool isQueueforNet(std::string str_req_name);
		void addNewQueueforNet(std::string str_req_name);
		std::vector<std::string>* getVectorOfNetNames();
		uint32_t getLenofReqQueue(std::string str_req_name);
		moodycamel::ConcurrentQueue<std::shared_ptr<request>>* getCompQueue();
		std::deque<std::shared_ptr<TaskSpec>>* getBatchQueueofProxy(proxy_info* pPInfo);

		void insertToBatchQueueofProxy(proxy_info* pPInfo, std::shared_ptr<TaskSpec> task_spec);
		uint32_t getSizeofBatchQueueofProxy(proxy_info *pPInfo);
		
		std::vector<std::pair<std::string, int>>* getProxyNetList(proxy_info* pPInfo);
		uint32_t getProxyNetListSize(proxy_info* pPInfo);
		void insertNetToProxyNetList(proxy_info* pPInfo, std::pair<std::string, int>& pair);
		bool isProxyNetListEmpty(proxy_info* pPInfo);

		std::vector<proxy_info*> *getDevMPSInfo(int dev_id);
		void insertMPSInfoToDev(int dev_id, proxy_info* pPInfo);
		
		int getIDfromModel(std::string net);
		void setIDforModel(std::string net, int id);

		// used for trackign per model rate
		int getPerModelCnt(std::string model);
		void setPerModelCnt(std::string model, int cnt);
		void incPerModelCnt(std::string model);

		// used for tracking per model trpt
		int getPerModelFinCnt(std::string model);
		void setPerModelFinCnt(std::string model, int cnt);
		void incPerModelFinCnt(std::string model);

		void setNGPUs(int ngpu);
		int getNGPUs();
		void setNumProxyPerGPU(int n_proxy_per_gpu);
		int getNumProxyPerGPU();

	
		
	private:
};

#endif 
