/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define GETTEXT_DOMAIN "rose-lib"

#ifdef _WIN32
#if !defined(UNICODE)
#error "On windows, this file must complete Use Unicode Character Set"
#endif
#endif

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/channels/leagor_ble.hpp>

#include "wml_exception.hpp"
#include <SDL_log.h>

tblebuf::~tblebuf()
{
	if (recv_data_) {
		free(recv_data_);
		recv_data_ = nullptr;
	}

	if (packet_data_) {
		free(packet_data_);
		packet_data_ = nullptr;
	}
}

void tblebuf::enqueue(const uint8_t* data, int len)
{
	VALIDATE(data != nullptr && len > 0, null_str);

	// SDL_Log("tblebuf::enqueue---len: %i, recv_data_vsize_: %i", len, recv_data_vsize_);

	const int min_size = posix_align_ceil(recv_data_vsize_ + len, 4096);

	if (min_size > recv_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(min_size);
		if (recv_data_) {
			if (recv_data_vsize_) {
				memcpy(tmp, recv_data_, recv_data_vsize_);
			}
			free(recv_data_);
		}
		recv_data_ = tmp;
		recv_data_size_ = min_size;
	}

	memcpy(recv_data_ + recv_data_vsize_, data, len);
	recv_data_vsize_ += len;

	while (true) {
		while (true) {
			// 1. must prefix with LEAGOR_BLE_PREFIX_BYTE
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_; i ++) {
				if (recv_data_[i] == LEAGOR_BLE_PREFIX_BYTE) {
					break;
				}
				skip ++;
			}
			if (skip && skip != recv_data_vsize_) {
				memcpy(recv_data_, recv_data_ + skip, recv_data_vsize_ - skip);
			}
			recv_data_vsize_ -= skip;
			if (recv_data_vsize_ < LEAGOR_BLE_MTU_HEADER_SIZE) { // payload len maybe is 0.
				return;
			}
	
			// 2. len
			const int cmd = recv_data_[1];
			if (recv_cmds_.count(cmd) == 0) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			const int len = posix_mku16(recv_data_[2], recv_data_[3]);
			if (recv_data_vsize_ < LEAGOR_BLE_MTU_HEADER_SIZE + len) {
				return;
			}
			break;
		}

		const int cmd = recv_data_[1];
		const int len = posix_mku16(recv_data_[2], recv_data_[3]);
		SDL_Log("tblebuf::enqueue, cmd: %i, len: %i", cmd, len);

		const int len2 = LEAGOR_BLE_MTU_HEADER_SIZE + len;
		VALIDATE(recv_data_vsize_ >= len2, null_str);

		if (did_read_) {
			did_read_(cmd, recv_data_ + LEAGOR_BLE_MTU_HEADER_SIZE, len);
		}

		if (recv_data_vsize_ > len2) {
			memcpy(recv_data_, recv_data_ + len2, recv_data_vsize_ - len2);
		}
		recv_data_vsize_ -= len2;

		if (recv_data_vsize_ == 0) {
			SDL_Log("tblebuf::enqueue(1.2): there is no extra data");
		} else {
			std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
			SDL_Log("tblebuf::enqueue(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

void tblebuf::resize_packet_data(int size)
{
	const int min_size = posix_align_ceil(size, 4096);

	if (min_size > packet_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(min_size);
		if (packet_data_) {
			free(packet_data_);
		}
		packet_data_ = tmp;
		packet_data_size_ = min_size;
	}
}

void tblebuf::form_send_packet(int cmd, uint8_t* payload, int len, tuint8data& result)
{
	if (payload != nullptr) {
		VALIDATE(len > 0 && len <= 65535, null_str);
	} else {
		VALIDATE(len == 0, null_str);
	}
	VALIDATE(send_cmds_.count(cmd) != 0, null_str);

	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + len;
	resize_packet_data(packet_len);
	
	packet_data_[0] = LEAGOR_BLE_PREFIX_BYTE;
	packet_data_[1] = cmd;
	packet_data_[2] = posix_lo8(posix_lo16(len));
	packet_data_[3] = posix_hi8(posix_lo16(len));
	if (len > 0) {
		memcpy(packet_data_ + LEAGOR_BLE_MTU_HEADER_SIZE, payload, len);
	}
	result.ptr = packet_data_;
	result.len = packet_len;
}

uint8_t* tblebuf::fill_mtu_4bytes(uint8_t* data, int cmd, int payload_len)
{
	data[0] = LEAGOR_BLE_PREFIX_BYTE;
	data[1] = cmd;
	data[2] = posix_lo8(posix_lo16(payload_len));
	data[3] = posix_hi8(posix_lo16(payload_len));

	return data + 4;
}

void tblebuf::form_queryip_req(tuint8data& result)
{
	int payload_len = LEAGOR_BLE_VERSION_SIZE;
	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + payload_len;
	resize_packet_data(packet_len);

	uint8_t* wt_ptr = fill_mtu_4bytes(packet_data_, msg_queryip_req, payload_len);
	wt_ptr[0] = LEAGOR_BLE_VERSION;
	wt_ptr ++;

	VALIDATE((int)(wt_ptr - packet_data_) == packet_len, null_str);

	result.ptr = packet_data_;
	result.len = packet_len;
}

int tblebuf::parse_queryip_req(const uint8_t* data, int len)
{
	const int must_payload_len = LEAGOR_BLE_VERSION_SIZE;
	if (len != must_payload_len) {
		return LEAGOR_BLE_VERSION_UNSPEC;
	}
	return data[0];
}

void tblebuf::form_queryip_resp(const tqueryip_resp& src, tuint8data& result)
{
	int payload_len = LEAGOR_BLE_VERSION_SIZE + LEAGOR_BLE_AF_SIZE + LEAGOR_BLE_ADDRESS_SIZE + LEAGOR_BLE_FLAGS_SIZE;
	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + payload_len;
	resize_packet_data(packet_len);
	memset(packet_data_, 0, packet_len);

	uint8_t* wt_ptr = fill_mtu_4bytes(packet_data_, msg_queryip_resp, payload_len);
	VALIDATE(src.version == LEAGOR_BLE_VERSION, null_str);
	wt_ptr[0] = LEAGOR_BLE_VERSION;
	wt_ptr ++;
	wt_ptr[0] = (uint8_t)(src.af);
	wt_ptr ++;
	if (src.af == LEAGOR_BLE_AF_INET) {
		memcpy(wt_ptr, &src.a.ipv4, 4);
	}
	wt_ptr += LEAGOR_BLE_ADDRESS_SIZE;
	memcpy(wt_ptr, &src.flags, LEAGOR_BLE_FLAGS_SIZE);
	wt_ptr += LEAGOR_BLE_FLAGS_SIZE;

	VALIDATE((int)(wt_ptr - packet_data_) == packet_len, null_str);

	result.ptr = packet_data_;
	result.len = packet_len;
}

tblebuf::tqueryip_resp tblebuf::parse_queryip_resp(const uint8_t* payload, int len)
{
	tqueryip_resp result;
	memset(&result, 0, sizeof(result));

	if (len == 0) {
		return result;
	}
	result.version = payload[0];
	result.af = LEAGOR_BLE_AF_UNSPEC;

	const int must_payload_len = LEAGOR_BLE_VERSION_SIZE + LEAGOR_BLE_AF_SIZE + LEAGOR_BLE_ADDRESS_SIZE + LEAGOR_BLE_FLAGS_SIZE;
	if (result.version != LEAGOR_BLE_VERSION || len != must_payload_len) {
		// if version dismatch, version is launcer's version and af is LEAGOR_BLE_AF_UNSPEC.
		return result;
	}

	const uint8_t* rd_ptr = payload + LEAGOR_BLE_VERSION_SIZE;
	result.af = rd_ptr[0];
	rd_ptr += LEAGOR_BLE_AF_SIZE;

	if (result.af == LEAGOR_BLE_AF_INET) {
		memcpy(&result.a.ipv4, rd_ptr, 4);
	}
	rd_ptr += LEAGOR_BLE_ADDRESS_SIZE;

	memcpy(&result.flags, rd_ptr, 4);
	rd_ptr += LEAGOR_BLE_FLAGS_SIZE;

	VALIDATE((int)(rd_ptr - payload) == must_payload_len, null_str);
	return result;
}

void tblebuf::form_wifilist_resp_ipv4(uint32_t ip, uint32_t flags, SDL_WifiScanResult* results, int count, tuint8data& out)
{
	// caller must sort results by rssi.
	const int max_wifiaps = 20;
	count = SDL_min(max_wifiaps, count);

	int payload_len = LEAGOR_BLE_AF_SIZE + LEAGOR_BLE_ADDRESS_SIZE + LEAGOR_BLE_FLAGS_SIZE + 1; // +1(count)
	if (count > 0) {
		VALIDATE(results != nullptr, null_str);
		for (int at = 0; at < count; at ++) {
			const SDL_WifiScanResult& result = results[at];
			// 4(rssi) + 4(flags)
			payload_len += 4 + 4 + SDL_strlen(result.ssid) + 1;
		}
	} else {
		VALIDATE(results == nullptr, null_str);
	}

	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + payload_len;
	resize_packet_data(packet_len);

	uint8_t* wt_ptr = fill_mtu_4bytes(packet_data_, msg_wifilist_resp, payload_len);

	wt_ptr[0] = LEAGOR_BLE_AF_INET;
	wt_ptr ++;
	memcpy(wt_ptr, &ip, 4);
	wt_ptr += LEAGOR_BLE_ADDRESS_SIZE;
	memcpy(wt_ptr, &flags, sizeof(flags));
	wt_ptr += 4;
	wt_ptr[0] = (uint8_t)count;
	wt_ptr ++;

	if (count > 0) {
		VALIDATE(results != nullptr, null_str);
		for (int at = 0; at < count; at ++) {
			const SDL_WifiScanResult& result = results[at];
			// 4(rssi) + 4(flags)
			memcpy(wt_ptr, &result.rssi, sizeof(result.rssi));
			wt_ptr += 4;
			memcpy(wt_ptr, &result.flags, sizeof(result.flags));
			wt_ptr += 4;
			int ssid_len = SDL_strlen(result.ssid);
			if (ssid_len != 0) {
				memcpy(wt_ptr, result.ssid, ssid_len);
				wt_ptr += ssid_len;
			}
			wt_ptr[0] = '\0';
			wt_ptr ++;
		}

		SDL_free(results);
	}
	VALIDATE((int)(wt_ptr - packet_data_) == packet_len, null_str);

	out.ptr = packet_data_;
	out.len = packet_len;
}

void tblebuf::form_connectwifi_req(const std::string& ssid, const std::string& password, tuint8data& result)
{
	VALIDATE(!ssid.empty() && !password.empty(), null_str);
	const int ssid_size = ssid.size();
	const int password_size = password.size();

	int payload_len = ssid_size + 1 + password_size + 1;

	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + payload_len;
	resize_packet_data(packet_len);

	packet_data_[0] = LEAGOR_BLE_PREFIX_BYTE;
	packet_data_[1] = msg_connectwifi_req;
	packet_data_[2] = posix_lo8(posix_lo16(payload_len));
	packet_data_[3] = posix_hi8(posix_lo16(payload_len));

	uint8_t* wt_ptr = packet_data_ + LEAGOR_BLE_MTU_HEADER_SIZE;
	memcpy(wt_ptr, ssid.c_str(), ssid_size);
	wt_ptr += ssid_size;
	wt_ptr[0] = '\0';
	wt_ptr ++;

	memcpy(wt_ptr, password.c_str(), password_size);
	wt_ptr += password_size;
	wt_ptr[0] = '\0';
	wt_ptr ++;

	VALIDATE((int)(wt_ptr - packet_data_) == packet_len, null_str);

	result.ptr = packet_data_;
	result.len = packet_len;
}

std::string tblebuf::parse_connectwifi_req(const uint8_t* data, int len, std::string& password)
{
	std::string ssid;
	password.clear();

	const int min_connectwifi_payload_len = 4;
	if (len < min_connectwifi_payload_len || data[len - 1] != '\0') {
		return ssid;
	}

	int ssid_terminate_at = nposm;
	for (int at = 0; at < len; at ++) {
		if (data[at] == '\0') {
			if (ssid_terminate_at == nposm) {
				ssid.assign((const char*)data, 0, at);
				ssid_terminate_at = at;
			} else {
				VALIDATE(at == len - 1, null_str);
				password.assign((const char*)data + ssid_terminate_at + 1);
			}
		}
	}
	if (password.empty()) {
		ssid.clear();
	}
	return ssid;
}

void tblebuf::form_removewifi_req(const std::string& ssid, tuint8data& result)
{
	VALIDATE(!ssid.empty(), null_str);
	const int ssid_size = ssid.size();

	int payload_len = ssid_size + 1;

	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + payload_len;
	resize_packet_data(packet_len);

	uint8_t* wt_ptr = fill_mtu_4bytes(packet_data_, msg_removewifi_req, payload_len);

	memcpy(wt_ptr, ssid.c_str(), ssid_size);
	wt_ptr += ssid_size;
	wt_ptr[0] = '\0';
	wt_ptr ++;

	VALIDATE((int)(wt_ptr - packet_data_) == packet_len, null_str);

	result.ptr = packet_data_;
	result.len = packet_len;
}

std::string tblebuf::parse_removewifi_req(const uint8_t* data, int len)
{
	std::string ssid;

	const int min_removewifi_payload_len = 2;
	if (len < min_removewifi_payload_len || data[len - 1] != '\0') {
		return ssid;
	}

	for (int at = 0; at < len; at ++) {
		if (data[at] == '\0') {
			ssid.assign((const char*)data, 0, at);
			break;
		}
	}

	return ssid;
}