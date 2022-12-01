#include"range.h"

namespace ias
{

std::vector<std::vector<size_t>> Range::rangeFind(const std::vector<double>& find_data, const double (&range)[2], const std::vector<size_t>& sort_idx)  //*$*
{
    std::vector<std::vector<size_t>> data;
    std::vector<size_t> idx(find_data.size()) ;

    if (find_data.size() == sort_idx.size()) {
        idx = sort_idx;
        // for (size_t i = 0; i < idx.size(); i++)
        // {
        //     std::cout<<"sort_idx"<<idx[i]<<std::endl;
        // }



    } else {
        std::iota(idx.begin(), idx.end(), 0);
        // for (size_t i = 0; i < idx.size(); i++)
        // {
        //     std::cout<<"find_data"<<idx[i]<<std::endl;
        // }
    }

    for (size_t i = 0; i < idx.size(); i++) {
        for (size_t j = i; j < idx.size(); j++) {
            //范围计算
            double comparedata = abs(find_data[idx[i]] -  find_data[idx[j]]);
            if (range[0] <= comparedata && range[1] >= comparedata) {
                std::vector<size_t> buff = {idx[i], idx[j]};
                data.push_back(buff);
            }
        }
    }
    return data;
}

}// namespace ias/core

