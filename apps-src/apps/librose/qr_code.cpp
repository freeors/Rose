//______________________________________________________________________________________
// Program : OpenCV based QR code Detection and Retrieval
// Author  : Bharath Prabhuswamy
// Github  : https://github.com/bharathp666/opencv_qr
//______________________________________________________________________________________

#include <opencv2/opencv.hpp>
#include "global.hpp"

#include <iostream>
#include <cmath>
#include "sdl_utils.hpp"
#include "wml_exception.hpp"
#include "qr_code.hpp"

#include "rose_config.hpp"
#include "preferences.hpp"
#include "font.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"


#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>


// #include <opencv2/ts.hpp>
#include <opencv2/wechat_qrcode.hpp>

#include "zxing/BarcodeFormat.h"
#include "zxing/MultiFormatWriter.h"
#include "zxing/TextUtfEncoding.h"
#include "zxing/BitMatrix.h"
#include "zxing/ByteMatrix.h"

std::string find_qr(const cv::Mat& image)
{
	VALIDATE(!image.empty(), null_str);

	std::string result;
	std::stringstream ss;

	std::vector<cv::Mat> points;

    // can not find the model file
    // so we temporarily comment it out
	// const std::string prefix = game_config::preferences_dir + "/tflites/wechat_qrcode/";
	const std::string prefix = game_config::path + "/data/core/tflites/wechat_qrcode/";

	tfile detect_prototxt(prefix + "detect.prototxt", GENERIC_READ, OPEN_EXISTING);
	size_t detect_prototxt_len = detect_prototxt.read_2_data();
	VALIDATE(detect_prototxt_len != 0, null_str);

	tfile detect_caffemodel(prefix + "detect.caffemodel", GENERIC_READ, OPEN_EXISTING);
	size_t detect_caffemodel_len = detect_caffemodel.read_2_data();
	VALIDATE(detect_caffemodel_len != 0, null_str);

	tfile sr_prototxt(prefix + "sr.prototxt", GENERIC_READ, OPEN_EXISTING);
	size_t sr_prototxt_len = sr_prototxt.read_2_data();
	VALIDATE(sr_prototxt_len != 0, null_str);

	tfile sr_caffemodel(prefix + "sr.caffemodel", GENERIC_READ, OPEN_EXISTING);
	size_t sr_caffemodel_len = sr_caffemodel.read_2_data();
	VALIDATE(sr_caffemodel_len != 0, null_str);

	uint32_t start_ticks = SDL_GetTicks();
    auto detector = cv::wechat_qrcode::WeChatQRCode(
         detect_prototxt.data, detect_prototxt_len, detect_caffemodel.data, detect_caffemodel_len, 
		 sr_prototxt.data, sr_prototxt_len, sr_caffemodel.data, sr_caffemodel_len);
	uint32_t end_step1_ticks = SDL_GetTicks();
    std::vector<std::string> decoded_info = detector.detectAndDecode(image, points);

	uint32_t stop_ticks = SDL_GetTicks();
	if (!decoded_info.empty()) {
		result = decoded_info.at(0);
	}

	ss << image.cols << "x" << image.rows << ": find_qr, used: ";
	ss << (stop_ticks - start_ticks) << "(WeChatQRCode:" << (end_step1_ticks - start_ticks) << " + detectAndDecode:" << (stop_ticks - end_step1_ticks) << ")";

	SDL_Log("%s", ss.str().c_str());
	return result;
}

surface generate_qr(const std::string& text, int width, int height)
{
	VALIDATE(!text.empty() && width >= 100 && height >= 100, null_str);

	surface result;
	int margin = 10;
	int eccLevel = 2; // -1
	std::string format = "QR_CODE";

	try {
		auto barcodeFormat = ZXing::BarcodeFormatFromString(format);
		VALIDATE(barcodeFormat != ZXing::BarcodeFormat::FORMAT_COUNT, null_str);

		ZXing::MultiFormatWriter writer(barcodeFormat);
		if (margin >= 0) {
			writer.setMargin(margin);
		}
		if (eccLevel >= 0) {
			writer.setEccLevel(eccLevel);
		}

		auto bitmap = writer.encode(ZXing::TextUtfEncoding::FromUtf8(text), width, height).toByteMatrix();
		if (bitmap.width() == width && bitmap.height() == height) {
			cv::Mat gray(bitmap.height(), bitmap.width(), CV_8UC1, (uint8_t*)bitmap.data());
			cv::Mat tmp;
			cvtColor(gray, tmp, cv::COLOR_GRAY2BGRA);
			result = clone_surface(tmp);
		}

	} catch (const std::exception& e) {
		SDL_Log("generate_qr: %s", e.what());
	}
	return result;
}