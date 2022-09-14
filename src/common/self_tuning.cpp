#include "self_tuning.h"
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <map>

SelfTuning::SelfTuning(const std::vector<int> &model_id_vec, int ngpu=4)
{
    assert(ngpu>0);
    assert(!model_id_vec.empty());
    _NUM_OF_GPU=ngpu;
    for(int id : model_id_vec){
        _PerModelAdjustFlag[id]=false;
        _PerModelGPUPart[id]=0;
        #ifdef ST_DEBUG
        std::cout << __func__ <<": model id: "<< id << "initiated to "<< _PerModelGPUPart[id]
        << std::endl;
        #endif
    }
}

float abs_float(float x){
    if(x >=0) return x;
    else return -x;
}

int SelfTuning::GetNewGPUPart(int curr_gpu_part, float rate, int slo, float avg_trpt, float avg_latency){
    #ifdef ST_DEBUG
    std::cout << __func__ << ": called with "
    << " curr_gpu_part: " << curr_gpu_part
    << " rate: " << rate
    << " SLO: " << slo
    << " avg_trpt: " << avg_trpt 
    << " avg_latency" << avg_latency 
    << std::endl;
    #endif
    float res_lat = slo - avg_latency;
    float res_trpt = avg_trpt - rate;
    
    float diff_lat = (res_lat / slo) * 100;
    float diff_trpt = (res_trpt/ rate) * 100;
    #ifdef ST_DEBUG
    
    std::cout << __func__ << ": diff_lat: " << diff_lat << ": diff_trpt: " <<  diff_trpt
    <<std::endl;   
    
    #endif 

    if ( ( diff_lat > 0 && diff_lat < _THRESHOLD) && ( diff_trpt >0 && diff_trpt < _THRESHOLD)) return curr_gpu_part;
    float change_factor =  std::max(abs_float(diff_lat), abs_float(diff_trpt))/100;
    int new_gpu_part, change_gpu_part;
    change_gpu_part = int(change_factor*curr_gpu_part);

    if(res_lat <= 0 || res_trpt <= 0) new_gpu_part= curr_gpu_part + change_gpu_part;
    if(res_lat >0 &&  res_trpt >0) new_gpu_part=curr_gpu_part - change_gpu_part;
    #ifdef ST_DEBUG
    
    std::cout << __func__ << ": new_gpu_part: " << new_gpu_part 
    <<std::endl;    
    
    #endif 
    return new_gpu_part;
}




int SelfTuning::GetNextBatchSize(int curr_batch, int SLO, float avg_latency, float rate){
    // during startup, this sometime happens(when there is no significant amout of data from server)
    if(avg_latency==0) return curr_batch;

    int new_batch;
    float res_lat = (SLO-avg_latency)/SLO;
    // normalize decreasing res_lat
    res_lat = std::max(res_lat, float(-1.0));
    new_batch = curr_batch + (res_lat*curr_batch);
    new_batch=std::min(_MAX_BATCH, new_batch);
    new_batch=std::max(1,new_batch);
   return new_batch;

}
bool SelfTuning::GetNeedAdjust(int model_id){
    assert(_PerModelAdjustFlag.find(model_id) != _PerModelAdjustFlag.end());
    return _PerModelAdjustFlag[model_id];
}
void SelfTuning::SetNeedAdjust(int model_id, bool flag){
    assert(_PerModelAdjustFlag.find(model_id) != _PerModelAdjustFlag.end());
    _PerModelAdjustFlag[model_id]=flag;
}
std::unordered_map<int,int>* SelfTuning::GetPerModelParts(){
    return &_PerModelGPUPart;
}

bool cmp_pair(const std::pair<int,int>&a, const std::pair<int,int> &b){
    return a.second <= b.second;
}

// assume avail_parts are sorted in ascending order
int getNextLargestPart(int part,const  std::vector<int> &avail_parts){
    bool found=false;
    int the_part;
    for(int avail_part : avail_parts){
        if(avail_part >= part){
            the_part = avail_part;
            found=true;
            break;
        }

    }
    //if not found, return maximum part
    if(!found) return avail_parts.back();
    else return the_part;
}
int getSecondLargestPart(int part,const  std::vector<int> &avail_parts){
    int the_part;
    int idx=0;
    for(int avail_part : avail_parts){
        if(avail_part >= part){
            the_part = part;
            break;
        }
        idx++;
    }
    //if there was no second largest part, then return min
    if(idx==0) avail_parts.front();
    return avail_parts[idx-1];
}


std::map<int,std::vector<int>> SelfTuning::AllocateMinMaxFair(std::unordered_map<int,int> &per_model_new_parts,  std::vector<int> &avail_parts){
    int total_part = _NUM_OF_GPU * 100;
       // sort 
    std::vector<std::pair<int,int>> temp_vec(per_model_new_parts.begin(), per_model_new_parts.end());
    std::sort(temp_vec.begin(),temp_vec.end(),cmp_pair);    
    std::sort(avail_parts.begin(),avail_parts.end());
    // get total amount of parts possible
    int avail_sum_of_parts = _NUM_OF_GPU  * 100;

    //allocate Max-Min / by availalbe gpu parts
    int num_of_remaining_tasks = per_model_new_parts.size();
    int avg_part=avail_sum_of_parts / num_of_remaining_tasks;
    std::map<int,std::vector<int>> PerModelResult;
    for(std::vector<std::pair<int,int>>::iterator it = temp_vec.begin(); it != temp_vec.end(); it++){
        int original_part=it->second;

        std::cout << "original_part: " << original_part << " avg_part: " << avg_part << std::endl;
        // if tasks original requirments are bigger, then just allocate avg
        if(original_part > avg_part){
            original_part=avg_part;
        }
        while(original_part >0){
            // if task will be  over-allocated
            int new_part= getNextLargestPart(original_part, avail_parts);

            PerModelResult[it->first].push_back(new_part);
            original_part-=new_part;
        }
        int new_sum=0;
        for(auto part: PerModelResult[it->first]) new_sum+=part;
        // update accordingly to new_sum
        it->second=new_sum;
        std::unordered_map<int,int>::iterator fit=per_model_new_parts.find(it->first);
        assert(fit != per_model_new_parts.end());
        fit->second=new_sum;
        avail_sum_of_parts-=new_sum;
        num_of_remaining_tasks--;
        // do not get average of last task , causes divide-by-zero if not checked
        if(num_of_remaining_tasks!=0) avg_part = avail_sum_of_parts / num_of_remaining_tasks;
    }
    #ifdef ST_DEBUG
        std::cout << __func__ << ": right after fairness distribution : "
        <<std::endl;
        for(auto pair_info : PerModelResult) {
            
           std::cout << __func__ << ": model " << pair_info.first << " results: " 
            <<std::endl;
            for(int part : pair_info.second){
                std::cout << " "<< part;
            }
            std::cout << std::endl;
        }
        
    #endif 

    return PerModelResult;
}

