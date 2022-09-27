#ifndef _BASE_DELEGATE_H__

#define _BASE_DELEGATE_H__
#include <string>
#include <vector>

class BaseDelegate{
	public:
		BaseDelegate();
		~BaseDelegate();
		virtual int Connect();
		virtual int Disconnect();
	protected:
		std::string rpc_port;
};

#else
#endif
