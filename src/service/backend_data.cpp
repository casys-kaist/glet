#include "backend_data.h"
#include "torch_utils.h"
#include "socket.h"
#include "gpu_proxy.h"
#include "common_utils.h"
#include <map> 
#include <queue>
#include <vector>
#include <condition_variable>
#include <unistd.h>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/un.h>
#include <errno.h>   
#include <netinet/tcp.h>


std::map<int,TensorSpec*> MapIDtoSpec;
std::map<proxy_info*, std::queue<BatchedRequest*>*> MapProxytoOutputQueue;
std::map<proxy_info*, std::condition_variable*> MapProxytoOutputCV;
std::map<proxy_info*, std::mutex*> MapProxytoOutputMtx;
extern std::unordered_map<int,std::vector<uint64_t>> gInputDimMapping;
extern std::unordered_map<int,std::string> gInputTypeMapping;

#ifdef BACKEND_DEBUG
// for debugging
std::mutex dbg_mtx;
std::map<int, int> dbg_input;
int dbg_output=0;
#endif

int sendOutput(int socket_fd, std::vector<int> &dims, float* raw_output_data){
    int size = dims.size();
    write(socket_fd,(void*)&size,sizeof(int));
    int len=1;
    for(int i =0; i<dims.size(); i++){
        int dim = dims[i];
        write(socket_fd,(void*)&dim,sizeof(int));
        len *=dims[i];
    }
    int ret = socket_send(socket_fd,(char*)raw_output_data,sizeof(float)*len,false);
    // check results of sending process, and if data is sent less than specified amount
    if(ret < sizeof(float)*len){
        std::cout << "ERROR in sending data to frontned" << std::endl;
        return EXIT_FAILURE;
    }
       free(raw_output_data);
    return EXIT_SUCCESS;
}

int sendOutput(int socket_fd, std::vector<int> &dims){
    int size = dims.size();
    write(socket_fd,(void*)&size,sizeof(int));
    int len=1;
    for(int i =0; i<dims.size(); i++){
        int dim = dims[i];
        write(socket_fd,(void*)&dim,sizeof(int));
        len *=dims[i];
    }
    return EXIT_SUCCESS;
}

int receiveandsendOutput(proxy_info *pPInfo, int batch_size, int req_id, int output_socket_fd){
    int ret;
    int out_fd = pPInfo->out_fd;
    std::vector<int> dims;
    int dim;
    ret=read(out_fd, &dim, sizeof(int));
    if(ret == -1){      
        printf("[BACKEND_DATA_CHANNEL] READ ERROR! on %s \n", proxy_info_to_string(pPInfo).c_str());
        printf("ERROR: was waiting for req_id: %d \n", req_id);
        printf("SOCKET ERROR = %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    int size;
    int len=1;
    for(int j =0; j<dim;j++){
        ret=read(out_fd, &size, sizeof(int));
        len*=size;
        dims.push_back(size);
    }
    assert(dims[0] == batch_size);
    #ifndef NO_NET
	float *tmp = (float*)malloc(len*sizeof(float));
    if ((ret=socket_receive(out_fd, (char*)tmp,len*sizeof(float), false)) <=0){ 
        printf("ERROR in receiving output data\n");
        printf("ERROR: was waiting for req_id: %d \n", req_id);
        printf("SOCKET ERROR = %s\n", strerror(errno));
        return EXIT_FAILURE;
    }      
    #else
    #endif
    #ifdef BACKEND_DEBUG
    // sends output to frontend 
        dbg_mtx.lock();
        dbg_output++;
        dbg_mtx.unlock();
        std::cout << "number of outputs " << dbg_output<< std::endl;
    #endif
    #ifndef NO_NET
    ret = sendOutput(output_socket_fd, dims, tmp);
    #else
    ret = sendOutput(output_socket_fd, dims);
    #endif
    return ret;
}

// receive batched inputs from frontend, and send it to proxy
int recvBatchedTensor(int socket_fd, proxy_info* pPInfo){
    //receive request 
        // 1. receive jid
    int jid;
    const int opt_val = 0;
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof(int)))
    {
        perror("recvBatchedTensor:setsocketopt");
        return -1;
        }
 
        int ret = socket_receive(socket_fd,(char*)&jid,sizeof(int),false);
        if(setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof(int))){
            perror("recvBatchedTensor:setsocketopt");
            return -1;
        }
        if(ret <=0 ){
            std::cout << "Client closed connection on fd: " << socket_fd << std::endl;
            // send messsage to queue 
            BatchedRequest *tmp = new BatchedRequest();
            tmp->rid=-1;
            tmp->batch_size=0;
            MapProxytoOutputMtx[pPInfo]->lock();
            MapProxytoOutputQueue[pPInfo]->push(tmp);
            MapProxytoOutputMtx[pPInfo]->unlock();
            MapProxytoOutputCV[pPInfo]->notify_all();
        }
        #ifdef OVERHEAD
        uint64_t start, end;
        uint64_t n_start, n_end;
        uint64_t c_start, c_end;
        start=getCurNs();
        #endif
        #ifdef OVERHEAD
        c_start = getCurNs();
        #endif 

    // 2. receive rid
        int rid = socket_rxsize(socket_fd);
        if(setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof(int))){
            perror("recvBatchedTensor:setsocketopt");
            return -1;
        }
    // 3. receive batch size
        std::vector<int64_t> dims;
        int batch_size = socket_rxsize(socket_fd);
        if(setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof(int))){
            perror("recvBatchedTensor:setsocketopt");
            return -1;
        }
        int Datalen = batch_size;
        dims.push_back(batch_size); 
        for(auto iter = gInputDimMapping[jid].begin(); iter != gInputDimMapping[jid].end(); iter++){
            Datalen *= *iter;
            dims.push_back(*iter);
        }
    // 4. receive data and make tensor 
    #ifndef NO_NET
        torch::Tensor input_tensor;
        if(gInputTypeMapping[jid] == "FP32"){
            #ifdef BACKEND_DEBUG
            cout << "Backend: FP32" << endl;
            #endif 
            float *inData = (float *)malloc(Datalen * sizeof(float));
            #ifdef OVERHEAD
            c_end = getCurNs();
            #endif 
            #ifdef OVERHEAD
            n_start=getCurNs();
            #endif

            int rcvd = socket_receive(socket_fd, (char *)inData,Datalen * sizeof(float) ,false);
         if(setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof(int))){
            perror("recvBatchedTensor:setsocketopt");
            return -1;
        }
    
            #ifdef OVERHEAD
            n_end = getCurNs();
            #endif
            if(rcvd !=Datalen * sizeof(float) ){
                std::cout << __func__ << ": ERROR in receiving data" << std::endl;
                std::cout << "expected: " << Datalen * sizeof(float) << " but received: " << rcvd << std::endl;
                return EXIT_FAILURE;
            } 
            auto options = torch::TensorOptions().dtype(torch::kFloat32).requires_grad(false);
            input_tensor = convert_rawdata_to_tensor(inData, dims, options);
        }
        else if(gInputTypeMapping[jid] == "INT64"){
            #ifdef BACKEND_DEBUG
            cout << "Backend: INT64" << endl;
            #endif 

            int64_t *inData = (int64_t *)malloc(Datalen * sizeof(int64_t));

            #ifdef OVERHEAD
            c_end = getCurNs();
            #endif 
            #ifdef OVERHEAD
            n_start=getCurNs();
            #endif

            int rcvd = socket_receive(socket_fd, (char *)inData,Datalen * sizeof(int64_t) ,false);
          if(setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof(int))){
            perror("recvBatchedTensor:setsocketopt");
            return -1;
          }
    
            #ifdef OVERHEAD
            n_end = getCurNs();
            #endif
            if(rcvd !=Datalen * sizeof(int64_t) ){
                std::cout << __func__ << ": ERROR in receiving data" << std::endl;
                std::cout << "expected: " << Datalen * sizeof(int64_t) << " but received: " << rcvd << std::endl;
                return EXIT_FAILURE;
            } 
            auto options = torch::TensorOptions().dtype(torch::kInt64).requires_grad(false);
            input_tensor = convert_rawdata_to_tensor(inData, dims, options);
 
        }
    #else
    #endif
        // 5. send data to proxy(defined in gpu_proxy.cpp)
        // send output to proxy
        pPInfo->sendMtx->lock();
        #ifndef NO_NET
        int retval = sendRequest(pPInfo->in_fd, rid, jid,input_tensor);
        #else
        int retval = sendRequest(pPInfo->in_fd, rid, jid, dims);
        #endif
        pPInfo->sendMtx->unlock();

        if(retval){
            std::cout << "sendRequest failed for rid: " << rid << " jid: " << jid << std::endl;
        }
        #ifdef BACKEND_DEBUG
        dbg_mtx.lock();
        if(dbg_input.find(jid) == dbg_input.end()){
            dbg_input[jid]=1;
        }
        else dbg_input[jid]++;
        dbg_mtx.unlock();
        int sum=0;
        for(auto pair : dbg_input){
            std::cout << "model: " << pair.first << " number of inputs: " << pair.second<< std::endl;
            sum+=pair.second;
        }
        std::cout << "sum of inputs: " << sum<< std::endl;
        #endif
        BatchedRequest *tmp = new BatchedRequest();
        tmp->rid=rid;
        tmp->batch_size=batch_size;
        MapProxytoOutputMtx[pPInfo]->lock();
        MapProxytoOutputQueue[pPInfo]->push(tmp);
        MapProxytoOutputMtx[pPInfo]->unlock();
        MapProxytoOutputCV[pPInfo]->notify_all();
        #ifdef OVERHEAD
        end= getCurNs();
        std::cout <<"BATCH_SIZE: " << batch_size << " TOTAL_OVERHEAD: " << (end-start)/1000000 
        << " NETWORK1:  " << (n_end-n_start) / 1000000 << " NETWORK2: " << (c_end - c_start)/1000000 
        << std::endl;
        #endif

        return EXIT_SUCCESS;
}

// receive output from proxy and sends batched outputs to frontend
int sendBatchedOutput(int socket_fd, proxy_info* pPInfo, BatchedRequest* pbr){
    // check queue , cv , and pop the request 

    if(receiveandsendOutput(pPInfo,pbr->batch_size, pbr->rid, socket_fd)){
        free(pbr);
        return EXIT_FAILURE;
    }
    free(pbr);
    return EXIT_SUCCESS;
}


