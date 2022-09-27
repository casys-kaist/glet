#ifndef __INPUT_
#define __INPUT_
#include <string>
#include <opencv2/opencv.hpp>
#include <torch/script.h>
#include <torch/serialize/tensor.h>
#include <torch/serialize.h>
#include "sys_monitor.h"
#include "global_scheduler.h"

torch::Tensor getRandLatVec(int batch_size);
torch::Tensor getRandImgTensor();
int readImgData(const char *path_to_txt, int num_of_img);
int configAppSpec(const char* ConfigJSON,SysMonitor &SysState, std::string res_dir);
int configGlobalScheduler(const char* ConfigJSON,SysMonitor &SysState, std::string res_dir, GlobalScheduler &scheduler);
int readAppJSONFile(const char* AppJSON, AppSpec &App, std::string res_dir);
int readInputDimsJsonFile(const char* configJSON, SysMonitor &ServerState, std::unordered_map<int,std::vector<uint64_t>> &InputDimMapping);
#else
#endif 
