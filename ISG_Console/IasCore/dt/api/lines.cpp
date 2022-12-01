#include "lines.h"

using namespace cv;
using namespace std;
namespace ias
{
    namespace feature
    {
        double Line::getPolarSystemPoint(Line::LinesData find_lines_data, const std::string &method, const double &pointdata)
        {
            // y = (-find_lines_data.a/find_lines_data.b)*x+(find_lines_data.rho/find_lines_data.b);
            double data = 0;
            if (method == "y")
            {
                if (find_lines_data.theta == 0)
                {
                    data = find_lines_data.x0;
                }
                else
                {
                    data = (pointdata - (find_lines_data.rho / find_lines_data.b)) / (-find_lines_data.a / find_lines_data.b);
                }
            }
            else if (method == "x")
            {
                data = (-find_lines_data.a / find_lines_data.b) * pointdata + (find_lines_data.rho / find_lines_data.b);
            }

            return data;
        }

        std::vector<Line::LinesData> Line::getLinesMethod1(cv::Mat &frame)
        {
            vector<Vec3f> lines;
            HoughLines(frame, lines, 1, CV_PI / 180, this->line_length_, 0, 0);
            float angle;
            for (size_t i = 0; i < lines.size(); i++)
            {
                LinesData linesdata;
                linesdata.rho = lines[i][0], linesdata.theta = lines[i][1];
                linesdata.a = cos(linesdata.theta), linesdata.b = sin(linesdata.theta);
                linesdata.x0 = linesdata.a * linesdata.rho, linesdata.y0 = linesdata.b * linesdata.rho;
                linesdata.pt1.x = cvRound(linesdata.x0 + 1000 * (-linesdata.b));
                linesdata.pt1.y = cvRound(linesdata.y0 + 1000 * (linesdata.a));
                linesdata.pt2.x = cvRound(linesdata.x0 - 1000 * (-linesdata.b));
                linesdata.pt2.y = cvRound(linesdata.y0 - 1000 * (linesdata.a));
                linesdata.score = lines[i][2];
                if (linesdata.pt2.y > linesdata.pt1.y)
                {
                    swap(linesdata.pt1, linesdata.pt2);
                }

                angle = atan2(linesdata.pt2.y - linesdata.pt1.y, linesdata.pt2.x - linesdata.pt1.x) * 180 / CV_PI;
                angle = (angle > 0.0 ? angle : 180 + angle);
                if (angle > line_angle_[0] and angle < line_angle_[1])
                {
                    this->lines_data_all_.push_back(linesdata);
                }
            }
            return this->lines_data_all_;
        }

        std::vector<Line::LinesData> Line::getLinesMethod2(cv::Mat &frame)
        {
            vector<Vec3f> lines;
            HoughLines(frame, lines, 1, CV_PI / 180, this->line_length_, 0, 0);
            float angle;
            for (size_t i = 0; i < lines.size(); i++)
            {
                LinesData linesdata;
                linesdata.rho = lines[i][0], linesdata.theta = lines[i][1];
                linesdata.a = cos(linesdata.theta), linesdata.b = sin(linesdata.theta);
                linesdata.x0 = linesdata.a * linesdata.rho, linesdata.y0 = linesdata.b * linesdata.rho;
                linesdata.pt1.x = cvRound(linesdata.x0 + 1000 * (-linesdata.b));
                linesdata.pt1.y = cvRound(linesdata.y0 + 1000 * (linesdata.a));
                linesdata.pt2.x = cvRound(linesdata.x0 - 1000 * (-linesdata.b));
                linesdata.pt2.y = cvRound(linesdata.y0 - 1000 * (linesdata.a));
                linesdata.score = lines[i][2];
                if (linesdata.pt2.y > linesdata.pt1.y)
                {
                    swap(linesdata.pt1, linesdata.pt2);
                }
                angle = atan2(linesdata.pt2.y - linesdata.pt1.y, linesdata.pt2.x - linesdata.pt1.x) * 180 / CV_PI;
                angle = (angle > 0.0 ? angle : 180 + angle);

                // ***********只在 + 180下工作***********
                // *********请注意倒车*******
                double encircle_angle = 180 - abs(fmod(fabs(angle - line_angle_[0]), 2 * 180) - 180);
                if (encircle_angle < line_angle_[1])
                {
                    linesdata.angle_diff = encircle_angle;
                    this->lines_data_all_.push_back(linesdata);
                }
            }
            return this->lines_data_all_;
        }

        std::vector<int> Line::getScoring(const cv::Mat &frame, bool normalization)
        {

            std::vector<int> score;
            switch (this->scoring_rules_)
            {
            case ias::feature::_SCORE:
            {
                cv::Mat frameNMS, x_grad, y_grad;
                Sobel(frame.clone(), x_grad, CV_8U, 1, 0, 3);  //边缘检测算子
                Sobel(frame.clone(), y_grad, CV_8U, 0, 1, 3);
                cv::addWeighted(x_grad, this->cols_rows_ratio_[0], y_grad, this->cols_rows_ratio_[1], 0, frameNMS);
                if (normalization == true)
                {
                    threshold(frameNMS, frameNMS, 1, 1, THRESH_BINARY);
                }
                if (frameNMS.channels() > 1)
                {
                    cvtColor(frameNMS, frameNMS, cv::COLOR_BGR2GRAY);
                }
                morphologyEx(frameNMS, frameNMS, MORPH_HITMISS, this->scoring_hit_kernel_);
                for (size_t i = 0; i < this->lines_data_all_.size(); i++)
                {
                    Mat mask(frameNMS.rows, frameNMS.cols, CV_8UC1, Scalar::all(0));
                    line(mask, this->lines_data_all_[i].pt1, this->lines_data_all_[i].pt2, Scalar(255), 1, LINE_8);
                    cv::bitwise_and(frameNMS, mask, mask);
                    score.push_back(countNonZero(mask));
                    lines_data_all_[i].score = score[i];
                }
                break;
            }
            case ias::feature::P_SCORE:
            {
                std::cout << "P_SCORE" << endl;
                break;
            }
            case ias::feature::N_SCORE:
            {
                std::cout << "N_SCORE" << endl;

                break;
            }
            case ias::feature::PN_SCORE:
            {
                cv::Mat x_grad, y_grad;
                cv::Mat p_frameNMS, n_frameNMS, frame16s = cv::Mat(frame.rows, frame.cols, CV_16SC1, cv::Scalar(0));
                frame.convertTo(frame16s, CV_16SC1);

                Sobel(frame16s, x_grad, CV_16S, 1, 0, 3);
                Sobel(frame16s, y_grad, CV_16S, 0, 1, 3);
                cv::addWeighted(x_grad, this->cols_rows_ratio_[0], y_grad, this->cols_rows_ratio_[1], 0, frame16s);
                threshold(frame16s, p_frameNMS, this->binary_threshold_, 255, cv::THRESH_TOZERO);
                threshold(frame16s, n_frameNMS, -1 * this->binary_threshold_, 255, cv::THRESH_TOZERO_INV);
                if (normalization == true)
                {
                    threshold(p_frameNMS, p_frameNMS, 1, 1, THRESH_BINARY);
                    threshold(n_frameNMS, n_frameNMS, 1, 1, THRESH_BINARY);
                }
                p_frameNMS.convertTo(p_frameNMS, CV_8UC1);
                n_frameNMS = abs(n_frameNMS);
                n_frameNMS.convertTo(n_frameNMS, CV_8UC1);
                if (p_frameNMS.channels() > 1)
                {
                    cvtColor(p_frameNMS, p_frameNMS, cv::COLOR_BGR2GRAY);
                    cvtColor(n_frameNMS, n_frameNMS, cv::COLOR_BGR2GRAY);
                }
                morphologyEx(p_frameNMS, p_frameNMS, MORPH_HITMISS, this->scoring_hit_kernel_);

                for (size_t i = 0; i < this->lines_data_all_.size(); i++)
                {
                    Mat p_mask(p_frameNMS.rows, p_frameNMS.cols, CV_8UC1, Scalar::all(0));
                    Mat n_mask(p_frameNMS.rows, p_frameNMS.cols, CV_8UC1, Scalar::all(0));
                    line(p_mask, this->lines_data_all_[i].pt1, this->lines_data_all_[i].pt2, Scalar(255), 1, LINE_8);

                    //线段右移2个像素
                    line(n_mask, {this->lines_data_all_[i].pt1.x + 2, this->lines_data_all_[i].pt1.y}, {this->lines_data_all_[i].pt2.x + 2, this->lines_data_all_[i].pt2.y}, cv::Scalar(255), 1, LINE_8);
                    cv::bitwise_and(p_frameNMS, p_mask, p_mask);
                    cv::bitwise_and(n_frameNMS, n_mask, n_mask);
                    score.push_back(countNonZero(p_mask) + countNonZero(n_mask));
                    lines_data_all_[i].score = score[i];
                }

                break;
            }

            case ias::feature::HOUGH_SCORE:
            {
                for (size_t i = 0; i < this->lines_data_all_.size(); i++)
                {
                    score.push_back(lines_data_all_[i].score);
                }
            }
            default:
                break;
            }
            return score;
        }

        bool Line::getCrossPoint(float rho1, float theta1, float rho2, float theta2, cv::Point &cross)
        {
            float ct1 = cosf(theta1);
            float st1 = sinf(theta1);
            float ct2 = cosf(theta2);
            float st2 = sinf(theta2);
            float d = ct1 * st2 - st1 * ct2;
            if (d != 0.0f)
            {
                cross.x = (int)((st2 * rho1 - st1 * rho2) / d);
                cross.y = (int)((-ct2 * rho1 + ct1 * rho2) / d);
                return (1);
            }
            else
            {
                return (0);
            }
        }

        bool Line::LinesData::operator==(const LinesData &rhs)
        {

            return ((a == rhs.a) && (b == rhs.b) && (x0 == rhs.x0) && (y0 == rhs.y0) && (rho == rhs.rho) && (theta == rhs.theta) && (pt1 == rhs.pt1) && (pt2 == rhs.pt2));
        }

        std::vector<Line::LinesData> Line::LineNms(const cv::Mat &frame)
        {
            std::vector<Line::LinesData> lines_data_Nns;
            cv::Point cross;
            auto score = getScoring(frame, this->normalization_);
            if (score.size() != 0)
            {
                ias::Sort sort;
                vector<size_t> idx = sort.vectorSort(score, "DOWN");
                lines_data_Nns.push_back(this->lines_data_all_[idx[0]]);
                for (size_t i = 1; i < score.size(); i++)
                {
                    for (auto n : lines_data_Nns)
                    {
                        if (getCrossPoint(this->lines_data_all_[idx[i]].rho,
                                          this->lines_data_all_[idx[i]].theta, n.rho, n.theta, cross))
                        {
                            if ((1 - 50 < cross.x && cross.x < frame.cols + 50) && (1 - 50 < cross.y && cross.y < frame.rows + 50))
                            {
                                break;
                            }
                        }

                        if (n == lines_data_Nns.back() && score[idx[i]] > this->matching_score_)
                        {
                            lines_data_Nns.push_back(this->lines_data_all_[idx[i]]);
                            break;
                        }
                    }
                }
            }
            return lines_data_Nns;
        }
        std::vector<Line::LinesData> Line::getLinesDataAll()
        {
            return this->lines_data_all_;
        }
        bool Line::setScoringHitKernel_(const cv::Mat &kernel)
        {
            this->scoring_hit_kernel_ = kernel.clone();
            return 1;
        }
        bool Line::setLineLength_(unsigned int line_length)
        {
            this->line_length_ = line_length;
            bool flag = false;
            if (this->line_length_ == line_length)
            {
                flag = true;
            }
            return flag;
        }
        bool Line::setNormalization_(const bool normalization)
        {
            this->normalization_ = normalization;
            bool flag = false;
            if (this->normalization_ == normalization)
            {
                flag = true;
            }
            return flag;
        }
        unsigned int Line::setBinary_Threshold_(const unsigned int threshold)
        {
            this->binary_threshold_ = threshold;
            return this->binary_threshold_;
        }
        bool Line::setCols_Rows_Ratio_(const double ratio[2])
        {
            this->cols_rows_ratio_[0] = ratio[0];
            this->cols_rows_ratio_[1] = ratio[1];
            return true;
        }
        std::vector<Line::LinesData> Line::Find(const cv::Mat &frame, const cv::Mat &mask)
        {

            std::vector<Line::LinesData> lines_data_Nns;
            if (!frame.empty())
            {
                cv::Mat line_frame = frame.clone(), x_grad, y_grad;
                if (frame.channels() > 1)
                {
                    cvtColor(frame, line_frame, cv::COLOR_BGR2GRAY); //转换成灰度图
                }
                Sobel(line_frame, x_grad, CV_8U, 1, 0, 3);  //图像边缘检测
                Sobel(line_frame, y_grad, CV_8U, 0, 1, 3);  //图像边缘检测
                cv::addWeighted(x_grad, this->cols_rows_ratio_[0], y_grad, this->cols_rows_ratio_[1], 0, line_frame);

                this->lines_data_all_.clear();
                if (!mask.empty())
                {
                    cv::bitwise_and(mask, line_frame, line_frame);
                }
                threshold(line_frame, line_frame, this->binary_threshold_, 255, cv::THRESH_TOZERO);

                switch (line_method_)
                {
                case feature::LINE_METHOD_1:
                    getLinesMethod1(line_frame);
                    break;

                case feature::LINE_METHOD_2:
                    getLinesMethod2(line_frame);
                    break;

                default:
                    break;
                }
                lines_data_Nns = LineNms(frame);
            }
            return lines_data_Nns;
        }

        Line::Line(unsigned int matching_num, int matching_score, const unsigned int scoring_rules, const unsigned int line_method,
                   const double (&angle)[2], const double length) : ShapeMatching(matching_num, matching_score)
        {
            line_angle_[0] = angle[0];
            line_angle_[1] = angle[1];
            line_length_ = length;
            line_method_ = line_method;
            scoring_rules_ = scoring_rules;
        }

        double LineTool::getLineAngle(cv::Point2d &data1, cv::Point2d &data2)
        {
            return atan2(data1.y - data2.y, data1.x - data2.x) * 180 / CV_PI;
        }

        double LineTool::getLineLength(cv::Point2d &data1, cv::Point2d &data2)
        {
            return sqrtf(powf(abs(data2.x - data1.x), 2) + powf(abs(data2.y - data1.y), 2));
        }

        cv::Point2d LineTool::rotatePoint(const cv::Point2d &rotate_point, const cv::Point2d &rotate_centre, const double &rotate_angle)
        {
            cv::Point2d rotate_result;
            double angle = rotate_angle * CV_PI / 180;
            rotate_result.x = ((rotate_point.x - rotate_centre.x) * cos(angle)) - ((rotate_point.y - rotate_centre.y) * sin(angle)) + rotate_centre.x;
            rotate_result.y = ((rotate_point.x - rotate_centre.x) * sin(angle)) + ((rotate_point.y - rotate_centre.y) * cos(angle)) + rotate_centre.y;
            return rotate_result;
        }

        bool LineTool::getRectCross(const std::vector<cv::Point2d> &point_data, cv::Mat &cross, const cv::Rect2d &rect_data)
        {

            cv::Mat backdrop_rect(rect_data.y + rect_data.height + 1, rect_data.x + rect_data.width + 1, CV_8UC1, cv::Scalar(0));
            cv::Mat backdrop_line = backdrop_rect.clone();
            rectangle(backdrop_rect, rect_data.tl(), rect_data.br(), cv::Scalar(255), 1, LINE_8, 0);
            cv::line(backdrop_line, point_data[0], point_data[1], cv::Scalar(255), 1, LINE_8);
            cv::bitwise_and(backdrop_rect, backdrop_line, backdrop_line);
            cv::findNonZero(backdrop_line, cross);

            return true;
        }

    } // namespace feature

} // namespace ias
