#ifndef _DEVICE_SPEC_H__
#define _DEVICE_SPEC_H__

#include <string>

class DeviceSpec{
	public:
		DeviceSpec(std::string name, int total_mem, std::string type);
		~DeviceSpec();
		int getTotalMem();
		std::string getName();
		std::string getType();
	private:
		int _total_mem;
		std::string _name;
		std::string _type;

};
#else
#endif
