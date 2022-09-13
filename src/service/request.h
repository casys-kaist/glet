/*
 *This file is wrapper of a single request
 *
 * */

#ifndef REQUEST_H__
#define REQUEST_H__

#include <string>
#include <vector>
#include <torch/script.h>
#include "app_instance.h"
#include "common_utils.h"

class request{
	private:
		int mTaskID; // the task ID that is managed by the server, for debugging purpose
		int mReqID; // different from above, the ID that client has sent, made for profiling purpose 
		int mBatchID; // the ID provied when forming a batch, used for debugging and logging
		int _cgid;  // setted up with AppInstance, used for identifiying which computation node this request is for
		int mClientFD;
		unsigned int mBatchNum; // batch size
		int mDeviceID; // the device ID this request was allocated to, purely for debugging purpose
		char mReqName[MAX_REQ_SIZE];
		std::shared_ptr<AppInstance> _task;
		bool _drop;
		uint64_t start;
		uint64_t startBatch;
		uint64_t endBatch;
		uint64_t startExec;
		uint64_t endExec;
		uint64_t endSend;
		uint64_t endReq;
		uint64_t endCmpq;

	public:
		std::vector<at::Tensor> _inputTensors; // in case model requires N tensor for inference, we maintain a vector of tensors
		request(int rid, int fd, int bnum);
		~request();
		void setupAppInstance(std::shared_ptr<AppInstance> task);
		void setOutputTensor(torch::Tensor tensor);
		void setFinished();
		void setReqName(const char * net_name);
		char* getReqName();
		int getTaskID();
		int getClientFD();
		int getCGID();
		int getBatchID();
		unsigned int getBatchNum();
		int setDeviceID(int id);
		void setTaskID(int id);
		void setCGID(int id);
		void setBatchID(int id);
		void pushInputData(torch::Tensor &input);
		bool isAppFinished();
		std::shared_ptr<AppInstance> getApp();
		void setStart(uint64_t time);
		void setendBatch(uint64_t time);
		void setstartExec(uint64_t time);
		void setendExec(uint64_t time);
		void setendSend(uint64_t time);
		void setendReq(uint64_t time);
		void setendCmpq(uint64_t time);
		void setstartBatch(uint64_t time);
		void writeToLog(FILE* fp);
		uint64_t getStart(); // used in drop_thraad.cpp
		double getLatency();
		void setDropped(bool drop);
		bool isDropped();

#ifdef DEBUG
		void printTimeStamps();
#endif 
};
#else
#endif 
