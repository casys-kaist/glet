#ifndef NETWORK_LIMIT_H__
#define NETWORK_LIMIT_H__

#include "scheduler_base.h"

class NetworkLimitChecker{
    public:
    NetworkLimitChecker();
    ~NetworkLimitChecker();
    int setupPerTaskInputDimension(std::string json_file);
    double getRequiredBandwidth(GPUPtr gpu);
    bool isBandwidthOK(SimState decision);
    bool isBandwidthOK(GPUPtr gpu);
    // returns new batch size;
    int adjustBatchSizetoNetBW(TaskPtr &task_ptr, GPUPtr &to_be_inserted_gpu);
    private:
    // for now just use a constant threshold to compare whether is is OK or not
    const double _LIMIT_GBITS = 3.0;
    std::unordered_map<int,int> _taskIDtoRequiredBytesPerRequest;
};

#else
#endif
