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

void fillPossibleCases2(std::vector<std::vector<Node>> *pVec, int ngpu){
}

void writeSchedulingResults(std::string filename, SimState *simulator, Scheduling::BaseScheduler &sched){
}

int main(int argc, char* argv[])
{

}
