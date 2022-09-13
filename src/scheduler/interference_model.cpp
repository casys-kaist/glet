#include "interference_model.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>

using namespace InterferenceModeling;


void InterferenceModeling::InterferenceModel::setup(std::string model_const_file, std::string util_file)
{
	this->_setup(model_const_file, util_file);
}

void InterferenceModeling::InterferenceModel::_setup(std::string input_file1, std::string input_file2)
{
		
	std::string line;
	std::string token;
	//set mv_intModelConsts
	std::ifstream infile1(input_file1);
	std::getline(infile1, line); // skip first line
	std::getline(infile1, line);
	std::istringstream ss2(line);
	for(int i=0;i<5;i++){
		std::getline(ss2, token, ',');
		InterferenceModeling::InterferenceModel::_intModelConsts.push_back(stod(token));
	}
	//set _utilInfo
	std::ifstream infile2(input_file2);
	std::getline(infile2, line);// skip first line
	while(std::getline(infile2, line)){
		std::pair<std::string, int> model_batch;
		std::vector<double> const_set;
		std::istringstream ss(line);
		std::getline(ss, token, ',');
		model_batch.first=token; // name
		std::getline(ss, token, ',');
		model_batch.second=std::stoi(token); //batch size	
		std::getline(ss, token, ','); // duration
		const_set.push_back(std::stod(token));
		for(int i=0;i<5;i++){
			std::getline(ss, token, ','); // sm_util, l2_util, mem_util, ach_occu, the_occu
			const_set.push_back(stod(token)/100);
		}
		InterferenceModeling::InterferenceModel::_utilInfo[model_batch]=const_set;
	}
}

double InterferenceModeling::InterferenceModel::getInterference(std::string my_model, int my_batch, int my_partition, std::string your_model, int your_batch, int your_partition)
{
    if (my_model == "lenet1" || my_model == "lenet2" || my_model == "lenet3" || my_model == "lenet4" || my_model == "lenet5" || my_model == "lenet6"){
            my_model="lenet";
    }
    if(your_model == "lenet1" || your_model == "lenet2" || your_model == "lenet3" || your_model == "lenet4" || your_model == "lenet5" || your_model == "lenet6"){
            your_model="lenet";
    }

	int my_batch_below=(int)exp2((int)log2(my_batch));
	int my_batch_top=std::min(32, my_batch_below*2);
	int your_batch_below=(int)exp2((int)log2(your_batch));
	int your_batch_top=std::min(32, your_batch_below*2);

	std::pair<std::string, int> my_info_below(my_model, my_batch_below);
	std::pair<std::string, int> my_info_top(my_model, my_batch_top);
	std::pair<std::string, int> your_info_below(your_model, your_batch_below);
	std::pair<std::string, int> your_info_top(your_model, your_batch_top);

	double my_batch_ratio=1.0;
	if(my_batch_top!=my_batch_below){
		my_batch_ratio=(double)(my_batch-my_batch_below)/(my_batch_top-my_batch_below);
	}

	double your_batch_ratio=1.0;
	if(your_batch_top!=your_batch_below){
		your_batch_ratio=(double)(your_batch-your_batch_below)/(your_batch_top-your_batch_below);
	}

    double my_l2_util = InterferenceModeling::InterferenceModel::_utilInfo[my_info_top][2] * my_batch_ratio + \
	InterferenceModeling::InterferenceModel::_utilInfo[my_info_below][2] * (1-my_batch_ratio);
    double your_l2_util = InterferenceModeling::InterferenceModel::_utilInfo[your_info_top][2]*your_batch_ratio + \
	InterferenceModeling::InterferenceModel::_utilInfo[your_info_below][2]*(1-your_batch_ratio);
    double my_dram_util = InterferenceModeling::InterferenceModel::_utilInfo[my_info_top][3] * my_batch_ratio + \
	InterferenceModeling::InterferenceModel::_utilInfo[my_info_below][3] * (1-my_batch_ratio);
    double your_dram_util = InterferenceModeling::InterferenceModel::_utilInfo[your_info_top][3]*your_batch_ratio + \
	InterferenceModeling::InterferenceModel::_utilInfo[your_info_below][3]*(1-your_batch_ratio);
    my_l2_util = my_l2_util *my_partition/100.0;
    your_l2_util = your_l2_util * your_partition/100.0;
    my_dram_util = my_dram_util *my_partition/100.0;
    your_dram_util = your_dram_util * your_partition/100.0;

	double alpha = InterferenceModeling::InterferenceModel::_intModelConsts[0]*my_l2_util+\
                    InterferenceModeling::InterferenceModel::_intModelConsts[1]*your_l2_util+\
                    InterferenceModeling::InterferenceModel::_intModelConsts[2]*my_dram_util+\
					InterferenceModeling::InterferenceModel::_intModelConsts[3]*your_dram_util+\
					InterferenceModeling::InterferenceModel::_intModelConsts[4];

	return alpha;

}
