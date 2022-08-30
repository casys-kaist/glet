#include "img_utils.h"
#include "torch_utils.h"
#include "common_utils.h"
#include <string.h>

// Resize an image to a given size to
cv::Mat __resize_to_a_size(cv::Mat &image, int new_height, int new_width){
  // get original image size
  int org_image_height = image.rows;
  int org_image_width = image.cols;
  // get image area and resized image area
  float img_area = float(org_image_height * org_image_width);
  float new_area = float(new_height * new_width);
  // resize
  cv::Mat image_scaled;
  cv::Size scale(new_width, new_height);
  if (new_area >= img_area) {
    cv::resize(image, image_scaled, scale, 0, 0, cv::INTER_LANCZOS4);
  } else {
    cv::resize(image, image_scaled, scale, 0, 0, cv::INTER_AREA);
  }
  return image_scaled;

}

// Normalize an image by subtracting mean and dividing by standard deviation
cv::Mat __normalize_mean_std(cv::Mat &image, std::vector<double> mean, std::vector<double> std){
  
}

// Preprocess
cv::Mat preprocess(cv::Mat *image, int new_height, int new_width) 
{
  std::vector<double> mean = {0.485, 0.456, 0.406};
  std::vector<double> std = {0.229, 0.224, 0.225};
  // Clone
  cv::Mat image_proc = image->clone();
  // Convert from BGR to RGB
  cv::cvtColor(image_proc, image_proc, cv::COLOR_BGR2RGB);
  // Resize image
  image_proc = __resize_to_a_size(image_proc, new_height, new_width);
  // Convert image to float
  image_proc.convertTo(image_proc, CV_32FC3);
  // 3. Normalize to [0, 1]
  image_proc = image_proc / 255.0;
  // 4. Subtract mean and divide by std
  image_proc = __normalize_mean_std(image_proc, mean, std);
  return image_proc;

}
torch::Tensor serialPreprocess(std::vector<cv::Mat> input, int imagenet_row, int imagenet_col){
  size_t input_size = input.size();
  std::vector<cv::Mat> resized;
  for (int j = 0; j < input_size; j++)
  {
    resized.push_back(preprocess(&input[j], imagenet_row, imagenet_col));
        }
        torch::Tensor ret = convert_images_to_tensor(resized);
        return ret;
}

