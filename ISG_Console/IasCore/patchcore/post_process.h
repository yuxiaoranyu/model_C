/*
 * @Author: yuxiaoranyu 2312067800@qq.com
 * @Date: 2022-10-26 15:49:52
 * @LastEditors: yuxiaoranyu 2312067800@qq.com
 * @LastEditTime: 2022-11-14 16:44:27
 * @FilePath: /ISG_Console/IasCore/patchcore/post_process.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <opencv2/opencv.hpp>
#include "corepatchcore.h"

RoiInfo post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg, InputInfo &Input_info);
RoiInfo tiaobao_post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg, InputInfo &Input_info);
bool isInPolygon(const cv::Point &point, const std::vector<cv::Point> &points);
