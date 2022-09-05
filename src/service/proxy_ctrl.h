#ifndef __PROXY_CTRL_H__
#define __PROXY_CTRL_H__

#include "gpu_proxy.h"
#include "shmem_ctrl.h"
#include "gpu_utils.h"

#include <string>
#include <mutex>

#define CLOSE_SOCKET 25301 // RANDOM prime number used for signaling closing socket
#define LOAD_MODEL 2251 // RANDOM prime number used for signaling closing socket
#define UNLOAD_MODEL 8297 // RANDOM prime number used for signaling closing socket


class ProxyCtrl{
    public:
    ProxyCtrl(bool clear_memory);
    ~ProxyCtrl();
    
    // setups proxy for (instead of constructor)
    
    // starts proxy, called from server
    int startProxy(proxy_info* pPInfo, int ngpus, int nparts);

    // ends proxy, called from server
    int endProxy(proxy_info* pPInfo);

    // wait until the proxy becomes state(USE IT AT YOUR OWN RISK)
    int waitProxy(proxy_info* pPInfo, proxy_state state);

    // connects proxy_info to corresponding proxy
    int connectProxy(proxy_info* pPInfo);

    // disconnects proxy_info from corresponding proxy 
    int disconnectProxy(proxy_info* pPInfo);

    // loads model with 'id' 
    int makeProxyLoadModel(proxy_info* pPInfo, int model_id, int batch_size);

    // unloads model with 'id' 
    int makeProxyUnloadModel(proxy_info* pPInfo, int model_id);

    // marks proxy as 'state', called from proxy
    int markProxy(proxy_info* pPInfo, proxy_state state);
  
    proxy_state getProxyState(proxy_info* pPInfo);

    // get total memory
    int getTotalMem(proxy_info *pPInfo);

    // get used memory 
    int getUsedMem(proxy_info *pPInfo);

    // setup proxy related string constants
    void setProxyConstants(std::string proxy_full_dir, std::string proxy_sh);

    private:
    int _MAX_PROXY_PER_GPU;
    //initiated during constuction of this object
    ShMemManager *_shmanager;
    // mutex used when changing directories and bootring proxys
    std::mutex _chdir_mtx;
    std::condition_variable _chdir_cv;
    char *_org_dir;
    GPUUtil *mGPUUtil;
    std::string _PROXY_FULL_DIR;
    std::string _PROXY_SH;
}; // class ProxyCtrl



#else
#endif
