/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "ble.hpp"

#include <time.h>
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "SDL_mixer.h"

#include <iomanip>
using namespace std::placeholders;

#include <algorithm>

#include "base_instance.hpp"

static tble* singleten = nullptr;

static void blec_discover_peripheral(SDL_BlePeripheral* peripheral)
{
	singleten->did_discover_peripheral(*peripheral);
}

static void blec_release_peripheral(SDL_BlePeripheral* peripheral)
{
	singleten->did_release_peripheral(*peripheral); 
}

static void blec_connect_peripheral(SDL_BlePeripheral* peripheral, int error)
{
	singleten->did_connect_peripheral(*peripheral, error);
}

static void blec_disconnect_peripheral(SDL_BlePeripheral* peripheral, int error)
{
	singleten->did_disconnect_peripheral(*peripheral, error);
}
    
static void blec_discover_services(SDL_BlePeripheral* peripheral, int error)
{
	singleten->did_discover_services(*peripheral, error);
}

static void blec_discover_characteristics(SDL_BlePeripheral* peripheral, SDL_BleService* service, int error)
{
	singleten->did_discover_characteristics(*peripheral, *service, error);
}

static void blec_read_characteristic(SDL_BlePeripheral* device, SDL_BleCharacteristic* characteristic, const uint8_t* data, int len)
{
	singleten->did_read_characteristic(*device, *characteristic, data, len);
}

static void blec_write_characteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, int error)
{
	singleten->did_write_characteristic(*peripheral, *characteristic, error);
}
    
static void blec_notify_characteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, int error)
{
    singleten->did_notify_characteristic(*peripheral, *characteristic, error);
}

tsendbuf::~tsendbuf()
{
	if (send_data_) {
		free(send_data_);
		send_data_ = nullptr;
	}
/*
	if (packet_data_) {
		free(packet_data_);
		packet_data_ = nullptr;
	}
*/
}

void tsendbuf::enqueue(const uint8_t* data, int len)
{
	VALIDATE(data != nullptr && len > 0, null_str);

	SDL_Log("tsendbuf::enqueue---len: %i, send_data_vsize_: %i", len, send_data_vsize_);
	resize_send_data(send_data_vsize_ + len);
	memcpy(send_data_ + send_data_vsize_, data, len);
	send_data_vsize_ += len;

	if (!wait_notification_) {
		send_data_internal();
	}
}

bool tsendbuf::did_notification_sent(int error)
{
	wait_notification_ = false;
	if (send_data_vsize_ > 0) {
		if (error == 0) {
			send_data_internal();
		} else {
			// empty send data buf.
			send_data_vsize_ = 0;
		}
	}
	return wait_notification_;
}

void tsendbuf::resize_send_data(int size)
{
	size = posix_align_ceil(size, 4096);

	if (size > send_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (send_data_) {
			if (send_data_vsize_) {
				memcpy(tmp, send_data_, send_data_vsize_);
			}
			free(send_data_);
		}
		send_data_ = tmp;
		send_data_size_ = size;
	}
}

void tsendbuf::send_data_internal()
{
	VALIDATE(!wait_notification_, null_str);
	VALIDATE(send_data_vsize_ > 0, null_str);
	
	const int this_len = SDL_min(send_data_vsize_, mtu_size_);
	if (did_write_) {
		did_write_(send_data_, this_len);
	}

	if (send_data_vsize_ > this_len) {
		memcpy(send_data_, send_data_ + this_len, send_data_vsize_ - this_len);
	}
	send_data_vsize_ -= this_len;
	wait_notification_ = true;
}

bool tble::tdisable_reconnect_lock::require_reconnect = true;

tble::tstep::tstep(const std::string& service, const std::string& characteristic, int _option, const uint8_t* _data, const size_t _len)
	: service_uuid(service)
	, characteristic_uuid(characteristic)
	, option(_option)
	, data(NULL)
	, len(_len)
{

	if (option == option_write) {
		data = (uint8_t*)malloc(len);
		memcpy(data, _data, len);
	}
}

void tble::tstep::execute(tble& ble, SDL_BlePeripheral& peripheral) const
{
    SDL_Log("tstep::execute, task#%i, current_step_: %i, tble: 0x%p", ble.current_task_->id(), ble.current_step_, &ble);
    
	ble.app_task_callback(*ble.current_task_, ble.current_step_, true);

	SDL_BleCharacteristic* characteristic = SDL_BleFindCharacteristic(&peripheral, service_uuid.c_str(), characteristic_uuid.c_str());
	if (option == option_write) {
		std::map<SDL_BleCharacteristic*, tsendbuf>::iterator find_it = ble.send_bufs_.find(characteristic);
		if (find_it == ble.send_bufs_.end()) {
			std::pair<std::map<SDL_BleCharacteristic*, tsendbuf>::iterator, bool> ins = ble.send_bufs_.insert(std::make_pair(characteristic, tsendbuf()));
			ins.first->second.set_did_write(std::bind(&tble::do_write_characteristic, &ble, &peripheral, characteristic, _1, _2));
			find_it = ins.first;
		}
		find_it->second.enqueue(data, len);
		// SDL_BleWriteCharacteristic(&peripheral, characteristic, data, len);

    } else if (option == option_notify) {
        SDL_BleNotifyCharacteristic(&peripheral, characteristic, SDL_TRUE);
    }
	  SDL_Log("---tstep::execute, task#%i, current_step_: %i, tble: 0x%p", ble.current_task_->id(), ble.current_step_, &ble);
}

void tble::ttask::insert(int at, const std::string& service, const std::string& characteristic, int option, const uint8_t* data, const int len)
{
	VALIDATE(at == nposm, null_str);

	steps_.push_back(tstep(service, characteristic, option, data, len));
}

void tble::ttask::execute(tble& ble)
{
	VALIDATE(ble.peripheral_, null_str);
	VALIDATE(ble.current_task_ == nullptr, null_str);

	SDL_BlePeripheral& peripheral = *ble.peripheral_;
	ble.current_task_ = this;
	ble.current_step_ = 0;

	const tstep& step = steps_.front();
	if (ble.make_characteristic_exist(step.service_uuid, step.characteristic_uuid)) {
		step.execute(ble, peripheral);
	}
}

const unsigned char tble::null_mac_addr[SDL_BLE_MAC_ADDR_BYTES] = {0};
std::string tble::peripheral_name(const SDL_BlePeripheral& peripheral, bool adjust)
{
	if (peripheral.name != nullptr && peripheral.name[0] != '\0') {
		return peripheral.name;
	}
	if (!adjust) {
		return null_str;
	}
	const std::string null_name = "<null>";
	return null_name;
}

std::string tble::peripheral_name2(const std::string& name)
{
	if (!name.empty()) {
		return name;
	}
	const std::string null_name = "<null>";
	return null_name;
}

std::string tble::get_full_uuid(const std::string& uuid)
{
	const int full_size = 36; // 32 + 4

	int size = uuid.size();
	if (size == full_size) {
		return uuid;
	}
	if (size != 4) {
		return null_str;
	}
	return std::string("0000") + uuid + "-0000-1000-8000-00805f9b34fb";
}

tble::tble(base_instance* instance, tconnector& connector)
	: connecting_peripheral_(nullptr)
	, peripheral_(nullptr)
	, disconnecting_(false)
	, discover_connect_(false)
	, nullptr_connecting_peripheral_ticks_(0)
	, persist_scan_(false)
	, disable_reconnect_(false)
	, current_task_(nullptr)
	, disconnect_threshold_(60000)
	, disconnect_music_("error.ogg")
	, background_scan_ticks_(0)
	, background_id_(INVALID_UINT32_ID)
	, connector_(&connector)
{
	singleten = this;
	VALIDATE(instance->ble() == nullptr, null_str);
	instance->set_ble(this);

	memset(&callbacks_, 0, sizeof(SDL_BleCallbacks));

	callbacks_.discover_peripheral = blec_discover_peripheral;
	callbacks_.release_peripheral = blec_release_peripheral;
	callbacks_.connect_peripheral = blec_connect_peripheral;
    callbacks_.disconnect_peripheral = blec_disconnect_peripheral;
    callbacks_.discover_services = blec_discover_services;
    callbacks_.discover_characteristics = blec_discover_characteristics;
	callbacks_.read_characteristic = blec_read_characteristic;
	callbacks_.write_characteristic = blec_write_characteristic;
	callbacks_.notify_characteristic = blec_notify_characteristic;
	SDL_BleSetCallbacks(&callbacks_);
}

tble::~tble()
{
	SDL_Log("tble::~tble()---");
	if (peripheral_ != nullptr && !disconnecting_) {
		// normally, wml_exception reuslt it.
		SDL_Log("maybe wml_exception reuslt it, call disconnect explicitly");
		disconnect_peripheral();
		peripheral_ = nullptr;
	}

	SDL_BleSetCallbacks(NULL);

	singleten = nullptr;
	if (instance != nullptr) {
		VALIDATE(instance->ble(), null_str);
		instance->set_ble(nullptr);
	}
	SDL_Log("tble::~tble()---");
}

void tble::enable_discover_connect(bool enable)
{
	VALIDATE(connecting_peripheral_ == nullptr && peripheral_ == nullptr, null_str);
	if (!enable) {
		connector_->clear();
	}
	discover_connect_ = enable;
}

tble::ttask& tble::insert_task(int id)
{
	for (std::vector<ttask>::const_iterator it = tasks_.begin(); it != tasks_.end(); ++ it) {
		const ttask& task = *it;
		VALIDATE(task.id() != id, null_str);
	}

	tasks_.push_back(ttask(id));
	return tasks_.back();
}

void tble::clear_background_connect_flag()
{
	background_scan_ticks_ = 0;
	background_id_ = INVALID_UINT32_ID;
}

// true: start reconnect detect when background.
// false: stop detect.
void tble::background_reconnect_detect(bool start)
{
	if (start) {
        SDL_Log("background_reconnect_detect, start");
		background_scan_ticks_ = SDL_GetTicks() + disconnect_threshold_;
        VALIDATE(background_id_ == INVALID_UINT32_ID, null_str);
        background_id_ = instance->background_connect(std::bind(&tble::background_callback, this, _1));

	} else {
        SDL_Log("background_reconnect_detect, stop, background_scan_ticks_: %u", background_scan_ticks_);
		clear_background_connect_flag();
		if (peripheral_ && !SDL_strcasecmp(sound::current_music_file().c_str(), disconnect_music_.c_str())) {
            SDL_Log("background_reconnect_detect, will stop music");
			sound::stop_music();
		}
	}
}

bool tble::background_callback(uint32_t ticks)
{
	// set bachournd_id_ and background_scan_ticks_ is in other thread, to avoid race, use tow judge together.
	if (background_id_ == INVALID_UINT32_ID && background_scan_ticks_ == 0) {
		// connect peripherah returned, has stop detect.
		return true;

	} else if (background_scan_ticks_ && ticks >= background_scan_ticks_) {
		if (!Mix_PlayingMusic()) {
			sound::play_music_repeatedly(disconnect_music_);
		}
        VALIDATE(background_id_ != INVALID_UINT32_ID, null_str);
        clear_background_connect_flag();
        return true;
	}
	return false;
}

// true: foreground-->background
// false: background-->foreground
void tble::handle_app_event(bool background)
{
	if (!background) {
		clear_background_connect_flag();
	}

	// if don't in connecting or connected, start_scan again.
	if (game_config::os == os_ios && !connecting_peripheral_ && !peripheral_) {
		// First remark it, ble module will be improved in the next version. We'll decide what to do then.
		// start_scan(); // requrie fix!
	}
}

void tble::start_scan()
{
    std::vector<std::string> v;
	const char* uuid = NULL;
	if (!instance->foreground() && mac_addr_.valid()) {
		// Service UUID of advertisement packet maybe serval UUID, Now I only pass first.
		v = utils::split(mac_addr_.uuid.c_str());
		if (!v.empty()) {
			uuid = v.front().c_str();
		}
		if (!connecting_peripheral_ && !peripheral_) {
            // even if in reconnect detect, app maybe call start_scan again.
            if (background_id_ == INVALID_UINT32_ID) {
                background_reconnect_detect(true);
            }
		}
	}

    SDL_Log("------tble::start_scan------uuid: %s", uuid? uuid: "<nil>");
	SDL_BleScanPeripherals(uuid);
}

void tble::stop_scan()
{
    SDL_Log("------tble::stop_scan------");
	SDL_BleStopScanPeripherals();
}

void tble::start_advertise()
{
	SDL_BleStartAdvertise();
}

void tble::connect_peripheral(SDL_BlePeripheral& peripheral)
{
	VALIDATE_IN_MAIN_THREAD();
	SDL_Log("tble::connect_peripheral--- name: %s", peripheral.name);

	VALIDATE(connecting_peripheral_ == nullptr, null_str);
	VALIDATE(peripheral_ == nullptr, null_str);
	VALIDATE(peripheral.services == nullptr && peripheral.valid_services == 0, null_str);
	VALIDATE(send_bufs_.empty(), null_str);

	// some windows os, connect is synchronous with did. set connecting_peripheral_ before connect.
	const int connect_threshold = 10 * 1000; // 10 second
	nullptr_connecting_peripheral_ticks_ = SDL_GetTicks() + connect_threshold;
	connecting_peripheral_ = &peripheral;

	// To reduce traffic on the Bluetooth bus, stop the scan first.
	if (!persist_scan_) {
		stop_scan();
	}

	SDL_BleConnectPeripheral(&peripheral);
}

void tble::disconnect_peripheral()
{
	SDL_Log("tble::disconnect_peripheral--- peripheral_: 0x%p, connecting_peripheral_: 0x%p", peripheral_, connecting_peripheral_);

	SDL_BlePeripheral* peripheral = connecting_peripheral_ != nullptr? connecting_peripheral_: peripheral_;
	if (connecting_peripheral_ != nullptr) {
        connecting_peripheral_ = nullptr;
    }
	if (peripheral != nullptr) {
		disconnecting_ = true;
		SDL_BleDisconnectPeripheral(peripheral);
    }
}

void tble::do_write_characteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const uint8_t* data, int len)
{
	SDL_BleWriteCharacteristic(peripheral, characteristic, data, len);
}

void tble::did_discover_peripheral(SDL_BlePeripheral& peripheral)
{
	const std::string str = rtc::hex_encode((const char*)peripheral.manufacturer_data, peripheral.manufacturer_data_len);
	SDL_Log("tble::did_discover_peripheral--- name: %s, connecting_peripheral_: 0x%p, peripheral_: 0x%p, connector_: %s, manufacturer_data: %s", 
		peripheral.name, connecting_peripheral_, peripheral_, connector_->valid()? "true": "false", str.c_str());
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
    // iOS cannot get macaddr normally. Call it to use private method to get it.
    app_calculate_mac_addr(peripheral);
#endif
	connector_->verbose("did_discover");
    
	if (!connecting_peripheral_ && !peripheral_ && connector_->match(peripheral)) {
		// tble think there are tow connect requirement.
		// one is app call connect_peripheral manuly. 
		// the other is at here. reconnect automaticly when discover.
		connect_peripheral(peripheral);
	}
	app_discover_peripheral(peripheral);
}

void tble::did_release_peripheral(SDL_BlePeripheral& peripheral) 
{
	// 1. !peripheral_. now no connected peripheral.
	// 2. peripheral_ != &peripheral. there is connected peripheral, not myself.
	// peripheral_ == &peripheral. must not, can not occur!
	VALIDATE(!peripheral_ || peripheral_ != &peripheral, null_str);

	SDL_Log("tble::did_release_peripheral, name: %s", peripheral.name);

    bool connecting = connecting_peripheral_ == &peripheral;
    if (connecting) {
		// Connect the ble peripheral required for a period of time, user release peripheral during this period. 
		// it is different from did_disconnect_peripheral. if disconnect one existed connection, did_disconnect_peripheral should be called before.
        connecting_peripheral_ = nullptr;
    }
	app_release_peripheral(peripheral);
}

void tble::did_connect_peripheral(SDL_BlePeripheral& peripheral, int error)
{
	SDL_Log("tble::did_connect_peripheral, name: %s, connecting_peripheral_: %p, error: %i, persist_scan_: %s", peripheral.name, connecting_peripheral_, error, persist_scan_? "true": "false");

	if (connecting_peripheral_ != nullptr) {
		connecting_peripheral_ = nullptr;
		if (!error) {
			peripheral_ = &peripheral;
/*
			if (!persist_scan_) {
				stop_scan();
			}
*/
			bool enable_delay = false;
			if (enable_delay) {
				int ii = 0;
				SDL_Delay(5000);
				SDL_Log("tble::did_connect_peripheral, for debug, SDL_Delay(5000)");
			} else {
				SDL_Log("tble::did_connect_peripheral, for debug, not SDL_Delay(5000)");
			}

			// After connection is successful, call SDL_BleGetServices.
			// when get the services, then call app's app_connect_peripheral.
			SDL_BleGetServices(peripheral_);
		} else {
			app_connect_peripheral(peripheral, bleerr_connect);
		}

	} else if (!error) { 
		// app call connect_peripheral, and call disconnect_peripheral at once.
		// step1.
		SDL_Log("tble::did_connect_peripheral, name: %s, (connecting_peripheral_ == nullptr && error == 0), call SDL_BleDisconnectPeripheral", peripheral.name);
		VALIDATE(peripheral_ == nullptr, null_str);
		disconnect_peripheral();
	}

	background_reconnect_detect(false);
}

void tble::did_disconnect_peripheral(SDL_BlePeripheral& peripheral, int error)
{
	SDL_Log("tble::did_disconnect_peripheral, name: %s", peripheral.name);

	disconnecting_ = false;
	if (peripheral_) {
		if (current_task_ != nullptr) {
			current_task_ = nullptr;
		}
		VALIDATE(peripheral_ == &peripheral, null_str);
		peripheral_ = nullptr;
		app_disconnect_peripheral(peripheral, error);
		send_bufs_.clear();

	} else if (!error) {
		// app call connect_peripheral, and call disconnect_peripheral at once.
		// step2.
		VALIDATE(!connecting_peripheral_, null_str);
	}

	// restart scan, persist_scan_ whether or not.
	if (!disable_reconnect_) {
		SDL_Log("tble::did_disconnect_peripheral, start_scan");
		start_scan();
	}
}

void tble::did_discover_services(SDL_BlePeripheral& peripheral, int error)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(peripheral_ == &peripheral, null_str);

	int ecode = bleerr_getservices;
	if (error == 0) {
		if (is_right_services()) {
			if (discover_connect_) {
				connector_->evaluate(peripheral);
			}
			ecode = bleerr_ok;
		} else {
			SDL_BleClearCache(&peripheral);
			ecode = bleerr_errorservices;
			// isn't my services, disconnect it.
			disconnect_peripheral();
		}
	}
	app_connect_peripheral(peripheral, ecode);
}

void tble::did_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, int error)
{
	VALIDATE(service.valid_characteristics >= 1, null_str);

	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		VALIDATE(make_characteristic_exist(step.service_uuid, step.characteristic_uuid), null_str);
		step.execute(*this, peripheral);
		return;
	}
	app_discover_characteristics(peripheral, service, error);
}

void tble::did_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len)
{
	SDL_Log("tble::did_read_characteristic(%s), current_task: %i, current_step_: %i, len: %i", characteristic.uuid, current_task_ != nullptr? current_task_->id(): nposm, current_step_, len);
	if (disconnecting_) {
		SDL_Log("tble::did_read_characteristic(%s), in disconnecting, do nothing", characteristic.uuid);
		return;
	}
/*
	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (SDL_BleUuidEqual(characteristic.uuid, step.characteristic_uuid.c_str()) && step.option == option_read) {
			task_next_step(peripheral);
			return;
		}
	}
*/
	app_read_characteristic(peripheral, characteristic, data, len);
}

void tble::did_write_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, int error)
{
    SDL_Log("tble::did_write_characteristic(%s), current_task: %i, current_step_: %i, 0x%p", characteristic.uuid, current_task_ != nullptr? current_task_->id(): nposm, current_step_, this);
    
	std::map<SDL_BleCharacteristic*, tsendbuf>::iterator find_it = send_bufs_.find(&characteristic);
	if (find_it != send_bufs_.end()) {
		bool waiting = find_it->second.did_notification_sent(error);
		if (waiting) {
			// require write continus.
			return;
		}
	}

	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (SDL_BleUuidEqual(characteristic.uuid, current_task_->get_step(current_step_).characteristic_uuid.c_str()) && step.option == option_write) {
			task_next_step(peripheral);
			return;
		}
	}
	app_write_characteristic(peripheral, characteristic, error);
}

void tble::did_notify_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, int error)
{
	SDL_Log("tble::did_notify_characteristic(%s), current_task: %i, current_step_: %i", characteristic.uuid, current_task_ != nullptr? current_task_->id(): nposm, current_step_);

	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (SDL_BleUuidEqual(characteristic.uuid, current_task_->get_step(current_step_).characteristic_uuid.c_str())  && step.option == option_notify) {
			task_next_step(peripheral);
			return;
		}
	}
	app_notify_characteristic(peripheral, characteristic, error);
}

tble::ttask& tble::get_task(int id)
{
	for (std::vector<ttask>::iterator it = tasks_.begin(); it != tasks_.end(); ++ it) {
		ttask& task = *it;
		if (task.id() == id) {
			return task;
		}
	}

	VALIDATE(false, null_str);
	return tasks_.front();
}

bool tble::make_characteristic_exist(const std::string& service_uuid, const std::string& characteristic_uuid)
{
	VALIDATE(peripheral_ != nullptr, null_str);
	VALIDATE(peripheral_->valid_services > 0, null_str);	
	SDL_BleService* service = SDL_BleFindService(peripheral_, service_uuid.c_str());
        
    std::stringstream err;
    err << "can not file service: " << service_uuid;
	if (!service) {
		SDL_Log("make_characteristic_exist, %s", err.str().c_str());
	}
    VALIDATE(service, err.str());
	if (!service->valid_characteristics) {
		SDL_BleGetCharacteristics(peripheral_, service);
		return false;
	}
	return true;
}

void tble::task_next_step(SDL_BlePeripheral& peripheral)
{
    SDL_Log("tble::task_next_step, current_task: %i, finished_step_: %i", current_task_->id(), current_step_);
    
	ttask& task = * current_task_;
	// Set current_task_(nullptr) first, so that app_task_callback can perform new tasks.
	current_task_ = nullptr;

	bool conti = app_task_callback(task, current_step_, false);
	if (current_task_ != nullptr) {
		// app_task_callback has started new task.
		SDL_Log("tble::task_next_step, app_task_callback has started new task, current_task: %i, current_step_: %i", current_task_->id(), current_step_);
		return;
	}
	if (!conti || ++ current_step_ == task.steps().size()) {
		return;
	}
	// set current_task_ again
	current_task_ = &task;

    const tstep& step = current_task_->get_step(current_step_);
    if (make_characteristic_exist(step.service_uuid, step.characteristic_uuid)) {
        current_task_->get_step(current_step_).execute(*this, peripheral);
    }
}

void tble::pump()
{
	if (connecting_peripheral_ != nullptr && SDL_GetTicks() >= nullptr_connecting_peripheral_ticks_) {
		disconnect_peripheral();
	}
}

std::string mac_addr_uc6_2_str(const unsigned char* uc6)
{
	std::stringstream ss;
	for (int i = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
        ss << std::setfill('0') << std::setw(2) << std::setbase(16) << (int)(uc6[i]);
    }
	return utils::uppercase(ss.str());
}

std::string mac_addr_uc6_2_str2(const unsigned char* uc6)
{
	std::stringstream ss;
	for (int i = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
		if (!ss.str().empty()) {
			ss << ":";
		}
        ss << std::setfill('0') << std::setw(2) << std::setbase(16) << (int)(uc6[i]);
    }
	return utils::uppercase(ss.str());
}

void mac_addr_str_2_uc6(const char* str, uint8_t* uc6, const char separator)
{
	char value_str[3];
	int i, at;
	int should_len = SDL_BLE_MAC_ADDR_BYTES * 2 + (separator? SDL_BLE_MAC_ADDR_BYTES - 1: 0);
	if (!str || SDL_strlen(str) != should_len) {
		SDL_memset(uc6, 0, SDL_BLE_MAC_ADDR_BYTES);
		return;
	}

	value_str[2] = '\0';
	for (i = 0, at = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
		value_str[0] = str[at ++];
		value_str[1] = str[at ++];
		uc6[i] = SDL_strtol(value_str, NULL, 16);
		if (separator && i != SDL_BLE_MAC_ADDR_BYTES - 1) {
			if (str[at] != separator) {
				SDL_memset(uc6, 0, SDL_BLE_MAC_ADDR_BYTES);
				return;
			}
			at ++;
		}
	}
}

bool mac_addr_valid(const unsigned char* uc6)
{
	for (int i = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
		if (uc6[i]) {
			return true;
		}
	}
	return false;
}

//
// use as peripheral
//
static tpble* pble_singleten = nullptr;

static void pblec_advertising_started(int error)
{
	pble_singleten->did_advertising_started(error);
}

static void pblec_center_connected(const char* name, uint8_t* macaddr, int error)
{
	pble_singleten->did_center_connected(name != nullptr? name: null_str, macaddr, error);
}

static void pblec_center_disconnected()
{
	pble_singleten->did_center_disconnected();
}

static void pblec_read_characteristic(const char* characteristic, const uint8_t* data, int len)
{
	pble_singleten->did_read_characteristic(characteristic, data, len);
}

static void pblec_write_characteristic(const char* characteristic, int error)
{
	pble_singleten->did_write_characteristic(characteristic, error);
}

static void pblec_notify_characteristic(const char* characteristic, int error)
{
	pble_singleten->did_notify_characteristic(characteristic, error);
}

static void pblec_notification_sent(const char* characteristic, int error)
{
	pble_singleten->did_notification_sent(characteristic, error);
}

tpble::tpble(base_instance* instance)
{
	pble_singleten = this;
	memset(&callbacks_, 0, sizeof(SDL_BleCallbacks));

	callbacks_.advertising_started = pblec_advertising_started;
	callbacks_.center_connected = pblec_center_connected;
    callbacks_.center_disconnected = pblec_center_disconnected;
	callbacks_.read_characteristic = pblec_read_characteristic;
	callbacks_.write_characteristic = pblec_write_characteristic;
	callbacks_.notify_characteristic = pblec_notify_characteristic;
	callbacks_.notification_sent = pblec_notification_sent;
	SDL_PBleSetCallbacks(&callbacks_);
}

tpble::~tpble()
{
	pble_singleten = nullptr;
}

void tpble::start_advertising(const std::string& name, int manufacturer_id, const uint8_t* manufacturer_data, int manufacturer_data_size)
{
	SDL_PBleStartAdvertising(name.c_str(), manufacturer_id, manufacturer_data, manufacturer_data_size);
}

void tpble::stop_advertising()
{
	SDL_PBleStopAdvertising();
}

bool tpble::is_advertising() const
{
	SDL_bool ret = SDL_PBleIsAdvertising();
	return ret? true: false;
}

bool tpble::is_connected() const
{
	return mac_addr_.valid();
}

void tpble::write_characteristic(const std::string& uuid, const uint8_t* data, int size)
{
	VALIDATE(!uuid.empty(), null_str);
	VALIDATE(data != nullptr && size > 0, null_str);

	SDL_PBleWriteCharacteristic(uuid.c_str(), data, size);
}

void tpble::did_advertising_started(int error)
{
	SDL_Log("tpble::did_advertising_started--- error: %i", error);
}

void tpble::did_center_connected(const std::string& name, const uint8_t* macaddr, int error)
{
	VALIDATE(send_bufs_.empty(), null_str);
	const std::string str = rtc::hex_encode((const char*)macaddr, macaddr != nullptr? SDL_BLE_MAC_ADDR_BYTES: 0);
	SDL_Log("tpble::did_center_connected--- name: %s, macaddr: %s, error: %i", name.c_str(), str.c_str(), error);
	if (error == 0) {
		mac_addr_.set(macaddr, null_str);
		VALIDATE(mac_addr_.valid(), null_str);
	}
	app_center_connected(mac_addr_, error);
}

void tpble::did_center_disconnected()
{
	SDL_Log("tpble::did_center_disconnected---");
	app_center_disconnected(mac_addr_);
	mac_addr_.clear();
	send_bufs_.clear();
}

void tpble::did_read_characteristic(const std::string& characteristic, const uint8_t* data, int len)
{
	SDL_Log("tpble::did_read_characteristic(%s), len: %i", characteristic.c_str(), len);
	app_read_characteristic(characteristic, data, len);
}

void tpble::did_write_characteristic(const std::string& characteristic, int error)
{
	SDL_Log("tpble::did_write_characteristic(%s)--- error: %i", characteristic.c_str(), error);
}

void tpble::did_notify_characteristic(const std::string& characteristic, int error)
{
	SDL_Log("tpble::did_notify_characteristic(%s)--- error: %i", characteristic.c_str(), error);
}

void tpble::did_notification_sent(const std::string& characteristic, int error)
{
	SDL_Log("tpble::did_notification_sent(%s)--- error: %i", characteristic.c_str(), error);
	app_notification_sent(characteristic, error);
}
