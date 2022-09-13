#ifndef _BACKEND_DELEGATE_H__

#define _BACKEND_DELEGATE_H__

#include <vector>
#include <string>
#include "gpu_proxy.h"
#include "torch_utils.h"
#include "proxy_ctrl.h"
#include "device_spec.h"

class BackendDelegate {
	public: 
		BackendDelegate(int node_id, std::vector<int> &sizes, std::string backend_data_addr, DeviceSpec *ds);
		~BackendDelegate();
		//set flag for emulating mode
		void setEmulatingMode(bool flag);
		// connect to backend data address and return connected socket
		int connectNewDataChannel();
		// disconnect from backend data address
		int disconnectDataChannel(int data_socket_fd);
		// connect to backend control address
		int connectCtrlChannel(std::string backend_control_addr);
		// disconnect from backend control address
		int disconnectCtrlChannel();
		int getUsedMem(int gpu_id);
		int getControlFD();
		DeviceSpec* getDeviceSpec();   
		int getNGPUs();
		void setNGPUs(int ngpu);
		int updateNGPUS();
		std::string getType();
		std::vector<int> getSizes();

	private:
		// gpu-let related info
		int _node_id; // initated in construction
		int _nGPUs;// initated in updateNGPUs
		std::vector<int> _sizes; // initated in construction
		int _control_socket_fd=-1; // initated in connectCtrlChannel
		std::string _backend_data_addr;
		DeviceSpec* _pDevSpec;
		bool _emul_flag=false;

};


#else
#endif

