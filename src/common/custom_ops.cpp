#include "custom_ops.h"
#include <stdio.h>
#include <torch/torch.h>

namespace custom{
    // custom operation for emulating box cropping for traffic application. 
    torch::Tensor getBoxforTraffic(torch::Tensor conf, torch::Tensor location, torch::Tensor input){
	    torch::Tensor ret;
        const int IMAGE_SIZE=224;
        std::vector<torch::Tensor> box_tensors;
        int batchsize = conf.size(0); // get batch size
        //for each input in batch
        for(int i=0; i< batchsize; i++){
            //1. get ith input from batch                   
            torch::Tensor one_input;
            one_input=input.narrow(0,i,1);
            //2. softmax confidence
            torch::Tensor t1=conf.softmax(2);
            //3. get argmax value(for box) of each max value(for class)
            torch::Tensor t2 = std::get<0>(t1.max(2));
            torch::Tensor t3 = t2.argmax(1);
            //4. we have to get location of the box, however for simplicity's sake we just resize input
            box_tensors.push_back(one_input.resize_({1,3,IMAGE_SIZE,IMAGE_SIZE}));
        }
        ret =   torch::cat(box_tensors, 0);
        return ret;

    }
}

