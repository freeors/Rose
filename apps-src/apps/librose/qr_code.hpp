#ifndef LIBROSE_QR_CODE_HPP_INCLUDED
#define LIBROSE_QR_CODE_HPP_INCLUDED

#include <string>

#include <opencv2/imgproc.hpp>

std::string find_qr(const cv::Mat& image);
std::string decode_qr(cv::Mat& grey);

surface generate_qr(const std::string& text, int width, int height);

#endif
