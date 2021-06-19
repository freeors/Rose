/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Server Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_LEAGOR_BLE_H
#define FREERDP_CHANNEL_LEAGOR_BLE_H

#include <freerdp/channels/leagor_common_context.h>
#include <string>
#include <set>

#include <SDL_peripheral.h>

#include "util.hpp"

enum {msg_queryip_req = 1, 
	msg_updateip_req,
	msg_connectwifi_req,
	msg_removewifi_req,
	msg_wifilist_req,

	msg_queryip_resp = 50,
	msg_wifilist_resp
};

#define LEAGOR_BLE_MTU_HEADER_SIZE	4 // 1(prefix) + 1(cmd) + 2(len)
#define LEAGOR_BLE_PREFIX_BYTE		0x5a

#define LEAGOR_BLE_VERSION_UNSPEC	0
#define LEAGOR_BLE_VERSION			1 // current versin, 1
#define LEAGOR_BLE_VERSION_SIZE		1

// value same as socket. x:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/shared\ws2def.h
// AF=address family 
enum {LEAGOR_BLE_AF_UNSPEC = 0, // unspecified
	LEAGOR_BLE_AF_INET = 2, // internetwork: UDP, TCP, etc. (bytes: 4+1)
	LEAGOR_BLE_AF_INET6 = 23, // Internetwork Version 6 (bytes: 16+1)
	LEAGOR_BLE_AF_MAX = 255,
};
#define LEAGOR_BLE_AF_SIZE			1
// some scenario maybe require address include subnet-mask. 
// since AF_INET6 is 16, add subnet-mask(1byte), will to 17.
#define LEAGOR_BLE_ADDRESS_SIZE		18

#define LEAGOR_BLE_FLAGS_SIZE		4
enum {
	LEAGOR_BLE_FLAG_WIFI_ENABLED = 0x1,
};

class tblebuf
{
public:
	tblebuf(bool center)
		: recv_data_(nullptr)
		, recv_data_size_(0)
		, recv_data_vsize_(0)
		, packet_data_(nullptr)
		, packet_data_size_(0)
	{
		if (center) {
			// use as ble center
			recv_cmds_.insert(msg_queryip_resp);
			recv_cmds_.insert(msg_wifilist_resp);

			send_cmds_.insert(msg_queryip_req);
			send_cmds_.insert(msg_updateip_req);
			send_cmds_.insert(msg_connectwifi_req);
			send_cmds_.insert(msg_removewifi_req);
			send_cmds_.insert(msg_wifilist_req);

		} else {
			// use as ble peripheral
			recv_cmds_.insert(msg_queryip_req);
			recv_cmds_.insert(msg_updateip_req);
			recv_cmds_.insert(msg_connectwifi_req);
			recv_cmds_.insert(msg_removewifi_req);
			recv_cmds_.insert(msg_wifilist_req);

			send_cmds_.insert(msg_queryip_resp);
			send_cmds_.insert(msg_wifilist_resp);
		}
	}
	~tblebuf();

	void set_did_read(const std::function<void (int cmd, const uint8_t* data, int len)>& did)
	{
		did_read_ = did;
	}
	void enqueue(const uint8_t* data, int len);
	void form_send_packet(int cmd, uint8_t* payload, int len, tuint8data& result);

	void form_queryip_req(tuint8data& result);
	int parse_queryip_req(const uint8_t* data, int len);

	struct tqueryip_resp {
		int version;
		int af;
		union {
			uint32_t ipv4;
		} a;
		uint32_t flags;
	};
	void form_queryip_resp(const tqueryip_resp& src, tuint8data& result);
	tqueryip_resp parse_queryip_resp(const uint8_t* payload, int len);

	void form_wifilist_resp_ipv4(uint32_t ip, uint32_t flags, SDL_WifiScanResult* results, int count, tuint8data& out);
	void form_connectwifi_req(const std::string& ssid, const std::string& password, tuint8data& result);
	std::string parse_connectwifi_req(const uint8_t* data, int len, std::string& password);

	void form_removewifi_req(const std::string& ssid, tuint8data& result);
	std::string parse_removewifi_req(const uint8_t* data, int len);

private:
	uint8_t* fill_mtu_4bytes(uint8_t* data, int cmd, int payload_len);
	void resize_recv_data(int size);
	void resize_packet_data(int size);

protected:
	uint8_t* recv_data_;
	int recv_data_size_;
	int recv_data_vsize_;
	std::function<void (int cmd, const uint8_t* data, int len)> did_read_;

	uint8_t* packet_data_;
	int packet_data_size_;

	std::set<int> recv_cmds_;
	std::set<int> send_cmds_;
};

#endif /* FREERDP_CHANNEL_LEAGOR_BLE_H */
