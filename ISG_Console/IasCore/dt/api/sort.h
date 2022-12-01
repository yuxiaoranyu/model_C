#pragma once
#include <iostream>
#include <vector>
#include <unistd.h>
#include <memory>
#include <assert.h>
#include <chrono>
#include <numeric>
#include <algorithm>

//Vector带索引排序
namespace ias{
class Sort{
public:
    std::vector<size_t> vectorSort(std::vector<int> target,std::string method); 
    std::vector<size_t> vectorSort(std::vector<double> target,std::string method); 
};
}
