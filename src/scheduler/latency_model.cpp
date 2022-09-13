#include "latency_model.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <cassert>
#include <algorithm>

#define MAX_BATCH 32
#define MIN_BATCH 1


void LatencyModel::setupTable(std::string TableFile){
    std::string str_buf;
    std::ifstream file(TableFile);
    std::string line;
    #ifdef DEBUG
    std::cout << __func__ << " called for " << TableFile << std::endl;
    #endif
    while(std::getline(file, line)){
        std::istringstream iss(line);
        std::string field;
        Entry new_entry;
		std::unordered_map<int,float> per_entry_latency;
        std::getline(iss, field,',');
        std::string model=field;
		std::map<std::string,std::unordered_map<int,float>*>::iterator it = _perModelLatnecyTable.find(model);
		if(it == _perModelLatnecyTable.end()){
			_perModelLatnecyTable[model]=new std::unordered_map<int,float>();
            _perModelGPURatioTable[model]=new std::unordered_map<int,float>();
		}
	    std::getline(iss, field,',');
        new_entry.part=stoi(field);
        std::getline(iss, field,',');
        new_entry.batch=stoi(field);
        std::getline(iss, field,',');
        new_entry.latency=stof(field);
	    int key=makeKey(new_entry.batch, new_entry.part);
        _perModelLatnecyTable[model]->operator[](key)=new_entry.latency;
        _perModelBatchVec[model][new_entry.part].push_back(new_entry.batch);      
    } 
}

// acts like an hash key
int LatencyModel::makeKey(int batch, int part){
    return batch*1000 + part;
}

Entry* LatencyModel::parseKey(int key){
    Entry *new_entry  = new Entry();
    new_entry->batch = key/1000;
    new_entry->part =  key % 1000;
    return new_entry;
}


std::pair<int,int> findBatchpair(std::vector<int> &list, int batch, int part)
{
    assert(MIN_BATCH < batch && batch < MAX_BATCH);
    std::pair<int,int> retPair;
    std::vector<int>::iterator it;
    int lowerbatch = batch;
    while(true){
        it=find(list.begin(), list.end(), lowerbatch);
        if(it !=list.end()) {
           retPair.first=lowerbatch;
           break;
        }
        lowerbatch--;
    }
    int upperbatch = batch;
    while(true){
        upperbatch++;
        it=find(list.begin(), list.end(), upperbatch);
        if(it !=list.end()) {
           retPair.second=upperbatch;
           break;
        }
    }
    return retPair;
}


 float LatencyModel::getBatchPartInterpolatedLatency(std::string model, int batch, int part){
     std::vector<int> keys_vec;
    for(std::unordered_map<int,float>::iterator it = _perModelLatnecyTable[model]->begin();
        it != _perModelLatnecyTable[model]->end();it++ )
    {
        keys_vec.push_back(it->first);
    }
    assert(keys_vec.size() >= 2);
    sort(keys_vec.begin(), keys_vec.end());
    Entry* temp = parseKey(keys_vec.front());
    int min_part = temp->part;
    delete temp;
    temp = parseKey(keys_vec.back());
    int max_part = temp->part;
    delete temp;
    // assume that every part has max batch size profiled
    int temp_key1 = makeKey(MAX_BATCH,min_part);
    int temp_key2 = makeKey(MAX_BATCH,part);
    int temp_key3 = makeKey(MAX_BATCH,max_part);
    float y1 = getBatchInterpolatedLatency(model,MAX_BATCH,min_part);
    float y =  getBatchInterpolatedLatency(model,MAX_BATCH,part);
    float y2 = getBatchInterpolatedLatency(model,MAX_BATCH,max_part);
    float b=(y-y2)/(y1-y2);
    y1=getBatchInterpolatedLatency(model,batch,min_part);
    y2=getBatchInterpolatedLatency(model,batch,max_part);
    float diff=y1-y2;
    return diff*b + y2;
 }

 float LatencyModel::getBatchInterpolatedLatency(std::string model, int batch, int part){
    uint64_t p1,p2,p3,p4;
    // if batch is in the table, lookup and return
    if(batch == MIN_BATCH || batch == MAX_BATCH){
        return _perModelLatnecyTable[model]->operator[](makeKey(batch,part));
    } 
    // if not, do interpolation
    std::pair<int,int> two_batch = findBatchpair(_perModelBatchVec[model][part], batch, part);

    int b1 = two_batch.first;
    int b2 = two_batch.second;
    float l1=_perModelLatnecyTable[model]->operator[](makeKey(b1,part));
    float l2=_perModelLatnecyTable[model]->operator[](makeKey(b2,part));
    assert(l1 != 0.0 && l2 != 0.0);
    float ret_latency = (l2-l1)/float(b2-b1) * (batch-b1) + l1;
    return ret_latency;
 }

float LatencyModel::getLatency(std::string model, int batch, int part){
}


float LatencyModel::getGPURatio(std::string model, int batch, int part){
     assert(MIN_BATCH <= batch && batch <= MAX_BATCH);
    if (model == "lenet1" || model == "lenet2" || model == "lenet3" \
    || model == "lenet4" || model == "lenet5" || model=="lenet6"){
        model="lenet1";
    }
    uint64_t p1,p2,p3,p4;
    // if batch is in the table, lookup and return
    if(batch == MIN_BATCH || batch == MAX_BATCH){
        return _perModelGPURatioTable[model]->operator[](makeKey(batch,part));
    } 
    // if not, do interpolation
    std::pair<int,int> two_batch = findBatchpair(_perModelBatchVec[model][part], batch, part);
    int b1 = two_batch.first;
    int b2 = two_batch.second;
    float g1=_perModelGPURatioTable[model]->operator[](makeKey(b1,part));
    float g2=_perModelGPURatioTable[model]->operator[](makeKey(b2,part));
    assert(g1 != 0.0 && g2 != 0.0);
    //2. do linear interpolation and return;
    float ret_gpu_ratio = (g2-g1)/float(b2-b1) * (batch-b1) + g1;
    return ret_gpu_ratio;
}