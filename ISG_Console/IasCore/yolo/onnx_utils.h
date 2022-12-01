#ifndef UTILS_H
#define UTILS_H

#include <fstream>
#include <parallel/algorithm>
#include "opencv2/opencv.hpp"
#include "cuda_runtime.h"
#include <omp.h>

std::vector<float> letterbox(const cv::Mat &image, cv::Mat &outImage,
                             const cv::Size &newShape = cv::Size(640, 640),
                             const cv::Scalar &color = cv::Scalar(114, 114, 114),
                             bool auto_ = false,
                             bool scaleFill = false,
                             bool scaleUp = true,
                             int stride = 32);
void scaleCoords(const cv::Size &imageShape, cv::Rect &box, const cv::Size &imageOriginalShape);

#endif // UTILS_H
