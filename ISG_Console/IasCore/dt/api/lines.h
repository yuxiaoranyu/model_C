#pragma once

#include <opencv2/opencv.hpp>
#include "range.h"
#include "sort.h"
#include "ShapeMatching.h"

namespace ias
{
    namespace feature
    {
        class Line : public ShapeMatching
        {
        public:
            struct LinesData
            {
                double a, b, x0, y0;
                float rho, theta;
                cv::Point pt1, pt2;
                double score, angle_diff;
                // LinesData_(): a(),b(),x0(),y0(),rho(),theta(),pt1(),pt2(){}
                bool operator==(const LinesData &rhs);
            };

            Line(unsigned int matching_num, int matching_score, const unsigned int scoring_rules, const unsigned int method, const double (&angle)[2], const double length);
            std::vector<Line::LinesData> Find(const cv::Mat &frame, const cv::Mat &mask = cv::Mat());
            std::vector<Line::LinesData> getLinesDataAll();
            bool setLineLength_(unsigned int line_length);
            bool setScoringHitKernel_(const cv::Mat &kernel);
            bool setNormalization_(const bool normalization);
            unsigned setBinary_Threshold_(const unsigned int threshold);
            bool setCols_Rows_Ratio_(const double ratio[0]);
            double getPolarSystemPoint(Line::LinesData, const std::string &method, const double &pointdata);

            std::vector<LinesData> getLinesMethod1(cv::Mat &frame);
            std::vector<LinesData> getLinesMethod2(cv::Mat &frame);
            std::vector<LinesData> LineNms(const cv::Mat &frame);
            std::vector<int> getScoring(const cv::Mat &frame, bool normalization = true);

        protected:
            //取line_angle_[0]到line_angle_[1]的角度，只能line_angle_[0]<line_angle_[1]，二义 >未实现。
            unsigned int line_angle_[2];
            unsigned int line_length_;
            unsigned int scoring_rules_;
            unsigned int line_method_;
            // cols x方向 rows y方向的导数相加的比例
            double cols_rows_ratio_[2] = {0.5, 0.5};
            // Sobel后的导数进行二值化的
            unsigned int binary_threshold_ = 0;

            bool normalization_ = false;
            //默认得分的是单像素线
            cv::Mat scoring_hit_kernel_ = (cv::Mat_<int>(1, 3) << 0, 1, 0);
            std::vector<LinesData> lines_data_all_;
            bool getCrossPoint(float rho1, float theta1, float rho2, float theta2, cv::Point &cross);
        };

        class LineTool
        {
        public:
            static double getLineAngle(cv::Point2d &data1, cv::Point2d &data2); //直线的角度
            static double getLineLength(cv::Point2d &data1, cv::Point2d &data2); //直线的长度
            static cv::Point2d rotatePoint(const cv::Point2d &rotate_point, const cv::Point2d &rotate_centre, const double &rotate_angle); //点旋转
            static bool getRectCross(const std::vector<cv::Point2d> &point_data, cv::Mat &cross, const cv::Rect2d &rect_data); //直线和矩形的交叉
        };

    } // namespace feature

} // namespace ias
