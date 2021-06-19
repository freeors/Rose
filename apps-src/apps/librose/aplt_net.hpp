#ifndef APLT_NET_HPP_INCLUDED
#define APLT_NET_HPP_INCLUDED

#include "rose_config.hpp"
#include "sdl_utils.hpp"
#include "version.hpp"
#include "aplt.hpp"

#include <json/json.h>
#include <net/url_request/url_request_http_job_rose.hpp>

namespace net {
// direct xmit width agbox
bool cswamp_getapplet(gui2::tprogress_& progress, int type, const std::string& bundleid, applet::tapplet& aplt, bool quiet);

enum {cswamp_login_type_password, cswamp_login_type_cookie, cswamp_login_type_count};
struct tcswamp_login_result
{
	std::string sessionid;
	std::string pwcookie;
};

bool cswamp_login(gui2::tprogress_& progress, int type, const std::string& username, const std::string& password, const std::string& deviceid, const std::string& devicename, bool quiet, tcswamp_login_result& result);
bool cswamp_logout(gui2::tprogress_& progress, const std::string& sessionid, bool quiet);
enum {app_kdesktop, app_launcher, app_count};
bool cswamp_syncappletlist(gui2::tprogress_& progress, const std::string& sessionid, int app, const std::vector<std::string>& adds, const std::vector<std::string>& removes, bool quiet);

}

#endif