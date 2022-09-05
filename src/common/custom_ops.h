#ifndef _CUSTOM_OPS_H__
#define _CUSTOM_OPS_H__
#include <torch/serialize/tensor.h>  

#include <string>
namespace custom{
        torch::Tensor getBoxforTraffic(torch::Tensor conf, torch::Tensor location, torch::Tensor input);
}
#endif

