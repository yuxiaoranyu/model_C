#pragma once
#include <iostream>
#include <vector>
#include <unistd.h>
#include <memory>
#include <assert.h>
#include <chrono>
#include <numeric>
#include <algorithm>

namespace ias{
class Range{
public:
    //向量范围查找
    std::vector<std::vector<size_t>> rangeFind(const std::vector<double>&find_data, const double (&range)[2], const std::vector<size_t>&sort_idx = {});
    
};
}//namespace ias
