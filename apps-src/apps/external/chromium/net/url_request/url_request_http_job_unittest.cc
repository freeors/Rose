// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
/*
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
*/
#include "base/test/task_environment.h"
#include "net/base/auth.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/cert/ct_policy_status.h"
// #include "net/cookies/cookie_monster.h"
// #include "net/cookies/cookie_store_test_callbacks.h"
// #include "net/cookies/cookie_store_test_helpers.h"
#include "net/http/http_transaction_factory.h"
// #include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
// #include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/next_proto.h"
// #include "net/socket/socket_test_util.h"
// #include "net/test/cert_test_util.h"
#include "net/socket/client_socket_factory.h"
// #include "net/test/embedded_test_server/default_handlers.h"
// #include "net/test/gtest_util.h"
// #include "net/test/test_data_directory.h"
// #include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
// #include "net/url_request/websocket_handshake_userdata_key.h"
// #include "net/websockets/websocket_test_util.h"
// #include "testing/gmock/include/gmock/gmock.h"
// #include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include <base/threading/thread_task_runner_handle.h>

// #include "base/test/task_environment.h"

#include "serialization/string_utils.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include "SDL.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "net/net_test_jni_headers/AndroidNetworkLibraryTestUtil_jni.h"
#endif

// using net::test::IsError;
// using net::test::IsOk;

namespace net {
/*
namespace {

using ::testing::Return;

const char kSimpleGetMockWrite[] =
    "GET / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: \r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

const char kSimpleHeadMockWrite[] =
    "HEAD / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: \r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

const char kTrustAnchorRequestHistogram[] =
    "Net.Certificate.TrustAnchor.Request";

const char kCTComplianceHistogramName[] =
    "Net.CertificateTransparency.RequestComplianceStatus";
*/


// TaskEnvironment is required to instantiate a
// net::ConfiguredProxyResolutionService, which registers itself as an IP
// Address Observer with the NetworkChangeNotifier.
// using URLRequestHttpJobWithProxyTest = TestWithTaskEnvironment;

class URLRequestHttpJobWithProxy {
 public:
  explicit URLRequestHttpJobWithProxy(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service)
      : proxy_resolution_service_(std::move(proxy_resolution_service)),
        context_(new TestURLRequestContext(true)) {
    // context_->set_client_socket_factory(&socket_factory_);
	context_->set_client_socket_factory(ClientSocketFactory::GetDefaultFactory());
    context_->set_network_delegate(&network_delegate_);
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());
    context_->Init();
  }

  // MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<TestURLRequestContext> context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJobWithProxy);
};

bool begin_show_slice_log = false;
static void show_slice(uint32_t start_ticks)
{
	if (begin_show_slice_log) {
		SDL_Log("%u ---show_slice---", SDL_GetTicks());
		begin_show_slice_log = false;
	} else {
		base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice, start_ticks));
	}
}

static void show_slice2(uint32_t start_ticks)
{
	SDL_Log("%u ---show_slice2---", SDL_GetTicks());
	// base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice2, start_ticks));
}

// @url: https://www.github.com
// @path: 1.html --> <Documents>/RoseApp/<appid>/1.html
bool fetch_url_data(const std::string& _url, const std::string& path, int timeout2)
{
	// base::test::TaskEnvironment task_environment(base::test::TaskEnvironment::TimeSource::SYSTEM_TIME, base::test::TaskEnvironment::MainThreadType::IO);
	VALIDATE(!path.empty(), null_str);

	const GURL url(_url);
	const std::string scheme = url.scheme();
	VALIDATE(scheme == "http" || scheme == "https", null_str);
	std::unique_ptr<ScopedPortException> port_lock;
	if (!url.port().empty()) {
		int port = utils::to_int(url.port());
		port_lock.reset(new ScopedPortException(port));
	}

	URLRequestHttpJobWithProxy http_job_with_proxy(nullptr);

	TestDelegate delegate;
	std::unique_ptr<URLRequest> req = 
        http_job_with_proxy.context_->CreateRequest(
			url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
	// turlrequest_instance_lock lock(*req.get());

	req->Start();
	CHECK(req->is_pending());

	begin_show_slice_log = false;
	uint32_t start_ticks = SDL_GetTicks();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice, start_ticks));
	const base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(4000); // 4 second.
	base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::Bind(&show_slice2, start_ticks), timeout);
	SDL_Log("%u fetch_url_data, pre delegate.RunUntilComplete()", SDL_GetTicks());
/*
	{
		uint32_t start_ticks = SDL_GetTicks();
		base::RunLoop loop;
		base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice, &loop, start_ticks, timeout));
		loop.Run();
	}
*/
    delegate.RunUntilComplete();
	SDL_Log("%u fetch_url_data, post delegate.RunUntilComplete()", SDL_GetTicks());
	begin_show_slice_log = true;

/*
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice, start_ticks));
	base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::Bind(&show_slice2, start_ticks), timeout);
	{
		base::RunLoop loop;
		loop.Run();
	}
*/
	std::string data = delegate.data_received();
	int status = delegate.request_status();
	if (delegate.request_status() != OK) {
		// "Fetch from " << _url << " fail. error: " << delegate.request_status();
		return false;
	}

	tfile file(path, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);
	posix_fwrite(file.fp, data.c_str(), data.size());
	return true;
}


}  // namespace net
