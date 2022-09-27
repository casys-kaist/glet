#include "input.h"
#include "json/json.h"
#include <opencv2/opencv.hpp>
#include <vector> 
#include <map>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "global_scheduler.h"
#include "torch_utils.h"


#define NZ 100 
using namespace cv;
std::vector<torch::Tensor> glbImgTensors;
unsigned int ID=0;
extern GlobalScheduler gScheduler;
extern SysMonitor ServerState;
const int DEFAULT_MAX_BATCH=32;
const int DEFAULT_MAX_DELAY=0;
std::mutex img_index_mtx;

#ifdef BACKEND
extern std::map<int,TensorSpec*> MapIDtoSpec;
#endif

int readInputDimsJsonFile(const char *configJSON, SysMonitor &SysState, std::unordered_map<int, std::vector<uint64_t>> &InputDimMapping){
    std::cout << __func__ << ": reading JSON File: " << configJSON
    <<std::endl;
    Json::Value root;
    std::ifstream ifs;
    ifs.open(configJSON);
    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << __func__ << ": Failed parsing from stream" << std::endl;
        std::cout << errs << std::endl;
        ifs.close();
        return EXIT_FAILURE;
    }
    for(unsigned int i=0; i< root["Models"].size(); i++){
        std::string name = root["Models"][i]["name"].asString();
        int id = root["Models"][i]["proxy_id"].asInt();
        SysState.setIDforModel(name,id);
        #ifdef DEBUG
        std::cout << __func__ <<": setted up " << name.c_str() << " as " << id
        <<std::endl;
        #endif
         for(unsigned int j=0; j<root["Models"][i]["input_dim"].size(); j++ ){
            InputDimMapping[id].push_back(root["Models"][i]["input_dim"][j].asInt());
        }
    }
    ifs.close();
    return EXIT_SUCCESS;
}

torch::Tensor convertToTensor(float *input_data, int batch_size, int nz){
	return convert_LV_vectors_to_tensor(input_data, batch_size,nz);
}



int configAppSpec(const char* ConfigJSON, SysMonitor &SysState, std::string res_dir){
#ifdef DEBUG
	printf("%s: Reading %s \n", __func__, ConfigJSON);
#endif 
	Json::Value root;
	std::ifstream ifs;
	ifs.open(ConfigJSON);
	if(!ifs){
		std::cout << __func__ << ": failed to open file: " << ConfigJSON
			<<std::endl;
		exit(1);
	}


	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;
	if (!parseFromStream(builder, ifs, &root, &errs)) {
		std::cout << errs << std::endl;
		return EXIT_FAILURE;
		ifs.close();
	}
	for(unsigned int i=0; i<root["App_specs"].size(); i++){

		AppSpec *temp = new AppSpec(root["App_specs"][i]["name"].asString());
		if(readAppJSONFile(root["App_specs"][i]["file"].asCString(), *temp, res_dir)){
			std::cout << __func__ << ": failed to setup " << root["App_specs"][i]["file"].asCString()
				<<std::endl;
			continue;
		}
		int len = SysState.getAppSpecVec()->size();
		temp->setGlobalVecID(len);
		SysState.getAppSpecVec()->push_back(*temp);

	}
	ifs.close();
	return EXIT_SUCCESS;
}

int readAppJSONFile(const char* AppJSON, AppSpec &App, std::string res_dir){
	Json::Value root;
	std::ifstream ifs;
	std::string full_name = res_dir + "/"+std::string(AppJSON);
#ifdef DEBUG
	printf("%s: Reading App JSON File: %s \n", __func__, full_name.c_str());
#endif 
	ifs.open(full_name);
	// fail check
	if(!ifs){
		std::cout << __func__ << ": failed to open file: " << full_name
			<<std::endl;
		exit(1);
	}

	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;
	if (!parseFromStream(builder, ifs, &root, &errs)) {
		std::cout << errs << std::endl;
		ifs.close();
		return EXIT_FAILURE;
	}
	//set input dimensions
	std::vector<int> InputDim;
	for(unsigned int i=0; i<root["Input"].size(); i++){
		TensorSpec *tpTensor=new TensorSpec();
		for(unsigned int j=0; j<root["Input"][i]["InputDim"].size();j++){
			tpTensor->dim.push_back(root["Input"][i]["InputDim"][j].asInt());

		}
		tpTensor->id=root["Input"][i]["ID"].asInt();

		if(root["Input"][i]["InputType"].asString() == "FP32")
			tpTensor->dataType=KFLOAT32;
		else if(root["Input"][i]["InputType"].asString() == "INT64")
			tpTensor->dataType=KINT64;
		else{
			LOG(ERROR) << "wrong type of InputType: "<< root["Input"][i]["InputType"].asString() <<std::endl;
			return EXIT_FAILURE;
		}
		for(unsigned int j=0; j<root["Input"][i]["output"].size();j++){
			tpTensor->output.push_back(root["Input"][i]["output"][j].asInt());
		}
		App.addInputSpec(*tpTensor);
	}
	//setup model-wise restrictions
	const int DEFAULT_MAX_BATCH=32;
	const int DEFAULT_MAX_DELAY=0;
	//setup model dependencies
	if(App.setupModelDependency(full_name.c_str())){
		std::cout << __func__ << ": failed to setup dependency for " << full_name
			<<std::endl;
		return EXIT_FAILURE;
	}
	ifs.close();
	return EXIT_SUCCESS;
}


