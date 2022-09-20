#ifndef _SCHEDULER_H__
#define _SCHEDULER_H__

#include <vector>
#include <string>
#include "scheduler_base.h"

void printResults(SimState &input);
void printMemState(const GPUPtr &gpu_ptr);
void copyToOutput(SimState &input, SimState &output);
void copySession( std::vector<Task> &org_session, std::vector<Task> &dst_session); 
void recoverScheduler(const SimState &backup, SimState &output);
TaskPtr createNewTaskPtr(int id=0, int request_rate=0, int SLO=0, int batch_size=0, float throughput=0);
TaskPtr createNewTaskPtr(int id=0, int request_rate=0, int ORG_RATE=0, int additiional_rate=0, 	int SLO=0, int batch_size=0, float throughput=0);
TaskPtr createNewTaskPtr(Task &task);
int getNumofUsedGPUs(SimState &input);
int getMaxPartSize(const SimState &input);
int getMinPartSize(const SimState &input);

#else
#endif
