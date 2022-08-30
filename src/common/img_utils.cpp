#include "img_utils.h"
#include "torch_utils.h"
#include "common_utils.h"
#include <string.h>

#include <mutex>
#include <condition_variable>

// 1. Preprocess
cv::Mat preprocess(cv::Mat *image, int new_height, int new_width) 
{
}
torch::Tensor serialPreprocess(std::vector<cv::Mat> input, int imagenet_row, int imagenet_col){
        size_t input_size = input.size();
        std::vector<cv::Mat> resized;
        for(int j =0; j< input_size; j++){
                resized.push_back(preprocess(&input[j], imagenet_row, imagenet_col));
        }
        torch::Tensor ret = convert_images_to_tensor(resized);
        return ret;
}

