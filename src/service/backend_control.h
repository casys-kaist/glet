#ifndef _BACKEND_CONTROL_H__
#define _BACKEND_CONTROL_H__
#include <vector>
#include <string>
#include "gpu_proxy.h"
#include <pthread.h>

typedef enum _BACKEND_CONTROL_CMD
{
	BACKEND_GET_STATE=0, 
	BACKEND_SET_STATE=1,
	BACKEND_LOAD_MODEL=2, 
	BACKEND_UNLOAD_MODEL=3,
	BACKEND_START_PROXY=4,
	BACKEND_END_PROXY=5,
	BACKEND_DISCONNECT_PROXY=6,
	BACKEND_CONNECT_PROXY=7,
	BACKEND_GET_MEM=8,
	BACKEND_GET_NGPUS=9
} BACKEND_CONTROL_CMD;

class BackendControl{
	public:
		BackendControl(int socket_fd);
		~BackendControl();
		int receiveControl(std::vector<int> &recv_data);
		int executeControl(std::vector<int> &params);
	private:
		int _socket_fd;
		int loadModelBackend(proxy_info *pPInfo, int model_id, int batch_size);
		int unloadModelBackend(proxy_info *pPInfo, int model_id);
		int getState(proxy_info *pPInfo, int &state);
		int startProxy(proxy_info *pPInfo);
		int endProxy(proxy_info *pPInfo);
		int setState(proxy_info *pPInfo, int state);
		int connectProxy(proxy_info *pPInfo);
		int disconnectProxy(proxy_info *pPInfo);
		int getUsedMemory(int gpuid);
		int getNGPUs();
};

#else
#endif
