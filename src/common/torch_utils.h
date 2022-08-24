#ifndef TORCHUTILS_H // To make sure you don't declare the function more than once by including the header multiple times.
#define TORCHUTILS_H

#include <iostream>
#include <vector>
#include <tuple>
#include <chrono>
#include <fstream>
#include <random>
#include <string>
#include <memory>
#include <queue>

#include <torch/script.h>
#include <torch/serialize/tensor.h>
#include <torch/serialize.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

torch::Tensor convert_rawdata_to_tensor(float *rawdata, std::vector<int64_t> dims, torch::TensorOptions options); 
torch::Tensor convert_rawdata_to_tensor(long *rawdata, std::vector<int64_t> dims, torch::TensorOptions options);
torch::Tensor convert_image_to_tensor(cv::Mat *image); //convert single image to tensor
torch::Tensor convert_images_to_tensor(std::queue<cv::Mat> *images, const int BATCH_SIZE);
torch::Tensor convert_images_to_tensor(std::queue<cv::Mat> images); // used in img workloads
torch::Tensor convert_images_to_tensor(std::vector<cv::Mat> images); // used in img workloads
torch::Tensor convert_images_to_tensor(cv::Mat *img_arr, int total_len); // used in img workloads that work in parallel
torch::Tensor convert_LV_vectors_to_tensor(float*input_data, int batch_size, int nz); // used in gab
torch::Tensor concatToSingleTensor(std::vector<torch::Tensor> vecTensor); // can be used anywhere
void sendTensorFloat(int socketfd, int reqID, torch::Tensor input);
void sendTensorLong(int socketfd, int reqID, torch::Tensor input);


#endif
