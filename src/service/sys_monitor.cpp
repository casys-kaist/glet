#include "sys_monitor.h"
#include <assert.h>

SysMonitor::SysMonitor()
{
    // for performance issues we explicitely set how much the queue should hold
    cmpQ = new moodycamel::ConcurrentQueue<std::shared_ptr<request>>(1000);
    //nEmulBackendNodes = 0;
    // init tracking flags
	_TRACK_INTERVAL=true;
	_TRACK_TRPT=false;
}

SysMonitor::~SysMonitor()
{
}

void SysMonitor::setupProxy(proxy_info* pPInfo){
    PerProxyBatchList[pPInfo]=new std::deque<std::shared_ptr<TaskSpec>>();
}

std::map<std::string, std::queue<std::shared_ptr<request>>>* SysMonitor::getReqListHashTable(){
    return &ReqListHashTable;
}

std::queue<std::shared_ptr<request>>* SysMonitor::getRequestQueueofNet(std::string str_req_name){
    return &ReqListHashTable[str_req_name];
}

bool SysMonitor::isQueueforNet(std::string str_req_name){
    return ReqListHashTable.find(str_req_name) != ReqListHashTable.end();
}

void SysMonitor::addNewQueueforNet(std::string str_req_name){
    if(!isQueueforNet(str_req_name)){
        std::queue<std::shared_ptr<request>> *rinput = new std::queue<std::shared_ptr<request>>;
        ReqListHashTable[str_req_name]=*rinput;
    }
    _netNames.push_back(str_req_name);
}
// returns names of network that has a queue
std::vector<std::string>* SysMonitor::getVectorOfNetNames(){
    return &_netNames;
}

uint32_t SysMonitor::getLenofReqQueue(std::string str_req_name){
    if(!isQueueforNet(str_req_name)) return 0;
    return ReqListHashTable[str_req_name].size();
}


moodycamel::ConcurrentQueue<std::shared_ptr<request>>* SysMonitor::getCompQueue(){
    return cmpQ;
}


/*
std::map<proxy_info *, std::deque<std::shared_ptr<TaskSpec>> *>* SysMonitor::getPerProxyBatchList(){
    return &PerProxyBatchList;
}
*/

std::deque<std::shared_ptr<TaskSpec>>* SysMonitor::getBatchQueueofProxy(proxy_info* pPInfo){
   return PerProxyBatchList[pPInfo];

}

void SysMonitor::insertToBatchQueueofProxy(proxy_info* pPInfo, std::shared_ptr<TaskSpec> task_spec){
    return PerProxyBatchList[pPInfo]->push_back(task_spec);

}

uint32_t SysMonitor::getSizeofBatchQueueofProxy(proxy_info *pPInfo){
    return PerProxyBatchList[pPInfo]->size();
}

void SysMonitor::setNGPUs(int ngpu){
    assert(ngpu >=0);
    nGPUs=ngpu;
}

int SysMonitor::getNGPUs(){
    return nGPUs;
}

std::vector<std::pair<std::string, int>>* SysMonitor::getProxyNetList(proxy_info* pPInfo){
    return  &PerProxyNetList[pPInfo];
}
void SysMonitor::insertNetToProxyNetList(proxy_info* pPInfo, std::pair<std::string, int>& pair){
    PerProxyNetList[pPInfo].push_back(pair);
}

uint32_t SysMonitor::getProxyNetListSize(proxy_info* pPInfo){
    return PerProxyNetList[pPInfo].size();
}

bool SysMonitor::isProxyNetListEmpty(proxy_info* pPInfo){
    return PerProxyNetList[pPInfo].empty();
}

std::vector<proxy_info*>* SysMonitor::getDevMPSInfo(int dev_id){
    return &PerDevMPSInfo[dev_id];
}

void SysMonitor::insertMPSInfoToDev(int dev_id, proxy_info* pPInfo){
    PerDevMPSInfo[dev_id].push_back(pPInfo);
}

int SysMonitor::getIDfromModel(std::string net){
    return PerModeltoIDMapping[net];
}

void SysMonitor::setIDforModel(std::string net, int id){
    PerModeltoIDMapping[net]=id;
}

int SysMonitor::getPerModelCnt(std::string model){
    return PerModelCnt[model];
}
void SysMonitor::setPerModelCnt(std::string model, int cnt){
    PerModelCnt[model]=cnt;
}

void SysMonitor::incPerModelCnt(std::string model){
    PerModelCnt[model]++;
}

// used for tracking per model trpt
int SysMonitor::getPerModelFinCnt(std::string model){
    return PerModelFinCnt[model];
}
void SysMonitor::setPerModelFinCnt(std::string model, int cnt){
    PerModelFinCnt[model]=cnt;
}
void SysMonitor::incPerModelFinCnt(std::string model){
    PerModelFinCnt[model]++;
}

std::map<int, int>* SysMonitor::getLocalIDToHostIDTable(){
    return &FrontDevIDToHostDevID;
}

void SysMonitor::setNumProxyPerGPU(int n_proxy_per_gpu){
    nProxyPerGPU = n_proxy_per_gpu;
}
int SysMonitor::getNumProxyPerGPU(){
    return nProxyPerGPU;
}

void SysMonitor::setFlagTrackInterval(bool val){
    _TRACK_INTERVAL=val;
}
bool SysMonitor::isTrackInterval(){
    return _TRACK_INTERVAL;
}

void SysMonitor::setFlagTrackTrpt(bool val){
    _TRACK_TRPT=val;
}
bool SysMonitor::isTrackTrpt(){
    return _TRACK_TRPT;
}
void SysMonitor::setSysFlush(bool val){
    _SYS_FLUSH=val;
}
bool SysMonitor::isSysFlush(){
    return _SYS_FLUSH;
}

std::vector<AppSpec> *SysMonitor::getAppSpecVec(){
    return &AppSpecVec;
}

proxy_info* SysMonitor::findProxy(int dev_id, int partition_num)
{
    if (FrontDevIDToHostDevID.empty())
    {
        assert(dev_id < nGPUs);
    }
    else
    {
        int first_idx = FrontDevIDToHostDevID[0];
        assert(dev_id - first_idx < nGPUs);
    }
#ifdef BACKEND_DEBUG
		std::cout << __func__ << ": finding dev_id: " << dev_id << ", partition_num: " << partition_num
			<< std::endl;
#endif
		proxy_info* ret = NULL;
		for(auto pPInfo : PerDevMPSInfo[dev_id]){
			if(pPInfo->partition_num == partition_num)  
			{   
				ret=pPInfo;
				break;
			}   
		}   
		assert(ret != NULL);
		return ret;
}

proxy_info* SysMonitor::findProxy(int dev_id, int resource_pntg, int dedup_num)
{
    if (FrontDevIDToHostDevID.empty())
    {
        assert(dev_id < nGPUs);
    }
    else
    {
        int first_idx = FrontDevIDToHostDevID[0];
        assert(dev_id - first_idx < nGPUs);
    }
    proxy_info *ret = NULL;
    for (auto pPInfo : PerDevMPSInfo[dev_id])
    {
        if (pPInfo->dev_id == dev_id && pPInfo->cap == resource_pntg && pPInfo->dedup_num == dedup_num)
        {
            ret = pPInfo;
            break;
        }
    }
    assert(ret != NULL);
    return ret;
}
