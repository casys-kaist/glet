#include <sys/shm.h>
#include <sys/ipc.h>
#include <cassert>
#include <iostream>
#include "shmem_ctrl.h"

ShMemManager::ShMemManager(){
}

ShMemManager::~ShMemManager(){
	clearShMem();
}

int ShMemManager::initShMem(bool clear_state){
	int shmid;
	const int SHM_INFO_COUNT=_MAX_GPU * _MAX_PROXY_PER_GPU;
	void *shared_memory = (void *)0;
	shmid = shmget((key_t)3836, sizeof(SHM_PROXY_INFOS)*SHM_INFO_COUNT, 0666|IPC_CREAT);
	if(shmid == -1){
		perror("shmget failed : ");
		return EXIT_FAILURE;
	}
	_shmid = shmid;
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1)
	{
		perror("shmat failed : ");
		return EXIT_FAILURE;
	}
	_shm_info = (SHM_PROXY_INFOS*) shared_memory;
    if(clear_state){
        for(int i=0; i < SHM_INFO_COUNT; i++){
    	    _shm_info[i].state=0;
        }
    }
	return EXIT_SUCCESS;
}

int ShMemManager::clearShMem(){
	if ( -1 == shmctl( _shmid, IPC_RMID, 0))
	{
		perror("shmctl failed: ");
		return EXIT_FAILURE;
	}
	else printf("shared memory successfully deleted! \n");
	return EXIT_SUCCESS;
}

int ShMemManager::setShMem(unsigned int value, int devid, int part_idx){
	assert(part_idx < _MAX_PROXY_PER_GPU);
	int part_index=part_idx;
	int shmem_index= devid * _MAX_PROXY_PER_GPU + part_index;
	assert(shmem_index < _MAX_PROXY_PER_GPU * _MAX_GPU);
	_shm_info[shmem_index].state=value;
	return EXIT_SUCCESS;
}

int ShMemManager::getShMem(unsigned int &value, int devid, int part_idx){
	assert(part_idx < _MAX_PROXY_PER_GPU);
	int part_index=part_idx;

	int shmem_index= devid * _MAX_PROXY_PER_GPU + part_index;
	assert(shmem_index < _MAX_PROXY_PER_GPU * _MAX_GPU);
	value=_shm_info[shmem_index].state;
	return EXIT_SUCCESS;
}

int ShMemManager::setProxyState(proxy_state state,int devid, int part_idx){
	return setShMem(state,devid,part_idx);
}

int ShMemManager::getProxyState(proxy_state &output_state, int devid, int part_idx){
    unsigned int ret_value;
	getShMem(ret_value,devid,part_idx);   
    switch(ret_value){
		case COLD:
			output_state=COLD;
			break;
		case BOOTING:
			output_state=BOOTING;
			break;
		case EXITING:
			output_state=EXITING;
			break;
		case RUNNING:
			output_state=RUNNING;
			break;
        case FLUSHED:
            output_state=FLUSHED;
			break;
        case LOADING:
            output_state=LOADING;
			break;
		default:
			std::cout<<"CHECK getProxyState" << std::endl;
			std::cout<<"CHECL ret_val:" << ret_value << std::endl; 
			return EXIT_FAILURE;
	}; 
	return EXIT_SUCCESS;
}

