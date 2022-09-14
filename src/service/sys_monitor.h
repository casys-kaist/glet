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
		// newley added method
		// 1. setupProxy: setsup maps, vectors for proxy
	
		
	private:
};

#endif 
