#include "thread.h"
#include "input.h"
#include "global_scheduler.h"
extern FILE* pAppLogFile;
extern SysMonitor ServerState;
extern GlobalScheduler gScheduler;
std::map<std::string,std::deque<std::shared_ptr<AppInstance>>*> PerAppQueue;
std::map<std::string,std::mutex*> PerAppMtx;
std::map<std::string,std::condition_variable*> PerAppCV;


void configureAppSpec(std::string config_json,std::string resource_dir, SysMonitor &state){
	configAppSpec(config_json.c_str(), state, resource_dir);
	for(unsigned int i =0; i < state.getAppSpecVec()->size(); i++){
		initAppThread(&(state.getAppSpecVec()->operator[](i)));
		usleep(500*1000);
	}
}

void addtoAppQueue(std::shared_ptr<AppInstance> appinstance, bool dropped){
	std::string StrName = appinstance->getName();
	if(dropped){
		appinstance->setDropped(true);
	}
	else
		appinstance->setDropped(false);
#ifdef DROP_DEBUG
	std::cout<<" task id : " << appinstance->getTaskID() << " dropped: " << appinstance->isDropped()<<std::endl;
#endif 
	PerAppMtx[StrName]->lock();
	PerAppQueue[StrName]->push_back(appinstance);
	PerAppMtx[StrName]->unlock();
	PerAppCV[StrName]->notify_one();
}

pthread_t initAppThread(AppSpec *App){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8*1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initApp, (void*)App) != 0)
		LOG(ERROR) << "Failed to create a initApp thread.\n";
	return tid;
}

void* initApp(void *args){
	AppSpec* App = (AppSpec*)args;
	std::string AppName = App->getName();
	PerAppQueue[AppName]= new std::deque<std::shared_ptr<AppInstance>>;
	PerAppMtx[AppName]=  new std::mutex();
	PerAppCV[AppName]=   new std::condition_variable();
	std::deque<std::shared_ptr<AppInstance>> *pAppQueue=PerAppQueue[AppName];
	int CompletedTasks=0;
	while(1){
		std::unique_lock<std::mutex> lk(*PerAppMtx[AppName]);
		PerAppCV[AppName]->wait(lk, [&pAppQueue]{return pAppQueue->size();});
		std::shared_ptr<AppInstance> task = pAppQueue->front();
		pAppQueue->pop_front();
		lk.unlock();
		int ret = App->sendOutputtoClient(task->getSocketFD(), task->getTaskID());
		if(ret != EXIT_SUCCESS) {
			printf("ERROR in sending output to client !\n");
		}
		task->writeToLog(pAppLogFile);
	} //infinite while loop
	return (void*)0;
}
