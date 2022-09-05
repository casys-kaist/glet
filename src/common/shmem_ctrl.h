#ifndef SHMEM_CNTRL_H__
#define SHMEM_CNTRL_H__
#include <string>


enum proxy_state {COLD=0, BOOTING, RUNNING, LOADING, EXITING, FLUSHED};

typedef struct _shm_proxy_info{
	unsigned int state;
}SHM_PROXY_INFOS;

class ShMemManager{
	public:
		ShMemManager();
		~ShMemManager();
		int initShMem(bool clear_state);
		int clearShMem();
		int setProxyState(proxy_state state, int devid, int part_idx);
		int getProxyState(proxy_state &output_state,int devid, int part_idx);
	private:
		int setShMem(unsigned int value, int devid, int part_idx);
		int getShMem(unsigned int &value, int devid, int part_idx);
		int _MAX_GPU=4;
		int _MAX_PROXY_PER_GPU=7;
		int _shmid;
		SHM_PROXY_INFOS* _shm_info;
}; // class ShMemManager

#else
#endif 
