#ifndef _LOAD_BALANCER_H__
#define _LOAD_BALANCER_H__
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "gpu_proxy.h"
#include "scheduler_base.h"


/*
   String for each type
   NO = "no"
   WRR="wrr"
   */

// type of load balancers,
enum load_balancer_type {NO=0, WRR=1};

class LoadBalancer{
	public:
		LoadBalancer();
		~LoadBalancer();
		void setType(std::string balancer_type);
		// wrapper function: returns whether the proxy is supposed to take the task(model_name)
		bool checkLoad(int model_id, proxy_info* pPInfo);
		// wrapper function: updates internal tables, returns whether successful or not 
		int updateTable(SimState &sched_results);
		// special wrapper function for testing mps_static scheduler, called in GlobalScheduler::setupLoadBalancer
		int updateTable(std::map<proxy_info *, std::vector<std::pair<int, double>>> &trps);
		proxy_info* choseProxy(int model_id);
	private:
		int renewTable(int model_id, int key, double trpt);
		int checknCreateMtx(int model_id);
		bool checkMinCredit(int model_id, int key);
		int getKey(int model_id, proxy_info* pPInfo);
		int getKey(int model_id, int gpu_id, int part, int dedup_num);
		void clearTables();
		bool containModelID(int key, int model_id);

		void resetCreditforModelID(int model_id);
		const int _FAIL_THRESHOLD=10;
		std::unordered_map<int, std::vector<int>> _TaskIDtoKeysMapping;

		std::unordered_map<int, double> _keyToCreditMapping;
		//checks how much time the key has failed to receive some credit
		std::unordered_map<int, int> _keyToFailMapping;
		std::unordered_map<int, double> _keyToCurrCreditMapping;
		std::unordered_map<int, double> _TaskIDToMinCreditMapping;

		std::unordered_map<int, std::mutex*> _TaskIDtoMtxMapping;
		std::mutex _mtx;
		int updateWRR(SimState &sched_results);
		bool checkWRR(int model_id, proxy_info* pPInfo);
		load_balancer_type _type;

};


#else
#endif
