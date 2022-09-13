#ifndef _BACKEND_PROXY_CTRL_H_
#define _BACKEND_PROXY_CTRL_H_

#include "gpu_proxy.h"
#include "proxy_ctrl.h"
#include "backend_delegate.h"
#include <map>
#include <mutex>



// this class must be called instead of ProxyCtrl, when running in backend
class BackendProxyCtrl{
	public:
		BackendProxyCtrl();
		~BackendProxyCtrl();
		// set emulation mode 
		void setEmulatedMode(bool flag);

		int addBackendDelegate(int node_id, BackendDelegate *backend_delegate);
		// the following methods must be implemented in allign with functions defined in backend_control.cpp
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
		int markProxy(proxy_info *pPInfo, proxy_state state);

		// get remote proxy state
		proxy_state getProxyState(proxy_info *pPInfo);

		// get total memory of remote node
		int getTotalMem(proxy_info* pPInfo);

		// ONLY USED IN FRONTEND
		// get used memory of remmote gpu which proxy_info is pointing to
		int getUsedMem(proxy_info *pPInfo);



	private:
		int lookupFrontDevIDtoBackDevID(proxy_info* pPInfo);
		std::map<int,BackendDelegate *> mMapBackendDelegate;
		// stores how many devices are in each node
		std::map<int,int> mPerBackendNodeDevNum;
		std::mutex _mtx;
		bool _emul_flag=false;

}; //BackendProxyCtrl
#else

#endif
