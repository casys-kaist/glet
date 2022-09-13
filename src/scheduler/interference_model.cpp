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
 

}
