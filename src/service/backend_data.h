#ifndef _BACKEND_DATA_H__
#define _BACKEND_DATA_H_
#include "app_spec.h"
#include "gpu_proxy.h"
#include <torch/script.h>
#include <torch/serialize/tensor.h>
#include <torch/serialize.h>

typedef struct _BatchedRequest
{
	int jid;
	int rid;
	int batch_size;
} BatchedRequest;

// receives batched inputs from frontend
int recvBatchedTensor(int socket_fd, proxy_info* pPInfo);

// sends batched outputs to frontend
int sendBatchedOutput(int socket_fd, proxy_info* pPInfo, BatchedRequest* pbatched_request);

#else
#endif
