#ifndef __SELF_TUNING_H__
#define __SELF_TUNING_H__

#include <unordered_map> 
#include <vector>
#include <utility>
#include <map>

class SelfTuning{

    public:
        SelfTuning(const std::vector<int> &model_id_vec, int ngpu);
        ~SelfTuning(){};
        // returns new GPU resource percentage
        int GetNewGPUPart(int curr_gpu_part, float rate, int slo, float avg_trpt, float avg_latency);
        int GetNextBatchSize(int curr_batch, int SLO, float avg_latency, float rate);
        bool GetNeedAdjust(int model_id);
        void SetNeedAdjust(int model_id, bool flag);
        std::unordered_map<int,int>* GetPerModelParts();
        std::map<int,std::vector<int>> AllocateMinMaxFair(std::unordered_map<int,int> &per_model_new_parts, std::vector<int> &avail_parts);

    private:
        // per model flag vector tracking whether adjustment is needed or not 
        std::unordered_map<int,bool> _PerModelAdjustFlag; 
        // amount of gpu computing rseource which needs to be allocated
        std::unordered_map<int,int> _PerModelGPUPart;
        // number of GPU, used in AllocateMinMaxFair
        int _NUM_OF_GPU;
        // threshold for everything in this algorithm
        const int _THRESHOLD=5;
        // max batch size when using SLAB
        const int _MAX_BATCH=32;
};


#else
#endif
