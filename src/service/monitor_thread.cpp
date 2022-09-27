#include "thread.h"
#include "global_scheduler.h"

#define RATE_INTERVAL 500// rate interval of counting occurnces, in ms

FILE *gpu_log;
extern GlobalScheduler gScheduler;
extern SysMonitor ServerState;

pthread_t initMonitorThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initMonitor, NULL) != 0)  
		LOG(ERROR) << "Failed to create a request handler thread.\n";
	return tid;

}
void *initMonitor(void* args){
#ifdef TRACK_DEBUG
	std::cout << "[initMonitor] started monitor thread" << std::endl;
#endif
	if(ServerState.isTrackTrpt()){
		gScheduler.initPerModelTrptEWMA(&ServerState, RATE_INTERVAL,gScheduler.getBeaconInterval());
#ifdef TRACK_DEBUG
		std::cout << "[initMonitor] TRACK_TRPT turned on " << std::endl;
#endif

	}
	for(std::vector<std::string>::iterator it = gScheduler.getNetNames()->begin();  it != gScheduler.getNetNames()->end(); it++){
		if(ServerState.isTrackInterval()){
			ServerState.setPerModelCnt(*it,0);
			gScheduler.initReqRate(*it,RATE_INTERVAL,gScheduler.getBeaconInterval(), gScheduler.getTaskTgtRate(*it));
		}

#ifdef TRACK_DEBUG
		std::cout << "[initMonitor] started to monitor INTERVAL : " << *it << std::endl; 
#endif 
	}
	while(true){
		usleep(RATE_INTERVAL*1000);
		for(std::vector<std::string>::iterator it = gScheduler.getNetNames()->begin();  it != gScheduler.getNetNames()->end(); it++){
			if(ServerState.isTrackInterval()){
				gScheduler.updateAvgReqRate(*it, ServerState.getPerModelCnt(*it));
				ServerState.setPerModelCnt(*it, 0);
			}
			if(ServerState.isTrackTrpt()){
				float trpt = float(ServerState.getPerModelFinCnt(*it))/ (float(RATE_INTERVAL)/1000);
				gScheduler.addTrpt(*it,trpt);
				ServerState.setPerModelFinCnt(*it,0);
			}
		}
	}
	return (void*)0;
}
void *initGPUMonitor(void* args){
	int model_id = intptr_t(args);
	FILE *fp = fopen("gpu_log.txt","w");
	const int GPU_MONITOR_INTERVAL=1;
#ifdef DEBUG
	std::cout << "[initGPUMonitor] started monitor thread for GPU with interval: " << GPU_MONITOR_INTERVAL <<"s" \
		<<std::endl;
#endif
	while(true){
		usleep(GPU_MONITOR_INTERVAL*1000*1000);
		float gpu_part=0;
		for(int i =0; i < ServerState.getNGPUs(); i++){

			for(auto pPInfo : *ServerState.getDevMPSInfo(i))
			{
				if(ServerState.getProxyNetListSize(pPInfo) !=0) gpu_part+=pPInfo->cap;
			}

		}
		gpu_part = gpu_part /  100;
		fprintf(fp,"%s,%lf\n",timeStamp(),gpu_part);
		fflush(fp);
	}

	return (void*)0;
}

pthread_t initGPUMonitorThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initGPUMonitor, (void *)0) != 0)  
		LOG(ERROR) << "Failed to create a request handler thread.\n";
	return tid;
}


void* initUtilMonitor(void* args){
	FILE *fp = fopen("util_log.txt","w");
	const int UTIL_MONITOR_INTERVAL=1;
#ifdef DEBUG
	std::cout << "[initUtilMonitor] started monitor thread for proxys with interval: " << UTIL_MONITOR_INTERVAL <<"s" <<std::endl;

#endif
	std::vector<float> per_task_util;
	while(true){
		int num_gpu=0;
		usleep(UTIL_MONITOR_INTERVAL*1000*1000);
		for(int i =0; i < ServerState.getNGPUs(); i++){
			for(auto pPInfo : *ServerState.getDevMPSInfo(i))
			{
				if(pPInfo->isSchedulable){
					num_gpu++;
					break;
				}
			}

		}
		fprintf(fp,"%s,%d\n",timeStamp(),num_gpu);
		fflush(fp);
	}
	return (void*)0;
}

pthread_t initUtilMonitorThread(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024); // set memory size, may need to adjust in the future, for now set it to 1MB
	pthread_t tid;
	if (pthread_create(&tid, &attr, initUtilMonitor,  (void*)NULL) != 0)  
		LOG(ERROR) << "Failed to create a request handler thread.\n";
	return tid;
}

