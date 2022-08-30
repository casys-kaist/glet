#ifndef OPENCVUTILS_H // To make sure you don't declare the function more than once by including the header multiple times.
#define OPENCVUTILS_H

#include <vector>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <torch/script.h>
#include <torch/serialize/tensor.h>
#include <torch/serialize.h>


torch::Tensor serialPreprocess(std::vector<cv::Mat> input, int imagenet_row, int imagenet_col);

#else
#endif
