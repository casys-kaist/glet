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
}
// acts like an hash key
int LatencyModel::makeKey(int batch, int part){ 
}

Entry* LatencyModel::parseKey(int key){
}

std::pair<int,int> findBatchpair(std::vector<int> &list, int batch, int part)
{
}


 float LatencyModel::getBatchPartInterpolatedLatency(std::string model, int batch, int part){
 }

 float LatencyModel::getBatchInterpolatedLatency(std::string model, int batch, int part){
 }

float LatencyModel::getLatency(std::string model, int batch, int part){
}


float LatencyModel::getGPURatio(std::string model, int batch, int part){
}

