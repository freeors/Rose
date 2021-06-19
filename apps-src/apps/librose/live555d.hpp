#ifndef LIBROSE_LIVE555D_HPP_INCLUDED
#define LIBROSE_LIVE555D_HPP_INCLUDED

#include "util.hpp"
#include "config.hpp"
#include <set>
#include "thread.hpp"

// webrtc
#include "rtc_base/event.h"
#include <rtc_base/thread_checker.h>
// chromium
#include "base/threading/thread.h"

namespace live555d {

class tvidcap
{
public:
	virtual void setup(int sessionid) = 0;
	virtual uint32_t read(int sessionid, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize) = 0;
	virtual void teardown(int sessionid) = 0;
};

void register_vidcap(tvidcap& vidcap);
void deregister_vidcap();

class tmanager: public tserver_
{
public:
	static tvidcap* vidcap;
	static threading::mutex vidcap_mutex;
	static std::unique_ptr<rtc::ThreadChecker> live555d_thread_checker;

	tmanager();

	void start(uint32_t ipaddr) override;
	void stop() override;
	const std::string& url() const override { return server_url_; }

private:
	void did_set_event();
	void start_internal(uint32_t ipaddr);
	void stop_internal();

private:
	std::unique_ptr<base::Thread> thread_;
	char eventLoopWatchVariable_;
	rtc::Event e_;
	std::string server_url_;
};

}


#endif
