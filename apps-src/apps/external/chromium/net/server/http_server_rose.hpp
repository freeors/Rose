#ifndef LIBROSE_HTTP_SERVER_ROSE_HPP_INCLUDED
#define LIBROSE_HTTP_SERVER_ROSE_HPP_INCLUDED

#include "util.hpp"
#include "thread.hpp"
#include "serialization/string_utils.hpp"
#include <set>
#include "net/server/http_server.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server_request_info.h"

// webrtc
#include "rtc_base/event.h"
#include "base/threading/thread.h"

namespace net {

struct tresponse_data
{
	tresponse_data()
		: status_code(HTTP_OK)
		, content_type("application/json; charset=UTF-8")
	{}

	HttpStatusCode status_code;
	std::string content_type;
	std::string data;
};

// Bas on HttpServerTest of chromium.
class HttpServerRose: public HttpServer::Delegate, public rtc::MessageHandler
{
	friend class thttpd_manager;
public:
	HttpServerRose();

	void SetUp(uint32_t ipaddr);

	void TearDown();

	void OnConnect(int connection_id) override;

	void OnHttpRequest(int connection_id,
						const HttpServerRequestInfo& info) override;

	void OnWebSocketRequest(int connection_id,
							const HttpServerRequestInfo& info) override {
		NOTREACHED();
	}

	void OnWebSocketMessage(int connection_id, std::string data) override {
		NOTREACHED();
	}

	void OnClose(int connection_id) override;

	void RunUntilConnectionIdClosed(int connection_id);

	std::unordered_map<int, bool>& connection_map() { return connection_map_; }

	const IPEndPoint& server_address() const { return server_address_; }
	const std::string& server_url() const { return server_url_; }

private:
	enum {POST_MSG_CONNECT, POST_MSG_REQUET, POST_MSG_CLOSE};
	struct tmsg_data_request: public rtc::MessageData {
		explicit tmsg_data_request(const HttpServerRequestInfo& info, tresponse_data& resp)
			: info(info)
			, resp(resp)
		{}

		const HttpServerRequestInfo& info;
		tresponse_data& resp;
	};
	void OnMessage(rtc::Message* msg) override;

protected:
	// threading::mutex mutex_;
	std::unique_ptr<HttpServer> server_;
	IPEndPoint server_address_;
	std::string server_url_;
	base::Closure run_loop_quit_func_;
	std::unordered_map<int /* connection_id */, bool /* connected */> connection_map_;

private:
	twebrtc_send_helper send_helper_;
	int quit_on_close_connection_;
};

class thttpd_manager: public tserver_
{
public:
	thttpd_manager();
	virtual ~thttpd_manager();

	void start(uint32_t ipaddr) override;
	void stop() override;
	const std::string& url() const override
	{ 
		return delegate_.get() != nullptr? delegate_->server_url(): null_str; 
	}

private:
	void did_set_event();
	void start_internal(uint32_t ipaddr);
	void stop_internal();

private:
	std::unique_ptr<base::Thread> thread_;
	std::unique_ptr<HttpServerRose> delegate_;
	rtc::Event e_;
};

}


#endif
