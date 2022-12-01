#include"sort.h"

using namespace std;
namespace ias
{
//Vector带索引排序，使用了lambda语法
std::vector<size_t> Sort::vectorSort(std::vector<int> target, std::string method)
{
    if (method == "DOWN") {
        vector<size_t> idx(target.size());
        iota(idx.begin(), idx.end(), 0);
        sort(idx.begin(), idx.end(), [&target](size_t i1, size_t i2) {
            return target[i1] > target[i2];
        });
        return idx;
    } else if (method == "UP") {
        vector<size_t> idx(target.size());
        iota(idx.begin(), idx.end(), 0);
        sort(idx.begin(), idx.end(), [&target](size_t i1, size_t i2) {
            return target[i1] < target[i2];
        });
        return idx;
    }
    throw runtime_error("vectorSort string failure");
}

std::vector<size_t> Sort::vectorSort(std::vector<double> target, std::string method)
{

    if (method == "DOWN") {
        vector<size_t> idx(target.size());
        iota(idx.begin(), idx.end(), 0);
        sort(idx.begin(), idx.end(), [&target](size_t i1, size_t i2) {
            return target[i1] > target[i2];
        });
        return idx;

    } else if (method == "UP") {
        vector<size_t> idx(target.size());
        iota(idx.begin(), idx.end(), 0);
        sort(idx.begin(), idx.end(), [&target](size_t i1, size_t i2) {
            return target[i1] < target[i2];
        });
        return idx;
    }
    throw runtime_error("vectorSort string failure");
}

}// namespace ias
