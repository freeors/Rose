#ifndef LIBROSE_LIVE555_HPP_INCLUDED
#define LIBROSE_LIVE555_HPP_INCLUDED

#include "util.hpp"
#include "config.hpp"
#include <set>
#include "filesystem.hpp"


namespace live555 {

// [IN]rtsp_url: rtsp://192.168.1.180:8554/0+
// [OUT]httpd_url: 192.168.1.180:8080
int start(const std::string& rtsp_url, bool send_rtcp, std::string& httpd_url, uint32_t* ipv4_ptr);
void stop(int id);
int frame_slice(int id, uint8_t* buffer, int buffer_size);

}


#endif
