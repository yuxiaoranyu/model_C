/*
 * @Author: error: git config user.name && git config user.email & please set dead value or install git
 * @Date: 2022-09-27 10:44:27
 * @LastEditors: error: git config user.name && git config user.email & please set dead value or install git
 * @LastEditTime: 2022-09-27 10:55:27
 * @FilePath: /code/algorithm/core/module/patchcore/post_process.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <opencv2/opencv.hpp>
#include "corepatchcore.h"

RoiInfo post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg, InputInfo &Input_info);
RoiInfo tiaobao_post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg, InputInfo &Input_info);
bool isInPolygon(const cv::Point &point, const std::vector<cv::Point> &points);
