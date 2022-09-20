#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <assert.h>
#include "config.h"
#include "boost/program_options.hpp"
#include "scheduler_incremental.h"
#include "scheduler_utils.h"

namespace po = boost::program_options;

std::vector<int> AVAIL_PARTS;

po::variables_map parse_opts(int ac, char** av) {
	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()("help,h", "Produce help message")
		("resource_dir,rd", po::value<std::string>()->default_value("../resource"),"directory which hold resource files")
		("task_config,tc", po::value<std::string>()->default_value("tasks.csv"),"csv file with each task's SLO and ")
		("sim_config", po::value<std::string>()->default_value("sim_config.json"),"json file which hold simulation configurations")
		("output", po::value<std::string>()->default_value("ModelList.txt"),"txt file which hold simulation results")
		("mem_config", po::value<std::string>()->default_value("mem-config.json"),"json file which holds the amount of memory each model+input uses")
		("full_search", po::value<bool>()->default_value(false),"flag: conduct full search or not")
		("proxy_config", po::value<std::string>()->default_value("proxy_config.json"),"json file which holds info input data")
		("device_config", po::value<std::string>()->default_value("device-config.json"),"json file which holds per device type data");

	po::variables_map vm;
	po::store(po::parse_command_line(ac, av, desc), vm);
	po::notify(vm);
	if (vm.count("help")) {
		std::cout << desc << "\n";
		exit(1);
	}
	return vm;
}


void fillPossibleCases2(std::vector<std::vector<Node>> *pVec, int ngpu){
}

void writeSchedulingResults(std::string filename, SimState *simulator, Scheduling::BaseScheduler &sched){
}

int main(int argc, char* argv[])
{
	po::variables_map vm = parse_opts(argc, argv);
	std::vector<std::string> files={"1_28_28.txt", "3_224_224.txt", "3_300_300.txt"};
	Scheduling::IncrementalScheduler  SBP;
	bool success=  SBP.initializeScheduler(vm["sim_config"].as<std::string>(),\
			vm["mem_config"].as<std::string>(),
			vm["device_config"].as<std::string>(),
			vm["resource_dir"].as<std::string>(),
			files);
	if(!success){
		std::cout << "Failed to initialize scheduler" << std::endl;
		exit(1);
	}
#ifdef CHECK_NETBW
	SBP.setupNetworkChecker(vm["resource_dir"].as<std::string>()+"/"+vm["proxy_config"].as<std::string>());
#endif
	std::vector<Task> task_list;
	if(SBP.getUseParts()) AVAIL_PARTS={50,60,80,100}; // other part will be 100-20, 100-40, 100-50 and so on
	//if(SBP.GetUseParts()) AVAIL_PARTS={50,}; // other part will be 100-20, 100-40, 100-50 and so on


	else AVAIL_PARTS={100}; // other part will be 100-20, 100-40, 100-50 and so on
	SimState simulator;
	SimState final_output;
	SBP.setupAvailParts(AVAIL_PARTS);

	std::string output_file = vm["output"].as<std::string>();

	std::vector<SimState> sim_list;
	SimState best_sim;
	SBP.setupTasks(vm["task_config"].as<std::string>(), &task_list);
	std::vector<int> per_dev_mem;
}
