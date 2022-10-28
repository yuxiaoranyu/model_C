/*
 * @Description:
 * @Version: 2.0
 * @Autor: yanglikun
 * @Date: 2022-03-24 09:35:40
 * @LastEditors: error: git config user.name && git config user.email & please set dead value or install git
 * @LastEditTime: 2022-10-14 17:14:55
 */
#pragma once
#include <iostream>
#include <vector>
#include <codecvt>
#include <fstream>
// #include <c10/cuda/CUDAStream.h>
// #include <ATen/cuda/CUDAEvent.h>
#include <utility>
#include <opencv2/opencv.hpp>
namespace ias
{
    namespace ml
    {
        enum FRAME_TYPE
        {
            FRAME_NONE = 0,
            FRAME_LIBTORCH = 1,
            FRAME_ONNX = 2
        };
        enum DEVICE_TYPE
        {
            DEVICE_NONE = 0,
            DEVICE_CPU = 1,
            DEVICE_GPU = 2
        };
        enum PRECISION_TYPE
        {
            USE_INT8 = 0,
            USE_FP16 = 1,
            USE_FP32
        };
    } // namespace
} // namespace