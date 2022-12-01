
#pragma once
#include <opencv2/opencv.hpp>

namespace ias{
namespace feature{
    
class ShapeMatching{
protected:
    unsigned int matching_num_ ;
    int matching_score_;
public:  
    ShapeMatching(unsigned int matchingnum ,int matchingscore):matching_num_(matchingnum),matching_score_(matchingscore){
    }
    ShapeMatching(double matchingnum ,double matchingscore):matching_num_(static_cast<unsigned int>(matchingnum)),matching_score_(static_cast<int>(matchingscore)){
    }
    unsigned int getMatchingNum_(void){
        return matching_num_;
    }
    int getMatchingScore_(void){
        return matching_score_;
    }
    void setMatchingScore_(int score){
        this->matching_score_ = score;
    }
    void setMatchingNum_(unsigned int num){
        this->matching_num_ = num;
    }
    
};

}}// namespace ias/core


namespace ias
{
    namespace feature
    {
        const bool ONTHRESHOLD = true;
        const bool OFFTHRESHOLD = false;

/*  continuous  两值化的连续分数 */
        const unsigned int HOUGH_SCORE = 1;   
/*  positive => P , negative => N ; 默认 _SCORE => CV_8U Sobel ;PN或NP 从左到右: 顺序结构 */
        const unsigned int _SCORE = 2;
        const unsigned int P_SCORE= 3;
        const unsigned int N_SCORE= 4;
        const unsigned int PN_SCORE= 5;
        const unsigned int NP_SCORE= 6;

        
/*  获得线的方法 */
        const unsigned int LINE_METHOD_1 = 1 ;
        const unsigned int LINE_METHOD_2 = 2 ;
        
    }// namespace shape

} // namespace name


