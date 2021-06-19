#define GETTEXT_DOMAIN "rose-lib"

#include "aplt_net.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"
#include "version.hpp"
#include "base_instance.hpp"
#include "SDL_image.h"

#include <net/base/port_util.h>
#include <net/base/upload_data_stream.h>
#include <iomanip>
#include <openssl/sha.h>

#include "minizip/minizip.hpp"

#define ERRCODE_LOW_VERSION		-2
#define ERRCODE_NO_MOBILE		-3
#define ERRCODE_PASSWORD_ERROR	-7
#define ERRCODE_PERSON_EXISTED	-8
#define ERRCODE_REMOTE_LOGIN	-9

#define TIMEOUT_NORMAL	5000 // 3500
#define TIMEOUT_6S		6000
#define TIMEOUT_10S		10000
#define TIMEOUT_15S		15000
#define TIMEOUT_30S		30000
#define TIMEOUT_45S		45000

// const tuser null_user2;
// tuser current_user;

static int invalid_faceid_in_villageserver = 0;

namespace net {
// int agbox_minus_local = 0;

static std::string generic_handle_response(const int code, Json::Value& json_object, bool& handled)
{
	std::stringstream err;
	utils::string_map symbols;
	
	handled = false;
/*
	if (code == ERRCODE_NO_MOBILE) {
		err << _("Mobile isn't existed");

	} else if (code == ERRCODE_REMOTE_LOGIN) {
		Json::Value& timestamp = json_object["timestamp"];

		symbols["time"] = format_time_date(timestamp.asInt64());
		err << vgettext2("Your account is logined at $time in different places. If it is not your operation, your password has been leaked. Please as soon as possible to modify the password.", symbols);
		current_user.sessionid.clear();

		handled = true;

	} else */ if (code == ERRCODE_PASSWORD_ERROR) {
		err << _("Password error");
		handled = true;

	} else if (code == ERRCODE_LOW_VERSION) {
		symbols["app"] = game_config::get_app_msgstr(null_str);
		err << vgettext2("The version is too low, please upgrade $app.", symbols);
		handled = true;
		// as far not login, so sessionid is invalid. can not do_reporterror.
	}

	return err.str();
}


static std::string form_url(const std::string& category, const std::string& task)
{
	std::stringstream url;

	url << "http://" << game_config::bbs_server.host;
	url << ":" << game_config::bbs_server.port;
	url << game_config::bbs_server.url << category << "/";
	url << task;

	return url.str();
}

static std::string form_url2(const std::string& subpath)
{
	std::stringstream url;

	url << "http://" << "www.cswamp.com";
	url << ":" << 80;
	url << "/" << subpath;

	return url.str();
}
/*
bool did_pre_login(net::HttpRequestHeaders& headers, std::string& body, const std::string& devicecode, const std::string& password)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["devicecode"] = devicecode;
	json_root["password"] = password;

	std::stringstream sn_ss;
	sn_ss << game_config::sn;
	if (game_config::android_os == aos_leagor) {
		SDL_OsInfo osinfo;
		SDL_GetOsInfo(&osinfo);
		const char* ptr = strstr(osinfo.display, "eng.leagor.");
		VALIDATE(ptr != nullptr, null_str);
		std::string str = ptr + strlen("eng.leagor.");
		sn_ss << "-k" << str.substr(0, str.find("."));

	} else if (game_config::os == os_windows) {
		// sn_ss << "-k20210422";
	}
	json_root["sn"] = sn_ss.str();
	json_root["type"] = dtype_int_2_str(game_config::dtype, game_config::access_type, game_config::phone_type);
	json_root["reboot"] = game_config::startup_time;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

int parse_cardtype(const std::string& str)
{
	const std::string str2 = utils::lowercase(str);
	if (str2 == "wg34" || str2 == "wg34be") {
		return cardtype_wg34be;

	} else if (str2 == "wg34le") {
		return cardtype_wg34le;

	} else if (str2 == "wg26be") {
		return cardtype_wg26be;

	} else if (str2 == "wg26le") {
		return cardtype_wg26le;

	} else if (str2 == "wg32be") {
		return cardtype_wg32be;
	}
	return cardtype_wg32le;
}

int parse_scene(const std::string& str)
{
	const std::string str2 = utils::lowercase(str);
	if (str2 == "company") {
		return scene_company;
	}
	return scene_village;
}

std::string scene_str(int scene)
{
	if (scene == scene_village) {
		return "village";
	} else if (scene == scene_company) {
		return "company";
	}
	VALIDATE(false, "Unknown scene");
	return null_str;
}

int parse_openmethod(const std::string& str)
{
	const std::string str2 = utils::lowercase(str);
	if (str2 == "wiegang") {
		return openmethod_wiegang;

	} else if (str2 == "armbus") {
		return openmethod_armbus;

	} else if (str2 == "armbuswiegang") {
		return openmethod_armbuswiegang;
	}
	return openmethod_internalrelay;
}

std::vector<trtsp_settings> parse_rtsp_string2(const std::string& camera_str, bool check_0, std::string& err)
{
	std::vector<trtsp_settings> rtsps = utils::parse_rtsp_string(camera_str, check_0, true);
	err.clear();
	if (rtsps.empty()) {
		err = _("Invalid 'camera' value. Go 'device management' to modify it.");
	} else {
		int at = 0;
		for (std::vector<trtsp_settings>::const_iterator it = rtsps.begin(); it != rtsps.end(); ++ it, at ++) {
			const trtsp_settings& settings = *it;
			if (settings.name != "in" && settings.name != "out") {
				err = _("Invalid 'camera' value. name must in or out. Go 'device management' to modify it.");
			} else if (!settings.tcp) {
				err = _("Invalid 'camera' value. current support tcp only. Go 'device management' to modify it.");
			}
			if (at == 0 && !check_0) {
				if (!settings.url.empty()) {
					err = _("Invalid 'camera' value. To AIO, first camera's url must be empty. Go 'device management' to modify it.");
				}
			}
		}
	}
	if (!err.empty()) {
		rtsps.clear();
	}
	return rtsps;
}

std::string parse_camera_str(const std::string& le102position)
{
	current_user.camera.clear();
	if (!dtype_require_login()) {
		return null_str;
	}

	std::string adjusted_camera_str = current_user.camera_str;
	std::string err;

	if (current_user.serverversion < version_info("1.0.3")) {
		current_user.camera.positions = std::vector<std::string>(1, le102position);
		current_user.camera.valid_rects = std::vector<SDL_Point>(1, SDL_Point{100, 100});

	} else {
		std::vector<std::string> branchs = utils::split(current_user.camera_str, ';', 0);
		std::stringstream new_camera_str;
		if (branchs.size() >= 3) {
			const int fields_per_camera = utils::to_int(branchs[1]);
			if (fields_per_camera < 7) {
				// field server's version may increase field.
				err = "fields per camera must >= 7";
			}
			std::string ride_cam_type_str;
			if (err.empty()) {
				ride_cam_type_str = current_user.camera_str.substr(branchs[0].size() + 1 + branchs[1].size() + 1);
			}
			std::vector<std::string> cameras = utils::split(ride_cam_type_str, ';', 0);
			for (std::vector<std::string>::const_iterator it = cameras.begin(); it != cameras.end(); ++ it) {
				const std::string& camera = *it;
				std::vector<std::string> fields = utils::split(camera, ',', utils::STRIP_SPACES);
				if (fields.size() != fields_per_camera) {
					err = "one cameras must have 7 fields";
					break;
				}
				const std::string& position = fields[0];
				if (!utils::is_guid(position, true)) {
					err = "postion must be guid format";
					break;
				}
				current_user.camera.positions.push_back(position);
				int valid_width = utils::to_int(fields[1]);
				if (valid_width < MIN_VALIDSIZE || valid_width > MAX_VALIDSIZE) {
					err = "valid-width isn't valid";
					break;
				}
				int valid_height = utils::to_int(fields[2]);
				if (valid_height < MIN_VALIDSIZE || valid_height > MAX_VALIDSIZE) {
					err = "valid-height isn't valid";
					break;
				}
				current_user.camera.valid_rects.push_back(SDL_Point{valid_width, valid_height});
				current_user.camera.lifttypes.push_back(parse_lifttype(fields[3]));

				if (!new_camera_str.str().empty()) {
					new_camera_str << ",";
				}
				size_t offset = 0;
				const int reserve_post_fields = 3;
				for (int n = 0; n < fields_per_camera - reserve_post_fields; n ++) {
					offset += fields[n].size() + 1;
				}
				new_camera_str << camera.substr(offset);
			}
		} else {
			err = "camera's branchs must >= 3";
		}
		if (err.empty()) {
			adjusted_camera_str = new_camera_str.str();
			// if all is ',' ==> it is split-usb, make it empty
			bool all_is_comma = true;
			const char* c_str = adjusted_camera_str.c_str();
			int size = adjusted_camera_str.size();
			for (int at = 0; at < size; at ++) {
				if (c_str[at] != ',') {
					all_is_comma = false;
					break;
				}
			}
			if (all_is_comma) {
				adjusted_camera_str.clear();
			}
		}
	}

	if (err.empty() && game_config::dtype == dtype_access && !adjusted_camera_str.empty()) {
		current_user.camera.rtsps = net::parse_rtsp_string2(adjusted_camera_str, game_config::access_type != access_aio, err);
		if (game_config::access_type == access_aio) {
			VALIDATE(current_user.camera.rtsps.size() <= 2, _("AIO supports up to one network camera"));
		}
	}
	if (!err.empty()) {
		// must not save invalid-camera to preference. clear it.
		current_user.camera_str.clear();
		if (current_user.serverversion >= version_info("1.0.3")) {
			current_user.camera.clear();
		}
	}
	return err;
}

bool did_post_login(const net::RoseDelegate& delegate, int status, const std::string& devicecode, tslot* slot_ptr, bool quiet, std::string& invalid_field_msg, int64_t* server_time)
{
	VALIDATE(invalid_field_msg.empty(), null_str);
	std::stringstream err;

	if (status != net::OK) {
		if (!quiet) {
			err << net::err_2_description(status);
			gui2::show_message(null_str, std::string("login[1]: ") + err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	int code = -9999;
	try {
		Json::Reader reader;
		Json::Value json_object;
			
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "login, Unknown error. code: " << code;
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& sessionid = results["sessionid"];

						current_user.devicecode = devicecode;
						current_user.sessionid = sessionid.asString();
						current_user.serverversion = results["serverversion"].asString();
						tcs_car::split_char = current_user.serverversion >= version_info("0.9.9")? '&': ',';

						current_user.keepalive = results["keepalive"].asInt();
						current_user.showname = results["showname"].asBool();
						current_user.persons = results["persons"].asInt();
						VALIDATE(current_user.persons >= 0, null_str);
						current_user.liveness = results["liveness"].asBool();
						if (results["openmethod"].isString()) {
							current_user.openmethod = parse_openmethod(results["openmethod"].asString());
						} else {
							current_user.openmethod = results["externalcardstore"].asBool()? openmethod_wiegang: openmethod_internalrelay;
						}
						current_user.opentime = results["opentime"].asInt();
						current_user.comparethreshold = results["comparethreshold"].asInt();
						current_user.areathreshold = results["areathreshold"].asInt();
						current_user.reboottime = results["reboottime"].asInt();
						if (current_user.serverversion >= version_info("1.0.3")) {
							current_user.eventmasktime_ms = results["eventmasktime"].asInt() * 1000;
							current_user.alerttemperature = results["alerttemperature"].asInt();
							current_user.feverpass = results["feverpass"].asBool();
						} else {
							current_user.eventmasktime_ms = DEFAULT_EVENTMASKTIME_S * 1000;
							current_user.alerttemperature = DEFAULT_ALERTTEMPERATURE;
							current_user.feverpass = false;
						}
						if (current_user.serverversion >= version_info("1.0.5")) {
							current_user.saveimage = results["saveimage"].asBool();
						} else {
							current_user.saveimage = true;
						}
						current_user.novideorebootthreshold = results["novideorebootthreshold"].asInt();
						current_user.update_version = results["update"].asString();
						current_user.scene = parse_scene(results["scene"].asString());
						current_user.oem = parse_oem(results["oem"].asString());

						current_user.camera_str = results["camera"].asString();
						current_user.serverflags = results["flags"].asUInt();
						current_user.virtualcard = utils::to_uint64(results["virtualcard"].asString());
						int cardtype = parse_cardtype(results["cardtype"].asString());
						if (current_user.serverversion < version_info("0.9.9") && (current_user.oem & oem_wgle)) {
							if (cardtype == cardtype_wg34be) {
								cardtype = cardtype_wg34le;

							} else if (cardtype == cardtype_wg26be) {
								cardtype = cardtype_wg26le;
							} 
						}
						current_user.cardtype = cardtype;

						// person version
						current_user.personversion = parse_personversion(results["personversion"].asInt());
						if (current_user.serverversion < version_info("0.9.9")) {
							VALIDATE(current_user.personversion == personversion_v0, null_str);
						} else {
							VALIDATE(current_user.personversion != personversion_v0, null_str);
							VALIDATE(game_config::personversions.find(current_user.personversion) != game_config::personversions.end(), null_str);
						}

						// feature size
						int feature_size = results["featuresize"].asInt();
						if (current_user.serverversion < version_info("0.9.9")) {
							VALIDATE(feature_size == 0, null_str);
							feature_size = DEFAULT_FEATURE_BLOB_SIZE;
						} else {
							VALIDATE(feature_size != 0, null_str);
						}
						VALIDATE(game_config::valid_feature_blob_sizes.count(feature_size) != 0, null_str);
						FEATURE_BLOB_SIZE = feature_size;

						game_config::blacklist_persontype = current_user.oem & oem_3s? BLACKLIST_PERSONTYPE: nposm;

						*server_time = results["time"].asInt64();
						if (slot_ptr != nullptr) {
							slot_ptr->slot_login(*server_time, results);
						}

						utils::string_map symbols;
						if (current_user.opentime < MIN_OPENTIME || current_user.opentime > MAX_OPENTIME) {
							current_user.opentime = DEFAULT_OPENTIME;
						}
						if (current_user.comparethreshold < 1 || current_user.comparethreshold > 99) {
							current_user.comparethreshold = DEFAULT_COMPARETHRESHOLD;
							if (symbols.empty()) {
								symbols["def"] = str_cast(current_user.comparethreshold);
								symbols["min"] = "1";
								symbols["max"] = "99";
								symbols["field"] = _("Compare threshold");
							}
						}
						if (current_user.areathreshold < MIN_AREATHRESHOLD || current_user.areathreshold > MAX_AREATHRESHOLD) {
							current_user.areathreshold = DEFAULT_AREATHRESHOLD;
							if (symbols.empty()) {
								symbols["def"] = str_cast(current_user.areathreshold);
								symbols["min"] = str_cast(MIN_AREATHRESHOLD);
								symbols["max"] = str_cast(MAX_AREATHRESHOLD);
								symbols["field"] = _("Area threshold");
							}
						}
						if (current_user.keepalive < 10 || current_user.keepalive > 30) {
							current_user.keepalive = DEFAULT_KEEPALIVE;
							if (symbols.empty()) {
								symbols["def"] = str_cast(current_user.keepalive);
								symbols["min"] = "10";
								symbols["max"] = "30";
								symbols["field"] = _("Keepalive interval");
							}
						}
						if (current_user.reboottime < MIN_REBOOTTIME || current_user.reboottime >= 3600 * 24) {
							current_user.reboottime = DEFAULT_REBOOTTIME;
							if (symbols.empty()) {
								symbols["def"] = "1:00:00";
								symbols["min"] = "00:10:00";
								symbols["max"] = "23:59:59";
								symbols["field"] = _("Reboot time");
							}
						}
						if (current_user.eventmasktime_ms < MIN_EVENTMASKTIME_S * 1000 || current_user.eventmasktime_ms > MAX_EVENTMASKTIME_S * 1000) {
							current_user.eventmasktime_ms = DEFAULT_EVENTMASKTIME_S * 1000;
							if (symbols.empty()) {
								symbols["def"] = str_cast(DEFAULT_EVENTMASKTIME_S);
								symbols["min"] = str_cast(MIN_EVENTMASKTIME_S);
								symbols["max"] = str_cast(MAX_EVENTMASKTIME_S);
								symbols["field"] = _("Event mask time");
							}
						}
						
						if (!symbols.empty()) {							
							invalid_field_msg = vgettext2("Invalid '$field', adjust to $def. Min is $min, and max is $max. Go 'device management' to modify it.", symbols);
						}

						const std::string camera_err = parse_camera_str(results["position"].asString());
						if (invalid_field_msg.empty() && !camera_err.empty()) {
							invalid_field_msg = camera_err;
						}

						if (dtype_terminal()) {
							if (invalid_field_msg.empty() && current_user.serverversion < version_info("1.0.4")) {
								invalid_field_msg = _("To use Terminal, the server must be v1.0.4 or above.");
							}
						}


						if (current_user.novideorebootthreshold <= 5 || current_user.novideorebootthreshold > 24 * 3600) {
							current_user.novideorebootthreshold = DEFAULT_NOVIDEOREBOOTTHRESHOLD;
						}

						if (game_config::checker) {
							current_user.eventmasktime_ms = game_config::mask_event_ms;
						}
						preferences::set_scene(current_user.scene);
						preferences::set_oem(current_user.oem);
						preferences::set_cardtype(current_user.cardtype);
						preferences::set_virtualcard(current_user.virtualcard);
						preferences::set_showname(current_user.showname);
						preferences::set_liveness(current_user.liveness);
						preferences::set_openmethod(current_user.openmethod);
						preferences::set_opentime(0, current_user.opentime);
						preferences::set_comparethreshold(current_user.comparethreshold);
						preferences::set_areathreshold(current_user.areathreshold);
						preferences::set_reboottime(current_user.reboottime);
						preferences::set_eventmasktime(current_user.eventmasktime_ms);
						preferences::set_novideorebootthreshold(current_user.novideorebootthreshold);
						preferences::set_alerttemperature(current_user.alerttemperature);
						preferences::set_feverpass(current_user.feverpass);
						preferences::set_saveimage(current_user.saveimage);
						preferences::set_personversion(current_user.personversion);
						if (current_user.serverversion < version_info("1.0.3")) {
							preferences::set_position(current_user.camera.positions.front());
						}
						preferences::set_serverflags(current_user.serverflags);
						preferences::set_camera(current_user.camera_str);
						preferences::set_serverversion(current_user.serverversion.str(false));
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !current_user.valid()) {
		err << "login, Unknown error.";
	}

	if (!err.str().empty()) {
		if (!quiet || code == ERRCODE_LOW_VERSION) {
			std::stringstream err2;
			if (code != ERRCODE_LOW_VERSION) {
				err2 << std::string("login[2]: ");
			}
			err2 << err.str();
			gui2::show_message(null_str, err2.str());
		}
		return false;
	}

	return true;
}

bool do_login2(const std::string& devicecode, const std::string& password, int timeout, bool quiet)
{
	VALIDATE(dtype_require_login(), null_str);
	VALIDATE(!game_config::fake_stsdk, null_str);
	tnet_send_wdg_lock wdg_lock;

	std::string invalid_field_msg;
	
	const std::vector<std::string> original_positions = current_user.camera.positions;
	const int original_personversion = current_user.personversion;
	const int original_feature_size = FEATURE_BLOB_SIZE;
	const int original_current_cameras = game_config::current_cameras;
	const int original_scene = current_user.scene;
	current_user.clear();

	const int64_t start_time = time(nullptr);
	const uint32_t start_ticks = SDL_GetTicks();
	int64_t server_time = 0;

	net::thttp_agent agent(form_url("village/device", "login"), "POST", game_config::cert_file, timeout == nposm? TIMEOUT_NORMAL: timeout);
	agent.did_pre = std::bind(&did_pre_login, _2, _3, devicecode, password);
	agent.did_post = std::bind(&did_post_login, _2, _3, devicecode, get_slot_ptr(), quiet, std::ref(invalid_field_msg), &server_time);
	SDL_Log("[net.cpp]do_login2, devicecode: %s, pre handle_http_request", devicecode.c_str());
	bool ret = net::handle_http_request(agent);
	SDL_Log("[net.cpp]do_login2, devicecode: %s, post handle_http_request, ret: %s", devicecode.c_str(), ret? "true": "false");
	if (!ret) {
		return false;
	}
	
	// since login success, save url. avoid below exception quit, require input url again.
	preferences::set_bbs_server_furl(game_config::bbs_server.generate_furl());

	// devicecode and password
	// devicecode is relatvie hero, it is complex, fix it later.

	utils::string_map symbols;
	if (game_config::dtype == dtype_hvm && original_scene != current_user.scene) {
		symbols.clear();
		symbols["device"] = scene_str(original_scene);
		symbols["server"] = scene_str(current_user.scene);
		VALIDATE(false, vgettext2("Current scene of the device is $device, and the server scene is $server. Please quit app and run again.", symbols));
	}

	const uint32_t stop_ticks = SDL_GetTicks();
	if (stop_ticks - start_ticks < 2000 && server_time >= 1532800000) {
		int64_t abs_diff = posix_abs(server_time - start_time);
		const int time_sync_threshold = game_config::dtype == dtype_access? 2: 900; // hvm: 15 minimuts
		if (abs_diff > time_sync_threshold) {
			int64_t desire = server_time - (stop_ticks - start_ticks) / 1000;
			if (game_config::dtype == dtype_access || game_config::dtype == dtype_avm) {
				SDL_SetTime(desire);
				if (abs_diff > 365 * 24 * 3600) {
					do_reporterror(errcode_rtc, nposm, _("RTC maybe fault. Time offset between device and server is more than one year."), false);
				}
			} else {
				symbols.clear();
				symbols["threshold"] = format_elapse_hms(time_sync_threshold);
				std::string message = vgettext2("Time offset between device and server is more than $threshold. Set up time of HVM or server.", symbols);
				gui2::show_message(null_str, message);
			}
		}
	}

	if (agbox_minus_local == 0 && stop_ticks - start_ticks < 3000 && server_time >= 1532800000) {
		agbox_minus_local = (server_time + 1) - time(nullptr); // why + 1? think server set time to this is 1 second.
	}

	if (faceprint::facestore_initialized) {
		if (game_config::bbs_server.generate_furl() != preferences::bbs_server_furl() || devicecode != group.leader().name() || original_positions != current_user.camera.positions) {
			faceprint::clear_list_feature();
			faceprint::facestore_initialized = false;
		}
	}

	if (game_config::use_facestore) {
		std::string reboot_msg;
		symbols.clear();
		symbols["threshold"] = format_elapse_hms(10);

		if (!faceprint::facestore_initialized) {
			ret = do_facestoreinit(current_user.sessionid);

		} else if (current_user.personversion != original_personversion) {
			reboot_msg = vgettext2("Person data format changes before and after this login. Device will automatically restart after $threshold.", symbols);

		} else if (FEATURE_BLOB_SIZE != original_feature_size) {
			reboot_msg = vgettext2("Feature size changes before and after this login. Device will automatically restart after $threshold.", symbols);
		}

		if (!reboot_msg.empty()) {
			gui2::show_message_onlycountdownclose(null_str, reboot_msg);
			SDL_Reboot();
			VALIDATE(false, "wait reboot");
		}
	}
	if (ret) {
		if (current_user.serverflags & serverflag_httpd) {
			if (!instance->server_registered(server_httpd)) {
				instance->register_server(server_httpd, nullptr);
			}
		} else if (instance->server_registered(server_httpd)) {
			instance->unregister_server(server_httpd);
		}
	}
	if (!invalid_field_msg.empty()) {
		do_reporterror(errcode_invalidfield, nposm, invalid_field_msg, false);
	}
	return ret;
}

bool did_pre_logout(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid)
{
	VALIDATE(!sessionid.empty(), null_str);
	
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = sessionid;
	json_root["operator"] = current_operator.username;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_logout(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, std::string("logout[1]: ") + err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "logout, Unknown error.";
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, std::string("logout[2]: ") + err.str());
		return false;
	}

	return true;
}

bool do_logout()
{
	VALIDATE(current_user.valid(), null_str);

	tnet_send_wdg_lock wdg_lock;
	if (dtype_multivillage()) {
		return true;
	}
	VALIDATE(dtype_require_login(), null_str);

	net::thttp_agent agent(form_url("village/device", "logout"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&did_pre_logout, _2, _3, current_user.sessionid);
	agent.did_post = std::bind(&did_post_logout, _2, _3);

	return net::handle_http_request(agent);
}

static bool allowed_attribute_key(const std::string& key)
{
	std::set<std::string> keys;
	keys.insert("nickname");

	return keys.count(key) > 0;
}

static std::string calculate_version()
{
	if (current_user.serverversion >= version_info("0.9.9")) {
		return game_config::version.str(true);
	}
	return game_config::version.str(false);
}

std::string from_ipv4(uint32_t ipv4)
{
	net::IPAddress ip((const uint8_t*)&ipv4, 4);
	return ip.ToString();
}

bool did_pre_keepalive(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, int nonce, int64_t notification_ts)
{
	VALIDATE(!sessionid.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = calculate_version();
	json_root["sessionid"] = sessionid;
	json_root["nonce"] = nonce;
	if (current_user.serverversion >= version_info("1.0.3")) {
		json_root["httpd"] = instance->httpd_mgr().url();
	}
	if (current_user.serverversion >= version_info("1.0.4")) {
		std::string lanip_str;
		uint32_t ip = instance->current_ip();
		if (ip != INADDR_ANY) {
			lanip_str = from_ipv4(ip);
		}
		json_root["lanip"] = lanip_str;
		json_root["notificationts"] = notification_ts;
	}
	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	std::vector<tkeepalive_track>& tracks = game_config::keepalive_tracks;
	const int max_keepalive_tracks = 3;
	if (tracks.size() >= max_keepalive_tracks) {
		tracks.erase(tracks.begin());
	}
	tracks.push_back(tkeepalive_track(nonce, time(nullptr)));

	return true;
}

bool did_post_keepalive(const net::RoseDelegate& delegate, int status, int nonce, int64_t* last_feature_ts, tkeepalive_result& result2)
{
	std::vector<tkeepalive_track>& tracks = game_config::keepalive_tracks;
	tkeepalive_track& track = tracks.back();
	track.receive_resp_ticks = SDL_GetTicks();

	*last_feature_ts = 0;
	std::stringstream err;

	bool quiet = true;
	if (status != net::OK) {
		track.finish_track(status);
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, std::string("keepalive[1]: ") + err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "keepalive, Unknown error.";
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& sessionid = results["sessionid"];

						if (results["nonce"].asInt() != nonce) {
							err << _("Confused nonce.");
						} else {
							*last_feature_ts = results["facestore"].asInt64();
							result2.datadictionary_ts = results["datadictionary"].asInt64();
							result2.reboot = results["reboot"].asBool();
							result2.relogin = results["relogin"].asBool();
							result2.featuretasks = results["featuretasks"].asInt();
							result2.servertime = results["time"].asInt64();
							result2.notification_ts = results["notificationts"].asInt64();

							Json::Value& notifications = results["notification"];
							if (dtype_require_notification() && notifications.isArray()) {
								track.notifications = notifications.size();
								for (int at = 0; at < (int)notifications.size(); at ++) {
									const Json::Value& item = notifications[at];
									result2.notifications.push_back(pb2::tnotification());
									pb2::tnotification& notification = result2.notifications.back();
									notification.set_deviceid(item["deviceid"].asInt());
									notification.set_account(item["account"].asString());
									notification.set_uuid(item["uuid"].asString());
									notification.set_timestamp(item["time"].asInt64());
									notification.set_errcode(item["errcode"].asInt());
									notification.set_channel(item["channel"].asInt());
									notification.set_msg(item["msg"].asString());
									notification.set_ctime(item["ctime"].asInt64());
								}
							}

						}
					} else {
						err << "keepalive, no results object";
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	track.finish_track(status);
	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, std::string("keepalive[2]: ") + err.str());
		}
		return false;
	}

	return true;
}

bool do_keepalive(const std::string& sessionid, int nonce, int64_t notification_ts, tkeepalive_result& result2)
{
	tnet_send_wdg_lock wdg_lock;
	if (dtype_multivillage()) {
		std::vector<tvillage> villages;
		return multivlg_do_keepalive(current_user.devicecode, villages, result2, TIMEOUT_NORMAL, true);
	}
	VALIDATE(dtype_require_login(), null_str);

	int64_t last_feature_ts = 0;
	uint32_t start_ticks = SDL_GetTicks();

	int timeout = game_config::dtype == dtype_hvm? nposm: TIMEOUT_NORMAL;
	net::thttp_agent agent(form_url("village/device", "keepalive"), "POST", game_config::cert_file, timeout);
	agent.did_pre = std::bind(&did_pre_keepalive, _2, _3, sessionid, nonce, notification_ts);
	agent.did_post = std::bind(&did_post_keepalive, _2, _3, nonce, &last_feature_ts, std::ref(result2));

	bool ret = net::handle_http_request(agent);
	if (!ret) {
		return ret;
	}

	uint32_t stop_ticks = SDL_GetTicks();
	if (stop_ticks - start_ticks < 2000) {
		agbox_minus_local = (result2.servertime + 1) - time(nullptr);
	}

	if (game_config::use_facestore) {
		if (!faceprint::facestore_initialized) {
			// do_facestoreinit in do_login maybe fail.
			net::do_facestoreinit(current_user.sessionid);
			
		} else if (last_feature_ts > faceprint::facestore_ts) {
			net::do_facestoresync();
		}
	}

	return true;
}

bool did_pre_verifyoperator(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& username, const std::string& password)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = sessionid;
	json_root["username"] = username;
	json_root["password"] = password;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_verifyoperator(const net::RoseDelegate& delegate, int status, const std::string& username, const std::string& password, toperator& operator2)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "verifyoperator, Unknown error.";
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						int group = results["group"].asInt();
						VALIDATE(CHECK_GROUP(group), null_str);
						operator2 = toperator(username, password, group);
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !operator2.valid()) {
		err << "did_post_verifyoperator, err.str().empty() && !operator2.valid(), Unknown error.";
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_verifyoperator(const std::string& sessionid, const std::string& username, const std::string& password, toperator& operator2)
{
	VALIDATE(!operator2.valid(), null_str);

	tnet_send_wdg_lock wdg_lock;
	if (dtype_multivillage()) {
		return multivlg_do_verifyoperator(username, password, operator2);
	}
	VALIDATE(dtype_require_login(), null_str);

	net::thttp_agent agent(form_url("village/device", "verifyoperator"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&did_pre_verifyoperator, _2, _3, sessionid, username, password);
	agent.did_post = std::bind(&did_post_verifyoperator, _2, _3, username, password, std::ref(operator2));

	return net::handle_http_request(agent);
}

std::string nationcode_2_nation(int code)
{
	if (game_config::nations.count(code) == 0) {
		// there is old person data.
		code = game_config::nations.begin()->first;
	}

	std::stringstream ss;
	ss << std::setw(2) << std::setfill('0') << code;
	return ss.str();
}

struct tother_image
{
	tother_image()
		: data(nullptr)
	{
		release();
	}

	tother_image(const std::string& jsonstr)
		: data(nullptr)
	{
		int offset = 0;
		bool ok = true;
		std::vector<std::string> vstr = utils::split(jsonstr, '|');
		int vstr_size = vstr.size();
		if (vstr_size != 4) {
			ok = false;
		}
		if (ok) {
			id = vstr[0];
			if (id != "idpic") {
				ok = false;
			}
		}
		if (ok) {
            start = utils::to_int(vstr[1]);
            if (start != offset) {
              ok = false;
            }
		}
		if (ok) {
            len = utils::to_int(vstr[2]);
            if (len <= 0) {
              ok = false;
            }
		}
		if (ok) {
			format = image_type_from_str(vstr[3]);
            if (format == nposm) {
				ok = false;
			}
		}
		if (!ok) {
			release();
		}
	}

	tother_image(const std::string& id, int start, int len, int format, uint8_t* data)
		: id(id)
		, start(start)
		, len(len)
		, data(data)
		, format(format)
	{}

	bool valid(bool exclude_data) const
	{ 
		if (id.empty() || start < 0 || len <= 0 || (format != img_jpg && format != img_png)) {
			return false;
		}
		if (!exclude_data && data == nullptr) {
			return false;
		}
		return true;
	}

	void release()
	{
		id.clear();
		start = 0;
		len = 0;
		format = nposm;
		if (data != nullptr) {
			SDL_free(data);
			data = nullptr;
		}
	}

	std::string to_jsonstr() const
	{
		VALIDATE(valid(false), null_str);
		std::stringstream ss;
		ss << id << "|" << start << "|" << len << "|" << image_formats[format];
		return ss.str();
	}

	std::string id;
	int start;
	int len;
	int format;
	uint8_t* data;
};

bool did_pre_updateface(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const tperson& person, bool insert)
{
	VALIDATE(person.feature && person.feature_size == FEATURE_BLOB_SIZE && person.image_data && person.image_len > 0, null_str);
	if (person.idtype == gat517_idcard) {
		VALIDATE(!person.idnumber.empty(), null_str);

	} else if (person.idtype == gat517_tempo_pass) {
		VALIDATE(person.idtype == gat517_tempo_pass, null_str);
		VALIDATE(person.idnumber.empty(), null_str);
	}
	if (person.visitor) {
		VALIDATE(insert, null_str);
		// VALIDATE(!person.signout, null_str);
		VALIDATE(person.deadline > time(nullptr) + net::agbox_minus_local, null_str);
		// avm maybe no intervieweeid in sms scene.
		// VALIDATE(person.intervieweeid != nposm, null_str);
		VALIDATE(!person.code3.valid() && !person.residence_code3.valid(), null_str);

	} else {
		if (insert) {
			VALIDATE(!person.signout, null_str);
		}
		if (current_user.serverversion < version_info("0.9.9")) {
			VALIDATE(person.deadline == 0, null_str);
		}
		// VALIDATE(person.intervieweeid == nposm, null_str);
		VALIDATE(!person.onceonly, null_str);
	}
	if (person.fakefeature) {
		VALIDATE(!person.dummyimage, null_str);
	} else if (person.dummyimage) {
		VALIDATE(!person.fakefeature, null_str);
	}

	if (person.idnumber_surf.get() != nullptr) {
		VALIDATE(current_user.serverversion >= version_info("1.0.0") && is_idtype_switchable(person.idtype), null_str);
	}

	std::stringstream ss;
	*body = nullptr;

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["idtype"] = str_cast(person.idtype);
	json_root["idnumber"] = person.idnumber;
	json_root["name"] = utils::truncate_to_max_bytes(person.name.c_str(), person.name.size(), MAX_CS_NAME_SIZE);
	json_root["card"] = person.card.valid()? person.card.to_json_card(): null_str;
	json_root["type"] = person.type;
	json_root["peopletype"] = person.peopletype;
	json_root["phones"] = utils::join(utils::split(person.phones, ','));
	json_root["houses"] = tcs_house::to_json_houses(person.houses);
	json_root["groups"] = utils::join(person.groups);
	json_root["cars"] = tcs_car::to_json_cars(person.cars);
	if (game_config::os == os_windows && !person.cars.empty()) {
		const std::string cars_str = tcs_car::to_json_cars(person.cars);
		tfile file(game_config::preferences_dir + "/cars_set.dat", GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, cars_str.c_str(), cars_str.size());
	}
	json_root["signout"] = person.signout;
	json_root["deadline"] = person.deadline;
	json_root["onceonly"] = person.onceonly;
	json_root["gender"] = person.gender;

	const bool is_exhibition = current_user.oem & oem_exhibition;
	if (!is_exhibition) {
		json_root["nation"] = nationcode_2_nation(person.nation);
	} else {
		ss.str("");
		for (std::set<int>::const_iterator it = person.exhibition_nations.begin(); it != person.exhibition_nations.end(); ++ it) {
			const int code = *it;
			if (!ss.str().empty()) {
				ss << ",";
			}
			ss << nationcode_2_nation(code);
		}
		json_root["nation"] = ss.str();
	}
	// birthplace code3
	json_root["address"] = person.code3.address;
	if (game_config::provinces.count(person.code3.province) != 0) {
		json_root["provincecode"] = person.code3.province;
	}
	if (person.code3.district != nullptr) {
		json_root["citycode"] = person.code3.district->city_code;
		json_root["districtcode"] = person.code3.district->code;
	}
	if (person.code3.street != nullptr) {
		json_root["streetcode"] = person.code3.street->code;
	}
	
	// residence code3
	json_root["residenceaddress"] = person.residence_code3.address;
	if (game_config::provinces.count(person.residence_code3.province) != 0) {
		json_root["residenceprovincecode"] = person.residence_code3.province;
	}
	if (person.residence_code3.district != nullptr) {
		json_root["residencecitycode"] = person.residence_code3.district->city_code;
		json_root["residencedistrictcode"] = person.residence_code3.district->code;
	}
	if (person.residence_code3.street != nullptr) {
		json_root["residencestreetcode"] = person.residence_code3.street->code;
	}

	if (insert || current_user.serverversion >= version_info("0.9.9")) {
		// >= 0.9.9, villageserver's updateface can support intervieweeid.
		// if app send request that has villageserver not support, will response "param error".
		json_root["intervieweeid"] = person.intervieweeid != nposm? person.intervieweeid: invalid_faceid_in_villageserver;
	}
	if (current_user.serverversion >= version_info("0.9.9")) {
		// >= 0.9.9, villageserver's addface/upateface can support visitor, featuresize
		json_root["nationality"] = person.nationality;
		json_root["education"] = person.education != nposm? person.education: 0;
		json_root["marital"] = person.marital != nposm? person.marital: 0;
		int64_t birth = date_str_2_ts(person.birth);
		json_root["birth"] = birth != 0? format_time_ymd2(birth, '-', true): null_str;
		json_root["startline"] = person.startline;
		json_root["visitor"] = person.visitor;
		json_root["featuresize"] = FEATURE_BLOB_SIZE;
	}

	if (current_user.serverversion >= version_info("1.0.1")) {
		json_root["note"] = person.note.to_json_str();
	}
	if (current_user.serverversion >= version_info("1.0.3")) {
		json_root["lifts"] = tperson_lift::to_json_lifts(person.lifts);
	}
	if (current_user.serverversion >= version_info("1.0.5")) {
		json_root["fakefeature"] = person.fakefeature;
		json_root["dummyimage"] = person.dummyimage;
	}

	json_root["imageformat"] = image_formats[person.image_type];

	tother_image other_image;
	if (person.idnumber_surf.get() != nullptr) {
		const int imageformat = img_jpg;
		int len;
		uint8_t* image_data = imwrite_mem(person.idnumber_surf, imageformat, &len);
		other_image = tother_image("idpic", 0, len, img_jpg, image_data);
		json_root["otherimage"] = other_image.to_jsonstr();
	}

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + other_image.len + person.feature_size + SHA_DIGEST_LENGTH + person.image_len;
	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());
	// other image
	if (other_image.valid(false)) {
		memcpy(buf2 + prefix_size + json_str.size(), other_image.data, other_image.len);
	}
	// feature
	memcpy(buf2 + prefix_size + json_str.size() + other_image.len, person.feature, person.feature_size);
	// sha1
	std::unique_ptr<uint8_t[]> md = utils::sha1((const uint8_t*)person.feature, person.feature_size);
	memcpy(buf2 + prefix_size + json_str.size() + other_image.len + person.feature_size, md.get(), SHA_DIGEST_LENGTH);
	// image
	memcpy(buf2 + prefix_size + json_str.size() + other_image.len + person.feature_size + SHA_DIGEST_LENGTH, person.image_data, person.image_len);

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	if (other_image.valid(false)) {
		other_image.release();
	}

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_updateface(const net::RoseDelegate& delegate, int status, tperson& person, bool insert, int* existed_code)
{
	std::stringstream err;

	const std::string& data_received = delegate.data_received();
	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					if (code != ERRCODE_PERSON_EXISTED || existed_code == nullptr) {
						err << json_object["msg"].asString();
						if (err.str().empty()) {
							err << "did_post_updateface, Unknown error.";
						}
					} else {
						*existed_code = code;
						return false;
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						if (insert) {
							person.stubno = results["stubno"].asString();
						}
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_updateface(const tvillage& village, tperson& person, bool insert, int* existed_code)
{
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);
	if (existed_code) {
		*existed_code = 0;
	}
	if (dtype_multivillage()) {
		return multivlg_do_updateface(village, person, insert, existed_code);
	}

	VALIDATE(dtype_require_login(), null_str);
	int timeout = game_config::dtype == dtype_access? 10000: nposm;
	if (game_config::os == os_windows) {
		int ii = 0;
		// timeout = 40000;
	}
	char* body = nullptr;
	std::string task = "updateface";
	if (insert && current_user.serverversion < version_info("0.9.9")) {
		task = "addface";
	}
	net::thttp_agent agent(form_url("village/device", task), "POST", game_config::cert_file, timeout);
	agent.did_pre = std::bind(&did_pre_updateface, _1, _2, &body, std::ref(person), insert);
	agent.did_post = std::bind(&did_post_updateface, _2, _3, std::ref(person), insert, existed_code);

	bool ret = net::handle_http_request(agent);
	if (body) {
		free(body);
	}
	return ret;
}

int make_right_gender(int gender)
{
	if (gender != SDL_GenderMale && gender != SDL_GenderFemale) {
		return SDL_GenderMale;
	}
	return gender;
}

void base_person_from_json(const Json::Value& results, tperson& person)
{
	// except: idtype, idnumber, imageformat
	VALIDATE(results.isObject(), null_str);
	person.card = tcs_card(results["card"].asString());
	person.name = results["name"].asString();
	person.gender = make_right_gender(results["gender"].asInt());
	person.nation = utils::to_int(results["nation"].asString());
	person.phones = utils::join(utils::split(results["phones"].asString(), ','));
	person.houses = tcs_house::from_json_houses(results["houses"].asString());
	person.groups = utils::split(results["groups"].asString());
	person.cars = tcs_car::from_json_cars(results["cars"].asString());
	person.lifts = tperson_lift::from_json_lifts(results["lifts"].asString());
	person.signout = results["signout"].asBool();
	person.startline = results["startline"].asInt64();
	person.deadline = results["deadline"].asInt64();
	if (current_user.serverversion >= version_info("0.9.9")) {
		person.visitor = results["visitor"].asBool();
	} else {
		person.visitor = person.deadline != 0;
	}
	person.onceonly = results["onceonly"].asBool();
	person.fakefeature = results["fakefeature"].asBool();
	person.type = results["type"].asInt();
	person.peopletype = results["peopletype"].asInt();
	person.nationality = results["nationality"].asString();
	person.education = results["education"].asInt();
	person.marital = results["marital"].asInt();
	person.birth = results["birth"].asString();
	person.note = tcs_note(person.visitor, results["note"].asString());

	const int intervieweeid = results["intervieweeid"].asInt();
	person.intervieweeid = intervieweeid != invalid_faceid_in_villageserver? intervieweeid: nposm;

	// code3
	person.code3.province = results["provincecode"].asInt64();
	int64_t i64code = results["districtcode"].asInt64();
	if (game_config::districts.count(i64code)) {
		person.code3.district = &(game_config::districts.find(i64code)->second);
	}
	i64code = results["streetcode"].asInt64();
	if (game_config::streets.count(i64code)) {
		person.code3.street = &(game_config::streets.find(i64code)->second);
	}
	if (!person.code3.valid()) {
		// person.code3.clear();
	}
	person.code3.address = results["address"].asString();

	// residence code3
	person.residence_code3.province = results["residenceprovincecode"].asInt64();
	i64code = results["residencedistrictcode"].asInt64();
	if (game_config::districts.count(i64code)) {
		person.residence_code3.district = &(game_config::districts.find(i64code)->second);
	}
	i64code = results["residencestreetcode"].asInt64();
	if (game_config::streets.count(i64code)) {
		person.residence_code3.street = &(game_config::streets.find(i64code)->second);
	}
	if (!person.residence_code3.valid()) {
		// person.residence_code3.clear();
	}
	person.residence_code3.address = results["residenceaddress"].asString();
}

std::pair<int64_t, int64_t> timespan_from_selector(int selector)
{
	int64_t starttime = 0;
	int64_t endtime = 0;

	const int64_t now_village_time = time(nullptr) + net::agbox_minus_local + game_config::village_timezone * 3600; // conside village's timezone
	const int one_day_seconds = 24 * 3600;
	int elapse_today = now_village_time % one_day_seconds;

	if (selector == time_today) {
		starttime = now_village_time - elapse_today;
		endtime = starttime + one_day_seconds;

	} else if (selector == time_yesterday) {
		starttime = now_village_time - elapse_today - one_day_seconds;
		endtime = starttime + one_day_seconds;

	} else if (selector == time_before_yesterday) {
		starttime = now_village_time - elapse_today - 2 * one_day_seconds;
		endtime = starttime + one_day_seconds;

	} else if (selector == time_3day) {
		starttime = now_village_time - elapse_today - 2 * one_day_seconds;
		endtime = starttime + 3 * one_day_seconds;

	} else {
		VALIDATE(selector == time_nonrestricted, null_str);

	} 

	return std::make_pair(starttime, endtime);
}

bool did_pre_findface(net::HttpRequestHeaders& headers, std::string& body, const tqueryperson_key& key, int status_type, bool all_persons)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	const int idtype = key.idtype == nposm? 0: key.idtype;
	json_root["idtype"] = idtype != 0? str_cast(idtype): null_str;
	json_root["idnumber"] = key.idnumber;
	json_root["maxfaces"] = all_persons? 0: FINDFACE_PAGE_SIZE;
	json_root["name"] = key.name;
	json_root["card"] = key.card;
	std::pair<int64_t, int64_t> timespan = timespan_from_selector(key.time_selector);
	json_root["starttime"] = timespan.first;
	json_root["endtime"] = timespan.second;
	json_root["houses"] = key.house;
	if (current_user.serverversion >= version_info("0.9.9")) {
		json_root["phones"] = key.phones;
	}

	std::string status_value;
	if (status_type == status_normal) {
		status_value = "normal";
	} else if (status_type == status_signouted) {
		status_value = "signouted";
	}
	json_root["status"] = status_value;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_findface(const net::RoseDelegate& delegate, int status, int deadline_type, int status_type, bool all_persons, int* count_ptr, std::vector<tperson>& persons)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "did_post_findface, Unknown error.";
					}
				} else {
					Json::Value& results = json_object["results"];
					*count_ptr = results["count"].asInt();
					if (results.isObject()) {
						Json::Value& faces = results["faces"];
						if (faces.isArray()) {
							for (int at = 0; at < (int)faces.size(); at ++) {
								const Json::Value& item = faces[at];
								const int64_t deadline = item["deadline"].asInt64();
								const bool visitor = current_user.serverversion >= version_info("0.9.9")? item["visitor"].asBool(): deadline != 0;
								if (deadline_type == deadline_owner) {
									if (visitor) {
										continue;
									}
								} else if (deadline_type == deadline_visitor) {
									if (!visitor) {
										continue;
									}
								}

								const Json::Value& signout_value = item["signout"];
								if (signout_value.isBool()) {
									// 0.9.6 has no signout filed.
									if (status_type == status_normal) {
										if (item["signout"].asBool()) {
											continue;
										}
									} else if (status_type == status_signouted) {
										if (!item["signout"].asBool()) {
											continue;
										}
									}
								}
								persons.push_back(tperson());
								tperson& person = persons.back();
								person.faceid = item["faceid"].asInt();
								if (all_persons) {
									person.visitor = visitor;
									person.deadline = deadline;
									person.name = item["name"].asString();
									person.gender = make_right_gender(item["gender"].asInt());
									person.phones = item["phones"].asString();
								} else {
									base_person_from_json(item, person);
									person.idtype = idtypes_find(game_config::idtypes, item["idtype"].asString());
									person.idnumber = item["idnumber"].asString();
									person.registertime = item["registertime"].asInt64();
								}
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !current_operator.valid()) {
		err << "did_post_findface, err.str().empty() && !current_operator.valid(), Unknown error.";
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_findface(const tvillage& village, const tqueryperson_key& key, int deadline_type, int status_type, bool all_persons, int* count_ptr, std::vector<tperson>& persons)
{
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(key.valid() && count_ptr != nullptr, null_str);
	VALIDATE(deadline_type == nposm || deadline_type == deadline_owner || deadline_type == deadline_visitor, null_str);
	VALIDATE(status_type == nposm || status_type == status_normal || status_type == status_signouted, null_str);
	*count_ptr = 0;

	if (dtype_multivillage()) {
		return multivlg_do_findface(village, key, status_type, count_ptr, persons);
	}

	VALIDATE(dtype_require_login(), null_str);

	net::thttp_agent agent(form_url("village/device", "findface"), "POST", game_config::cert_file, nposm);
	agent.did_pre = std::bind(&did_pre_findface, _2, _3, std::ref(key), status_type, all_persons);
	agent.did_post = std::bind(&did_post_findface, _2, _3, deadline_type, status_type, all_persons, count_ptr, std::ref(persons));

	return net::handle_http_request(agent);
}

bool did_pre_queryface(net::HttpRequestHeaders& headers, std::string& body, const tperson& person)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["idtype"] = str_cast(person.idtype);
	json_root["idnumber"] = person.idnumber;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_queryface(const net::RoseDelegate& delegate, int status, bool quiet, tperson& person)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						base_person_from_json(results, person);
						person.intervieweename = results["intervieweename"].asString();
						person.intervieweehouses = tcs_house::from_json_houses(results["intervieweehouses"].asString());
						
						person.image_type = image_type_from_str(results["imageformat"].asString());

						std::string other_image_str = results["otherimage"].asString();
						tother_image other_image(other_image_str);
						
						int extra_data_size = content_size - prefix_size - json_size;
						bool ok = extra_data_size > other_image.len;
						if (ok && other_image.valid(true)) {
							SDL_RWops* src = SDL_RWFromMem((void*)(content + prefix_size + json_str.size()), other_image.len);
							person.idnumber_surf = IMG_Load_RW(src, 0);
							SDL_RWclose(src);
							person.idnumber_surf = makesure_neutral_surface(person.idnumber_surf);
						}

						const uint8_t* face_image_start = (const uint8_t*)content + prefix_size + json_size + other_image.len;
						extra_data_size -= other_image.len;
						if (ok && extra_data_size < FEATURE_BLOB_SIZE + SHA_DIGEST_LENGTH) {
							// if extra_data_size == FEATURE_BLOB_SIZE + SHA_DIGEST_LENGTH, it hasn't image. 
							ok = false;
						}
						if (ok) {
							// sha1
							std::unique_ptr<uint8_t[]> md = utils::sha1(face_image_start, FEATURE_BLOB_SIZE);
							ok = memcmp(face_image_start + FEATURE_BLOB_SIZE, md.get(), SHA_DIGEST_LENGTH) == 0;
						}
						if (ok) {
							person.feature_size = FEATURE_BLOB_SIZE;
							person.feature = (char*)malloc(FEATURE_BLOB_SIZE);
							memcpy(person.feature, face_image_start, FEATURE_BLOB_SIZE);

							person.image_len = content_size - prefix_size - json_size - other_image.len - FEATURE_BLOB_SIZE - SHA_DIGEST_LENGTH;

							if (person.image_len != 0) {
								VALIDATE(person.image_len > 4, null_str);

								person.image_data = (uint8_t*)malloc(person.image_len);
								memcpy(person.image_data, face_image_start + FEATURE_BLOB_SIZE + SHA_DIGEST_LENGTH, person.image_len);

								if (person.image_type == nposm) {
									// if register use combo-identify, image_type is nposm.
									if (person.image_data[0] == 0x89 &&
										person.image_data[1] == 'P' &&
										person.image_data[2] == 'N' &&
										person.image_data[3] == 'G' ) {
										person.image_type = img_png;
									} else {
										person.image_type = img_jpg;
									}
								}
							} else {
								person.dummyimage = !person.fakefeature;

								const tuint8data& data = get_jpgdata(person.fakefeature? jpgdata_fakefeature: jpgdata_unsaved);
								person.image_len = data.len;
								person.image_data = (uint8_t*)malloc(person.image_len);
								memcpy(person.image_data, data.ptr, person.image_len);
								person.image_type = img_jpg;
							}

						} else {
							err << _("Xmit fail. Get feature or image fail. Try again.");
							person.image_type = nposm;
						}
					} else {
						err << _("device/queryface. Json data error. No results object.");
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_queryface(const tvillage& village, tperson& person, bool quiet)
{
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(game_config::idtypes.count(person.idtype) != 0 && !person.idnumber.empty(), null_str);

	tnet_send_wdg_lock wdg_lock;
	if (dtype_multivillage()) {
		return multivlg_do_queryface(village, person, quiet);
	}
	VALIDATE(dtype_require_login(), null_str);

	net::thttp_agent agent(form_url("village/device", "queryface"), "POST", game_config::cert_file, nposm);
	agent.did_pre = std::bind(&did_pre_queryface, _2, _3, std::ref(person));
	agent.did_post = std::bind(&did_post_queryface, _2, _3, quiet, std::ref(person));

	return net::handle_http_request(agent);
}

bool did_pre_findvisit(net::HttpRequestHeaders& headers, std::string& body, const tqueryvisit_key& key)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["maxvisits"] = key.maxvisits;
	json_root["page"] = key.page;
	const int idtype = key.idtype == nposm? 0: key.idtype;
	json_root["idtype"] = idtype != 0? str_cast(idtype): null_str;
	json_root["idnumber"] = key.idnumber;
	json_root["name"] = key.name;
	json_root["stubno"] = key.stubno;
	json_root["close"] = key.close_status;
	std::pair<int64_t, int64_t> timespan = timespan_from_selector(key.time_selector);
	json_root["starttime"] = timespan.first;
	json_root["endtime"] = timespan.second;
	json_root["house"] = key.house;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_findvisit(const net::RoseDelegate& delegate, int status, int* count_ptr, std::vector<tvisit>& visits)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					*count_ptr = results["count"].asInt();
					if (results.isObject()) {
						Json::Value& faces = results["visits"];
						if (faces.isArray()) {
							for (int at = 0; at < (int)faces.size(); at ++) {
								const Json::Value& item = faces[at];
								visits.push_back(tvisit());
								tvisit& visit = visits.back();

								visit.id = item["id"].asInt();
								visit.stubno = item["stubno"].asString();
								visit.name = item["name"].asString();
								visit.idtype = idtypes_find(game_config::idtypes, item["idtype"].asString());
								visit.idnumber = item["idnumber"].asString();
								visit.phones = item["phones"].asString();
								visit.intervieweename = item["intervieweename"].asString();
								visit.houses = tcs_house::from_json_houses(item["intervieweehouse"].asString());
								visit.starttime = item["starttime"].asInt64();
								visit.endtime = item["endtime"].asInt64();
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !current_operator.valid()) {
		err << _("Unknown error.");
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_findvisit(const tqueryvisit_key& key, int* count_ptr, std::vector<tvisit>& visits)
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(count_ptr != nullptr, null_str);
	*count_ptr = 0;

	net::thttp_agent agent(form_url("village/device", "findvisit"), "POST", game_config::cert_file, nposm);
	agent.did_pre = std::bind(&did_pre_findvisit, _2, _3, std::ref(key));
	agent.did_post = std::bind(&did_post_findvisit, _2, _3, count_ptr, std::ref(visits));

	return net::handle_http_request(agent);
}

bool did_pre_closevisit(net::HttpRequestHeaders& headers, std::string& body, const tvisit& visit)
{
	VALIDATE(visit.id > 0 && visit.endtime > 0, null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["id"] = visit.id;
	json_root["time"] = visit.endtime;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_closevisit(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !current_operator.valid()) {
		err << _("Unknown error.");
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_closevisit(const tvisit& visit)
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);

	net::thttp_agent agent(form_url("village/device", "closevisit"), "POST", game_config::cert_file, nposm);
	agent.did_pre = std::bind(&did_pre_closevisit, _2, _3, std::ref(visit));
	agent.did_post = std::bind(&did_post_closevisit, _2, _3);

	return net::handle_http_request(agent);
}

bool did_pre_downloadfile(net::HttpRequestHeaders& headers, std::string& body, const std::string& src, int start, int size)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["file"] = src;
	json_root["start"] = start;
	json_root["size"] = size;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_downloadfile(const net::RoseDelegate& delegate, int status, uint8_t* data_ptr, int* size_ptr, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					*size_ptr = content_size - prefix_size - json_size;
					if (*size_ptr > 0) {
						memcpy(data_ptr, content + prefix_size + json_size, *size_ptr);
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_downloadfile(const std::string& src, const std::string& filename, int filetype, bool quiet)
{
	VALIDATE(dtype_require_login(), null_str);
	VALIDATE(gui2::tprogress_::instance != nullptr, null_str);

	tmainserial& main_serial = *tmainserial::singleton;
	VALIDATE(current_user.valid(), null_str);

	tfile file(filename, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);

	int start = 0, fsize = nposm; // * 100 maybe exceed int32_t
	const int block_size = 4 * 1024 * 1024;
	int once_received_bytes = block_size;
	file.resize_data(block_size);

	net::thttp_agent agent(form_url("village/device", "downloadfile"), "POST", game_config::cert_file, nposm);

	bool ret = true;
	while (once_received_bytes == block_size) {
		main_serial.stop_wdg();
		agent.did_pre = std::bind(&did_pre_downloadfile, _2, _3, src, start, block_size);
		agent.did_post = std::bind(&did_post_downloadfile, _2, _3, (uint8_t*)file.data, &once_received_bytes, quiet);

		if (!net::handle_http_request(agent)) {
			ret = false;
			break;
		}
		if (once_received_bytes > 0) {
			posix_fwrite(file.fp, file.data, once_received_bytes);
		}

		start += once_received_bytes;
		SDL_Log("do_downloadfile, received %s", format_i64size(start).c_str());

		if (filetype == ftype_update) {
			if (fsize == nposm) {
				VALIDATE(start == once_received_bytes, null_str);
				const tupdate_header* header = (tupdate_header*)(file.data);
				if (header->apk_size <= 0) {
					ret = false;
					break;
				}
				fsize = sizeof(tupdate_header) + header->apk_size + SHA_DIGEST_LENGTH;
			}
			
			gui2::tprogress_::instance->set_percentage(100.0 * start / fsize);
			gui2::tprogress_::instance->set_message(format_i64size(start) + "/" + format_i64size(fsize));
		}
	}
	if (!ret) {
		file.close();
		SDL_DeleteFiles(filename.c_str());
	}
	return ret;
}

bool did_pre_uploadfile(net::URLRequest& req, net::HttpRequestHeaders& headers,  char** body, const std::string& src, int start, bool close, const uint8_t* data_ptr, int data_len)
{
	*body = nullptr;

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["file"] = src;
	json_root["start"] = start;
	json_root["close"] = close;

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + data_len;
	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());
	memcpy(buf2 + prefix_size + json_str.size(), data_ptr, data_len);

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_uploadfile(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_uploadfile(const std::string& short_filename, const std::string& filename, int filetype)
{
	VALIDATE(dtype_require_login(), null_str);
	VALIDATE(gui2::tprogress_::instance != nullptr, null_str);

	tmainserial& main_serial = *tmainserial::singleton;
	VALIDATE(current_user.valid(), null_str);

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = posix_fsize(file.fp);
	VALIDATE(fsize > 0, null_str);
	posix_fseek(file.fp, 0);

	int start = 0; // * 100 maybe exceed int32_t
	const int block_size = 4 * 1024 * 1024;
	int once_upload_bytes = block_size;
	file.resize_data(block_size);

	net::thttp_agent agent(form_url("village/device", "uploadfile"), "POST", game_config::cert_file, nposm);

	bool ret = true;
	while (start < fsize) {
		main_serial.send_wdg(60);
		once_upload_bytes = block_size;
		if (fsize - start < once_upload_bytes) {
			once_upload_bytes = fsize - start;
		}

		posix_fread(file.fp, file.data, once_upload_bytes);

		char* body = nullptr;
		agent.did_pre = std::bind(&did_pre_uploadfile, _1, _2, &body, short_filename, start, fsize - start == once_upload_bytes, (uint8_t*)file.data, once_upload_bytes);
		agent.did_post = std::bind(&did_post_uploadfile, _2, _3);

		if (!net::handle_http_request(agent)) {
			ret = false;
		}
		if (body) {
			free(body);
		}
		if (!ret) {
			break;
		}
		
		start += once_upload_bytes;
		SDL_Log("do_uploadfile, uploaded %s", format_i64size(start).c_str());

		if (filetype == ftype_update) {
			gui2::tprogress_::instance->set_percentage(100.0 * start / fsize);
			gui2::tprogress_::instance->set_message(format_i64size(start) + "/" + format_i64size(fsize));
		}
	}
	if (!ret) {
		file.close();
	}
	return ret;
}

bool do_uploadmem(const std::string& short_filename, const uint8_t* mem, int msize)
{
	VALIDATE(dtype_require_login(), null_str);

	tmainserial& main_serial = *tmainserial::singleton;
	VALIDATE(current_user.valid(), null_str);

	int start = 0; // * 100 maybe exceed int32_t
	const int block_size = 4 * 1024 * 1024;
	int once_upload_bytes = block_size;

	net::thttp_agent agent(form_url("village/device", "uploadfile"), "POST", game_config::cert_file, nposm);

	bool ret = true;
	while (start < msize) {
		main_serial.send_wdg(60);
		once_upload_bytes = block_size;
		if (msize - start < once_upload_bytes) {
			once_upload_bytes = msize - start;
		}

		char* body = nullptr;
		agent.did_pre = std::bind(&did_pre_uploadfile, _1, _2, &body, short_filename, start, msize - start == once_upload_bytes, mem + start, once_upload_bytes);
		agent.did_post = std::bind(&did_post_uploadfile, _2, _3);

		if (!net::handle_http_request(agent)) {
			ret = false;
		}
		if (body) {
			free(body);
		}
		if (!ret) {
			break;
		}
		
		start += once_upload_bytes;
		SDL_Log("do_uploadmem, uploaded %s", format_i64size(start).c_str());
	}
	return ret;
}

bool did_pre_facestoreinit(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = sessionid;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_facestoreinit(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	bool quiet = true;
	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, std::string("facestoreinit[1]: ") + err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, "device/init, content_size <= prefix_size");
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, "device/init, content_size < prefix_size + json_size");
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "facestoreinit, Unknown error.";
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						faceprint::clear_list_feature();
						VALIDATE(!faceprint::facestore_initialized, null_str);

						// personversion of local facestore maybe different from new version.
						faceprint::recalculate_sizeof_tcs_feature();

						int64_t server_facestore_ts = results["facestore"].asInt64();
						faceprint::load_facestore(server_facestore_ts, content + prefix_size + json_size, data_received.size() - prefix_size - json_size, true);
						faceprint::facestore_initialized = true;
						faceprint::facestore_dirty = true;
						faceprint::write_facestore_dat();
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, std::string("facestoreinit[2]: ") + err.str());
		}
		return false;
	}

	return true;
}

bool do_facestoreinit(const std::string& sessionid)
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;

	SDL_Log("[net.cpp]do_facestoreinit--- sessionid: %s", sessionid.c_str());
	net::thttp_agent agent(form_url("village/device", "init"), "POST", game_config::cert_file, TIMEOUT_30S);
	agent.did_pre = std::bind(&did_pre_facestoreinit, _2, _3, sessionid);
	agent.did_post = std::bind(&did_post_facestoreinit, _2, _3);

	bool ret = net::handle_http_request(agent);
	SDL_Log("[net.cpp]do_facestoreinit X, ret: %s", ret? "true": "false");
	return ret;
}

bool did_pre_facestoresync(net::HttpRequestHeaders& headers, std::string& body)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["timestamp"] = faceprint::facestore_ts;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_facestoresync(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;
	bool quiet = true;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, std::string("facestoresync[1]: ") + err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		if (!quiet) {
			gui2::show_message(null_str, _("Unknown error."));
		}
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		if (!quiet) {
			gui2::show_message(null_str, _("Unknown error."));
		}
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						int64_t server_facestore_ts = results["facestore"].asInt64();
						int modifies = results["modifies"].asInt();
						faceprint::sync_facestore(server_facestore_ts, content + prefix_size + json_size, data_received.size() - prefix_size - json_size, modifies, true);

						// post. write facestore.dat
						if (faceprint::facestore_dirty) {
							uint32_t now = SDL_GetTicks();
							if (faceprint::last_write_facestore_ticks == 0) {
								faceprint::last_write_facestore_ticks = now;
							}

							const int max_must_write_faces = 15000;
							bool require_write = false;
							if (faceprint::list_cs_vsize <= max_must_write_faces) {
								require_write = true;
							} else if (now - faceprint::last_write_facestore_ticks >= UINT32_C(30000) * (1 + (faceprint::list_cs_vsize - max_must_write_faces) / 5000)) {
								require_write = true;
							}

							if (require_write) {
								faceprint::write_facestore_dat();
							}
						}

					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, std::string("facestoresync[2]: ") + err.str());
		}
		return false;
	}

	return true;
}

bool do_facestoresync()
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);

	net::thttp_agent agent(form_url("village/device", "sync"), "POST", game_config::cert_file, TIMEOUT_10S);
	agent.did_pre = std::bind(&did_pre_facestoresync, _2, _3);
	agent.did_post = std::bind(&did_post_facestoresync, _2, _3);

	return net::handle_http_request(agent);
}

static const char* json_method_type[] = {
	"", // see certifiedtype_card/certifiedtype_face
	"card",
	"face"
};
static int nb_json_method_type = sizeof(json_method_type) / sizeof(json_method_type[0]);

bool did_pre_sendevent(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const tsend_event& sevent, const uint8_t* image_data, bool access)
{
	VALIDATE(image_data != nullptr && sevent.image_len > 0, null_str);
	if (sevent.method == certifiedtype_card) {
		VALIDATE(!sevent.cardnumber.empty() && sevent.facerect == empty_rect, null_str);
	} else {
		VALIDATE(sevent.method == certifiedtype_face && sevent.facerect != empty_rect, null_str);
	}
	if (access) {
		VALIDATE(current_user.valid(), null_str);
	}

	*body = nullptr;
	
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	if (access) {
		json_root["sessionid"] = current_user.sessionid;
	}
	json_root["time"] = sevent.timestamp;
	json_root["method"] = json_method_type[sevent.method];
	json_root["eventtype"] = agbox_eventcode_is_dir_in(sevent.eventcode)? "in": "out";

	if (!access || current_user.serverversion >= version_info("0.9.7")) {
		json_root["channel"] = sevent.channel;
	}
	json_root["imageformat"] = image_formats[sevent.imageformat];
	if (sevent.method == certifiedtype_face || sevent.method == certifiedtype_card) {
		Json::Value json_method;
		if (sevent.method == certifiedtype_face) {
			json_method["id"] = sevent.faceid;
			json_method["similarity"] = sevent.similarity;
			if (!access) {
				std::stringstream rect_ss;
				rect_ss << sevent.facerect.x << "," << sevent.facerect.y << "," << sevent.facerect.w << "," << sevent.facerect.h;
				json_method["rect"] = rect_ss.str();
			}
		} else {
			VALIDATE(sevent.method == certifiedtype_card, null_str);
			json_method["number"] = sevent.cardnumber;
			if (!access || current_user.serverversion >= version_info("0.9.7")) {
				json_method["faceid"] = nposm;
			}
		}
		json_root[json_method_type[sevent.method]] = json_method;
	}

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + sevent.image_len;
	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());
	memcpy(buf2 + prefix_size + json_str.size(), image_data, sevent.image_len);

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	// headers.SetHeader(net::HttpRequestHeaders::kConnection, "close");
	return true;
}

bool did_pre_sendevent099(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const tsend_event& sevent, const uint8_t* image_data)
{
	VALIDATE(image_data != nullptr && sevent.image_len > 0, null_str);
	if (sevent.method == certifiedtype_card) {
		VALIDATE(!sevent.cardnumber.empty() && sevent.facerect == empty_rect && is_valid_cardreason(sevent.cardreason), null_str);
	} else {
		VALIDATE(sevent.method == certifiedtype_face && sevent.facerect != empty_rect && sevent.cardreason == nposm, null_str);
	}
	VALIDATE(current_user.valid(), null_str);

	*body = nullptr;
	
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["time"] = sevent.timestamp;
	json_root["channel"] = sevent.channel;
	const bool idvalid = sevent.idtype != nposm && !sevent.idnumber.empty();
	json_root["idtype"] = idvalid? str_cast(sevent.idtype): null_str;
	json_root["idnumber"] = idvalid? sevent.idnumber: null_str;
	json_root["similarity"] = sevent.similarity;
	if (current_user.serverversion >= version_info("1.0.1")) {
		json_root["recognitiontime"] = sevent.recognitiontime;
	}
	if (current_user.serverversion >= version_info("1.0.3")) {
		json_root["position"] = sevent.position;
		json_root["temperature"] = sevent.temperature;
	}
	json_root["imageformat"] = image_formats[sevent.imageformat];

	int eventcode = agbox_eventcode_2_eventcode(sevent.eventcode);
	int method = sevent.method;
	std::string card = sevent.cardnumber;
	std::string note = sevent.note;
	bool xmit_image = true;
	if (method == certifiedtype_card) {
		if (sevent.cardreason == cardreason_outbutton) {
			method = certifiedtype_human;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("Out button");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_remote) {
			method = certifiedtype_human;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("Open remotely");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_stranger) {
			method = certifiedtype_face;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: stranger hover");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_strangecard) {
			method = certifiedtype_card;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: stranger hover");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_phone) {
			method = certifiedtype_phone;
			eventcode = eventcode_other * 10;
			card.clear();
			note = _("note: phone unlock");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_startbypass) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_startbypass * 10;
			note = _("note: gate bypass");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_startblock) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_startbypass * 10;
			note = _("note: gate block");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_stopbypass) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_stopbypass * 10;
			note = _("note: gate stopbypass");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_startdemolish) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: start demolish");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_stopdemolish) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: stop demolish");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_startalarm) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: start alarm");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_stopalarm) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: stop alarm");
			xmit_image = true;

		} else if (sevent.cardreason == cardreason_qrcode) {
			method = certifiedtype_qrcode;

		} else if (sevent.cardreason == cardreason_register) {
			method = certifiedtype_other;
			card.clear();
			eventcode = eventcode_other * 10;
			note = _("note: register");
			xmit_image = true;
		}
	}
	json_root["eventcode"] = eventcode;
	json_root["certifiedtypecode"] = method;
	json_root["card"] = card;
	json_root["note"] = note;

	std::string eventcode2_str;
	if (eventcode == agbox_eventcode_2_eventcode(sevent.eventcode)) {
		// doesn't update eventcode, mean in or out.
		eventcode2_str = agbox_eventcode_is_dir_in(sevent.eventcode)? "in": "out";
	}
	json_root["eventcode2"] = eventcode2_str;

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + (xmit_image? sevent.image_len: 0);
	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());
	if (xmit_image) {
		memcpy(buf2 + prefix_size + json_str.size(), image_data, sevent.image_len);
	}

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_sendevent(const net::RoseDelegate& delegate, int status)
{
	const bool quiet = true;
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_sendevent(const tsend_event& sevent, const uint8_t* image_data)
{
	VALIDATE(dtype_require_login(), null_str);
	if (is_fever(sevent.faceid, sevent.temperature)) {
		// if he is fevering, now don't send to agbox.
		return true;
	}
	tnet_send_wdg_lock wdg_lock;

	if (game_config::os == os_windows && sevent.cardreason == cardreason_stranger) {
		// ms-windows is debug platform, will always stranger, is it too event.
		// if want ms-windows to enable, remark if condition.
		int ii = 0;
		return true;
	}

	char* body = nullptr;
	net::thttp_agent agent(form_url("village/device", "event"), "POST", game_config::cert_file, TIMEOUT_6S);
	if (current_user.serverversion >= version_info("0.9.9")) {
		agent.did_pre = std::bind(&did_pre_sendevent099, _1, _2, &body, std::ref(sevent), image_data);
	} else {
		agent.did_pre = std::bind(&did_pre_sendevent, _1, _2, &body, std::ref(sevent), image_data, true);
	}
	agent.did_post = std::bind(&did_post_sendevent, _2, _3);

	bool ret = net::handle_http_request(agent);
	if (body) {
		free(body);
	}
	return ret;
}

bool did_pre_updateidnumber(net::HttpRequestHeaders& headers, std::string& body, int from_idtype, const std::string& from_idnumber, int to_idtype, const std::string& to_idnumber)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["idtype"] = from_idtype;
	json_root["idnumber"] = from_idnumber;
	json_root["idtype2"] = to_idtype;
	json_root["idnumber2"] = to_idnumber;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_updateidnumber(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					// Json::Value& results = json_object["results"];
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_updateidnumber(int from_idtype, const std::string& from_idnumber, int to_idtype, const std::string& to_idnumber, bool quiet)
{
	VALIDATE(dtype_require_login(), null_str);
	VALIDATE(current_user.valid(), null_str);

	tnet_send_wdg_lock wdg_lock;
	VALIDATE(!from_idnumber.empty() && !to_idnumber.empty(), null_str);

	net::thttp_agent agent(form_url("village/device", "updateidnumber"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&did_pre_updateidnumber, _2, _3, from_idtype, from_idnumber, to_idtype, to_idnumber);
	agent.did_post = std::bind(&did_post_updateidnumber, _2, _3, quiet);

	return net::handle_http_request(agent);
}

bool did_pre_reporterror(net::HttpRequestHeaders& headers, std::string& body, int errcode, int channel, const std::string& msg)
{
	VALIDATE(errcode >= errcode_app_min && !msg.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["time"] = (int64_t)(time(nullptr));
	json_root["errcode"] = str_cast(errcode);
	json_root["channel"] = str_cast(channel);
	json_root["msg"] = msg;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_reporterror(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	const bool quiet = true;
	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					// Json::Value& results = json_object["results"];
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_reporterror(int errcode, int channel, const std::string& msg, bool popup_dialog)
{
	if (game_config::dtype == dtype_gauth) {
		return true;
	}
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(errcode >= errcode_app_min && !msg.empty(), null_str);
	if (game_config::dtype == dtype_hvm || dtype_terminal()) {
		gui2::show_error_message(msg);
		return true;
	}

	bool ret = false;
	if (current_user.valid()) {
		net::thttp_agent agent(form_url("village/device", "reporterror"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
		agent.did_pre = std::bind(&did_pre_reporterror, _2, _3, errcode, channel, msg);
		agent.did_post = std::bind(&did_post_reporterror, _2, _3);

		ret = net::handle_http_request(agent);
	}

	if (popup_dialog) {
		// it is camera, but let hdmi knonw why, popup dialog. and will reboot 5 second.
		// if this error don't require reboot, must not set popup_dialog = true.
		tmainserial::singleton->send_wdg(5);
		gui2::show_error_message(msg);
	}
	return ret;
}

bool did_pre_getlist(net::HttpRequestHeaders& headers, std::string& body)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_getlist(const net::RoseDelegate& delegate, int status, bool quiet, tvillage& village)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& village_list = results["villageList"];
						if (village_list.isArray()) {
							VALIDATE(village_list.size() == 1, _("No village or multiple villages in the server. Ensure that exist only one valid village."));
							for (int at = 0; at < (int)village_list.size(); at ++) {
								const Json::Value& item = village_list[at];
								const std::string name = item["villageName"].asString();
								if (name.empty()) {
									gui2::show_message(null_str, _("Village name must not empty string"));
									VALIDATE(false, null_str);
								}
								village = tvillage(item["villageCode"].asString(), name);
								int64_t i64code = item["provinceCode"].asInt64();
								if (game_config::provinces.count(i64code)) {
									village.code3.province = i64code;
								}
								i64code = item["districtCode"].asInt64();
								if (game_config::districts.count(i64code)) {
									village.code3.district = &(game_config::districts.find(i64code)->second);
								}
								i64code = item["streetCode"].asInt64();
								if (game_config::streets.count(i64code)) {
									village.code3.street = &(game_config::streets.find(i64code)->second);
								}
								if (!village.code3.valid()) {
									village.code3.clear();
								}
								village.code3.address = item["address"].asString();
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !village.valid()) {
		err << _("Unknown error.");
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_getlist(tvillage& village, bool quiet)
{
	if (dtype_multivillage()) {
		return multivlg_do_getlist(village, quiet);
	}

	VALIDATE(dtype_require_login(), null_str);
	village.clear();

	const int timeout = game_config::dtype == dtype_access? TIMEOUT_NORMAL: nposm;
	net::thttp_agent agent(form_url("village/device", "getList"), "POST", game_config::cert_file, timeout);
	agent.did_pre = std::bind(&did_pre_getlist, _2, _3);
	agent.did_post = std::bind(&did_post_getlist, _2, _3, quiet, std::ref(village));

	return net::handle_http_request(agent);
}

bool did_pre_getbuilding(net::HttpRequestHeaders& headers, std::string& body, const std::string& villagecode)
{
	VALIDATE(!villagecode.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["villageCode"] = villagecode;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_getbuilding(const net::RoseDelegate& delegate, int status, bool quiet, std::vector<tbuilding>& buildings)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& building_list = results["buildingList"];
						if (building_list.isArray()) {
							for (int at = 0; at < (int)building_list.size(); at ++) {
								const Json::Value& item = building_list[at];
								const std::string code = item["buildingCode"].asString();
								buildings.push_back(tbuilding(code, item["buildingNo"].asString(), item["floorTotal"].asInt(), item["hidden"].asBool()));
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_getbuilding(const std::string& villagecode, std::vector<tbuilding>& buildings, bool quiet)
{
	buildings.clear();
	if (dtype_multivillage()) {
		return multivlg_do_getbuilding(villagecode, buildings, quiet);
	}

	VALIDATE(dtype_require_login(), null_str);

	const int timeout = game_config::dtype == dtype_access? TIMEOUT_10S: nposm;
	net::thttp_agent agent(form_url("village/device", "getBuilding"), "POST", game_config::cert_file, timeout);
	agent.did_pre = std::bind(&did_pre_getbuilding, _2, _3, std::ref(villagecode));
	agent.did_post = std::bind(&did_post_getbuilding, _2, _3, quiet, std::ref(buildings));

	return net::handle_http_request(agent);
}

bool did_pre_gethouse(net::HttpRequestHeaders& headers, std::string& body, const std::string& buildingcode)
{
	VALIDATE(!buildingcode.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["buildingCode"] = buildingcode;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_gethouse(const net::RoseDelegate& delegate, int status, bool quiet, std::vector<thouse>& houses)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& house_list = results["houseList"];
						if (house_list.isArray()) {
							for (int at = 0; at < (int)house_list.size(); at ++) {
								const Json::Value& item = house_list[at];
								houses.push_back(thouse(item["houseCode"].asString(), item["houseNo"].asString(), item["floor"].asInt()));

							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_gethouse(const std::string& buildingcode, std::vector<thouse>& houses, bool quiet)
{
	houses.clear();
	if (dtype_multivillage()) {
		return multivlg_do_gethouse(buildingcode, houses, quiet);
	}

	VALIDATE(dtype_require_login(), null_str);

	const int timeout = game_config::dtype == dtype_access? TIMEOUT_10S: nposm;
	net::thttp_agent agent(form_url("village/device", "getHouse"), "POST", game_config::cert_file, timeout);
	agent.did_pre = std::bind(&did_pre_gethouse, _2, _3, std::ref(buildingcode));
	agent.did_post = std::bind(&did_post_gethouse, _2, _3, quiet, std::ref(houses));

	return net::handle_http_request(agent);
}

bool did_pre_getentrance(net::HttpRequestHeaders& headers, std::string& body, const std::string& villagecode)
{
	VALIDATE(!villagecode.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["villageCode"] = villagecode;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_getentrance(const net::RoseDelegate& delegate, int status, bool quiet, std::vector<tentrance>& entrances)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& entrance_list = results["entranceList"];
						if (entrance_list.isArray()) {
							for (int at = 0; at < (int)entrance_list.size(); at ++) {
								const Json::Value& item = entrance_list[at];
								entrances.push_back(tentrance(item["entranceCode"].asString(), item["name"].asString()));
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !current_operator.valid()) {
		err << _("Unknown error.");
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_getentrance(const std::string& villagecode, std::vector<tentrance>& entrances, bool quiet)
{
	VALIDATE(current_operator.valid(), null_str);
	entrances.clear();
	if (dtype_multivillage()) {
		return multivlg_do_getentrance(villagecode, entrances, quiet);
	}

	VALIDATE(dtype_require_login(), null_str);

	net::thttp_agent agent(form_url("village/device", "getEntrance"), "POST", game_config::cert_file, nposm);
	agent.did_pre = std::bind(&did_pre_getentrance, _2, _3, std::ref(villagecode));
	agent.did_post = std::bind(&did_post_getentrance, _2, _3, quiet, std::ref(entrances));

	return net::handle_http_request(agent);
}

bool did_pre_getmisccode(net::HttpRequestHeaders& headers, std::string& body, const std::string& category)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	if (current_user.serverversion >= version_info("1.0.3")) {
		json_root["category"] = category;
	}

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_getmisccode(const net::RoseDelegate& delegate, int status, tmisccode& misccode, int64_t* last_datadictionary_ts)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						*last_datadictionary_ts = results["datadictionary"].asInt64();
						Json::Value& person_list = results["personType"];
						if (person_list.isArray()) {
							for (int at = 0; at < (int)person_list.size(); at ++) {
								const Json::Value& item = person_list[at];
								const int code = item["code"].asInt();
								const std::string name = code != game_config::blacklist_persontype? item["name"].asString(): _("Blacklist");
								misccode.persontypes.insert(std::make_pair(code, tpersontypecode(code, name, item["category"].asInt())));
							}
						}
						Json::Value& people_list = results["peopleType"];
						if (people_list.isArray()) {
							for (int at = 0; at < (int)people_list.size(); at ++) {
								const Json::Value& item = people_list[at];
								misccode.peopletypes.insert(std::make_pair(item["code"].asInt(), tpeopletypecode(item["code"].asInt(), item["name"].asString(), item["category"].asInt())));
							}
						}
						Json::Value& houserel_list = results["houseRel"];
						if (houserel_list.isArray()) {
							for (int at = 0; at < (int)houserel_list.size(); at ++) {
								const Json::Value& item = houserel_list[at];
								misccode.houserels.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& platetype_list = results["plateType"];
						if (platetype_list.isArray()) {
							for (int at = 0; at < (int)platetype_list.size(); at ++) {
								const Json::Value& item = platetype_list[at];
								misccode.platetypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& cartype_list = results["carType"];
						if (cartype_list.isArray()) {
							for (int at = 0; at < (int)cartype_list.size(); at ++) {
								const Json::Value& item = cartype_list[at];
								misccode.cartypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& credential_list = results["credentialType"];
						if (credential_list.isArray()) {
							for (int at = 0; at < (int)credential_list.size(); at ++) {
								const Json::Value& item = credential_list[at];
								misccode.idtypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
							if (misccode.idtypes.count(gat517_openid) == 0) {
								misccode.idtypes.insert(std::make_pair(gat517_openid, _("WeChat Openid")));
							}
						}
						Json::Value& certified_list = results["certifiedType"];
						if (certified_list.isArray()) {
							for (int at = 0; at < (int)certified_list.size(); at ++) {
								const Json::Value& item = certified_list[at];
								misccode.certifiedtypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& event_list = results["eventCode"];
						if (event_list.isArray()) {
							for (int at = 0; at < (int)event_list.size(); at ++) {
								const Json::Value& item = event_list[at];
								misccode.eventcodes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& nation_list = results["nation"];
						if (nation_list.isArray()) {
							// early villageserver hasn't nation block.
							int size = nation_list.size();
							for (int at = 0; at < (int)nation_list.size(); at ++) {
								const Json::Value& item = nation_list[at];
								int code = utils::to_int(item["code"].asString());
								VALIDATE(code >= 1, "Invalid nation code form agbox");
								misccode.nations.insert(std::make_pair(code, item["name"].asString()));
							}
						}
						Json::Value& group_list = results["accessGroup"];
						if (group_list.isArray()) {
							for (int at = 0; at < (int)group_list.size(); at ++) {
								const Json::Value& item = group_list[at];
								misccode.groups.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& education_list = results["education"];
						if (education_list.isArray()) {
							for (int at = 0; at < (int)education_list.size(); at ++) {
								const Json::Value& item = education_list[at];
								misccode.educations.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& marital_list = results["marital"];
						if (marital_list.isArray()) {
							for (int at = 0; at < (int)marital_list.size(); at ++) {
								const Json::Value& item = marital_list[at];
								misccode.maritals.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& cardtype_list = results["cardType"];
						if (cardtype_list.isArray()) {
							for (int at = 0; at < (int)cardtype_list.size(); at ++) {
								const Json::Value& item = cardtype_list[at];
								misccode.cardtypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
						}
						Json::Value& parking_list = results["parking"];
						if (parking_list.isArray()) {
							for (int at = 0; at < (int)parking_list.size(); at ++) {
								const Json::Value& item = parking_list[at];
								const std::string code = item["code"].asString();
								if (!code.empty()) {
									misccode.parkings.insert(std::make_pair(code, item["name"].asString()));
								}
							}
						}
						Json::Value& nationality_list = results["nationality"];
						if (nationality_list.isArray()) {
							for (int at = 0; at < (int)nationality_list.size(); at ++) {
								const Json::Value& item = nationality_list[at];
								const std::string code = item["code"].asString();
								if (!code.empty()) {
									misccode.nationalities.insert(std::make_pair(code, item["name"].asString()));
								}
							}
						}
						Json::Value& cause_list = results["cause"];
						if (cause_list.isArray()) {
							for (int at = 0; at < (int)cause_list.size(); at ++) {
								const Json::Value& item = cause_list[at];
								misccode.causes.push_back(item["name"].asString());
							}
						}
						Json::Value& note_list = results["note"];
						if (note_list.isArray()) {
							for (int at = 0; at < (int)note_list.size(); at ++) {
								const Json::Value& item = note_list[at];
								misccode.notes.push_back(item["name"].asString());
							}
						}
						Json::Value& lift_list = results["lift"];
						if (lift_list.isArray()) {
							std::map<int, std::string> raw_lifts;
							for (int at = 0; at < (int)lift_list.size(); at ++) {
								const Json::Value& item = lift_list[at];
								raw_lifts.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
							}
							std::vector<std::string> vstr, fields;
							for (std::map<int, std::string>::const_iterator it = raw_lifts.begin(); it != raw_lifts.end(); ++ it) {
								const std::string& floors = it->second;
								vstr = utils::split(floors, ';');
								for (std::vector<std::string>::const_iterator it2 = vstr.begin(); it2 != vstr.end(); ++ it2) {
									fields = utils::split(*it2, ',');
									if (fields.size() != 2 || !utils::is_guid(fields[0], true) || !utils::isinteger(fields[1])) {
										continue;
									}
									const std::string key = utils::join_int_string(utils::to_int(fields[1]), utils::lowercase(fields[0]), '-');
									if (misccode.lifts.count(key) == 0) {
										misccode.lifts.insert(std::make_pair(key, it->first));
									}
								}
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

static bool do_getmisccode_internal(tmisccode& misccode, const std::string& category, int64_t& last_datadictionary_ts)
{
	// VALIDATE(current_operator.valid(), null_str);
	misccode.clear();
	last_datadictionary_ts = 0;

	if (dtype_multivillage()) {
		return multivlg_do_getmisccode_internal(misccode, category, last_datadictionary_ts);
	}
	VALIDATE(dtype_require_login(), null_str);

	net::thttp_agent agent(form_url("village/device", "getMiscCode"), "POST", game_config::cert_file, TIMEOUT_15S);
	agent.did_pre = std::bind(&did_pre_getmisccode, _2, _3, category);
	agent.did_post = std::bind(&did_post_getmisccode, _2, _3, std::ref(misccode), &last_datadictionary_ts);

	return net::handle_http_request(agent);
}

bool do_getmisccode(tmisccode& misccode)
{
	int64_t last_datadictionary_ts;
	return do_getmisccode_internal(misccode, null_str, last_datadictionary_ts);
}

bool do_getliftcode(std::map<std::string, int>& lifts, int64_t& datadictionary_ts)
{
	if (current_user.serverversion < version_info("1.0.3")) {
		lifts.clear();
		datadictionary_ts = 0;
		return true;
	}
	std::map<int, tpersontypecode> persontypes;
	std::map<int, tpeopletypecode> peopletypes;
	std::map<int, std::string> houserels;
	std::map<int, std::string> platetypes;
	std::map<int, std::string> cartypes;
	std::map<int, std::string> idtypes;
	std::map<int, std::string> certifiedtypes;
	std::map<int, std::string> eventcodes;
	std::map<int, std::string> groups;
	std::map<int, std::string> educations;
	std::map<int, std::string> maritals;
	std::map<int, std::string> cardtypes;
	std::map<std::string, std::string> parkings;
	std::map<std::string, std::string> nationalities;
	std::vector<std::string> causes;
	std::vector<std::string> notes;
	std::map<int, std::string> nations;

	net::tmisccode misccode(persontypes, peopletypes, houserels, platetypes, cartypes, idtypes, certifiedtypes, eventcodes, nations, groups, educations, maritals, cardtypes, parkings, nationalities, causes, notes, lifts);
	return do_getmisccode_internal(misccode, "lift", datadictionary_ts);
}

bool did_pre_requesttask(net::HttpRequestHeaders& headers, std::string& body)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;
	json_root["type"] = "feature";

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_requesttask(const net::RoseDelegate& delegate, int status, std::vector<std::unique_ptr<tfeature_task> >& tasks, threading::mutex& mutex, int max_once_feature_tasks, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		if (!quiet) {
			gui2::show_message(null_str, _("Unknown error."));
		}
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		if (!quiet) {
			gui2::show_message(null_str, _("Unknown error."));
		}
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						Json::Value& feature_array = results["features"];
						if (feature_array.isArray()) {
							const int images_size = content_size - prefix_size - json_size;
							threading::lock lock(mutex);
							for (int at = 0; at < (int)feature_array.size() && (int)tasks.size() < max_once_feature_tasks; at ++) {
								const Json::Value& item = feature_array[at];
								std::string name = current_user.serverversion >= version_info("1.0.4")? item["name"].asString(): "fakename";
								std::string idtype = item["idtype"].asString();
								std::string idnumber = item["idnumber"].asString();
								int nonce = item["nonce"].asInt();
								int imageoffset = item["imageoffset"].asInt();
								int imagesize = item["imagesize"].asInt();

								int errcode = featuretaskerrcode_ok;
								surface surf;
								if (idtype.empty() || idnumber.empty()) {
									errcode = featuretaskerrcode_idfield;

								} else if (name.empty()) {
									errcode = featuretaskerrcode_nameempty;

								} else if (!utils::is_utf8str(name.c_str(), name.size())) {
									errcode = featuretaskerrcode_nameutf8;

								} else if (imageoffset < 0 || imageoffset >= images_size || imagesize <= 0  || imageoffset + imagesize > images_size) {
									// image file size maybe 0, server don't judge it. app accept it, and reqport error. 
									errcode = featuretaskerrcode_imagefield;

								} else {
									surf = imread_mem(content + prefix_size + json_size + imageoffset, imagesize);
									if (game_config::os == os_windows) {
										// surf = nullptr;
										int ii = 0;
									}
									if (!surf) {
										errcode = featuretaskerrcode_imageformat;
									}
								}
								tasks.push_back(std::unique_ptr<tfeature_task>(new tfeature_task(idtype, idnumber, nonce, surf, errcode)));
							}
						}

					} else {
						err << _("device/queryface. Json data error. No results object.");
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_request_task(std::vector<std::unique_ptr<tfeature_task> >& tasks, threading::mutex& mutex, int max_once_feature_tasks, bool quiet)
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(tasks.empty(), null_str);

	net::thttp_agent agent(form_url("village/device", "requesttask"), "POST", game_config::cert_file, TIMEOUT_15S);
	agent.did_pre = std::bind(&did_pre_requesttask, _2, _3);
	agent.did_post = std::bind(&did_post_requesttask, _2, _3, std::ref(tasks), std::ref(mutex), max_once_feature_tasks, quiet);

	return net::handle_http_request(agent);
}

bool did_pre_taskfinished(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const std::vector<std::unique_ptr<tfeature_task> >& tasks, threading::mutex& mutex)
{
	VALIDATE(!tasks.empty(), null_str);

	threading::lock lock(mutex);

	*body = nullptr;

	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["sessionid"] = current_user.sessionid;

	int offset = 0;
	Json::Value json_tasks;
	for (std::vector<std::unique_ptr<tfeature_task> >::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const tfeature_task& task = **it;
		VALIDATE(task.handled(), null_str);
		bool ok = task.feature_size > 0;

		Json::Value json_task;
		json_task["idtype"] = task.idtype;
		json_task["idnumber"] = task.idnumber;
		json_task["nonce"] = task.nonce;

		json_task["featureoffset"] = ok? offset: 0;
		json_task["featuresize"] = task.feature_size;

		if (current_user.serverversion >= version_info("1.0.4")) {
			json_task["errcode"] = task.errcode;
		}

		json_tasks.append(json_task);
		
		if (ok) {
			VALIDATE(task.errcode == featuretaskerrcode_ok, null_str);
			VALIDATE(task.feature_data.get() != nullptr, null_str);
			offset += task.feature_size + SHA_DIGEST_LENGTH;

		} else {
			VALIDATE(task.errcode != featuretaskerrcode_ok, null_str);
			VALIDATE(task.feature_data.get() == nullptr, null_str);
		}
	}
	json_root["features"] = json_tasks;

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + offset;
	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());

	int offset2 = 0;
	for (std::vector<std::unique_ptr<tfeature_task> >::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const tfeature_task& task = **it;
		bool ok = task.feature_size > 0;
		if (!ok) {
			continue;
		}
		// feature
		memcpy(buf2 + prefix_size + json_str.size() + offset2, task.feature_data.get(), task.feature_size);
		// sha1
		std::unique_ptr<uint8_t[]> md = utils::sha1((const uint8_t*)task.feature_data.get(), task.feature_size);
		memcpy(buf2 + prefix_size + json_str.size() + offset2 + task.feature_size, md.get(), SHA_DIGEST_LENGTH);

		offset2 += task.feature_size + SHA_DIGEST_LENGTH;
	}
	VALIDATE(offset == offset2, null_str);

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_taskfinished(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	const std::string& data_received = delegate.data_received();
	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	return true;
}

bool do_task_finished(const std::vector<std::unique_ptr<tfeature_task> >& tasks, threading::mutex& mutex, bool quiet)
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);

	char* body = nullptr;
	net::thttp_agent agent(form_url("village/device", "taskfinished"), "POST", game_config::cert_file, TIMEOUT_10S);
	agent.did_pre = std::bind(&did_pre_taskfinished, _1, _2, &body, std::ref(tasks), std::ref(mutex));
	agent.did_post = std::bind(&did_post_taskfinished, _2, _3, quiet);

	bool ret = net::handle_http_request(agent);
	if (body) {
		free(body);
	}
	return ret;
}

bool did_pre_agbox_updateperson(net::HttpRequestHeaders& headers, std::string& body, const tperson& person)
{
	VALIDATE(person.feature && person.feature_size == FEATURE_BLOB_SIZE && person.image_data && person.image_len > 0, null_str);
	if (person.idtype == gat517_idcard) {
		VALIDATE(!person.idnumber.empty(), null_str);

	} else if (person.idtype == gat517_tempo_pass) {
		VALIDATE(person.idtype == gat517_tempo_pass, null_str);
		VALIDATE(person.idnumber.empty(), null_str);
	}
	if (person.visitor) {
		VALIDATE(!person.signout, null_str);
		// avm maybe no intervieweeid in sms scene.
		// VALIDATE(person.intervieweeid != nposm, null_str);
		VALIDATE(!person.code3.valid() && !person.residence_code3.valid(), null_str);

	} else {
		// VALIDATE(person.intervieweeid == nposm, null_str);
		VALIDATE(!person.onceonly, null_str);
	}

	std::stringstream ss;

	Json::Value json_root;
	json_root["credentialType"] = person.idtype;
	json_root["credentialNo"] = person.idnumber;

	json_root["peopleName"] = utils::truncate_to_max_bytes(person.name.c_str(), person.name.size(), MAX_AGBOX_NAME_SIZE);
	json_root["personTypeCode"] = person.type;
	json_root["peopleTypeCode"] = person.peopletype;
	json_root["securityCardNo"] = person.card.number;
	json_root["picUrl"] = "images/visitor090807060504030201.jpg";
	// json_root["picUrl"] = "images/owner-noface.jpg";
	json_root["visitor"] = person.visitor;
	json_root["villageCode"] = "9d9919de-2b37-4c35-88dc-176d3056e103";
	json_root["phones"] = person.phones;

	if (!person.houses.empty()) {
		Json::Value json_houses, json_house;
		for (std::vector<tcs_house>::const_iterator it = person.houses.begin(); it != person.houses.end(); ++ it) {
			const tcs_house& house = *it;
			json_house["houseCode"] = house.house;
			json_house["houseRelation"] = house.houserel;
			json_houses.append(json_house);
		}
		json_root["house"] = json_houses;
	}

	if (!person.cars.empty()) {
		Json::Value json_cars, json_car;
		for (std::vector<tcs_car>::const_iterator it = person.cars.begin(); it != person.cars.end(); ++ it) {
			const tcs_car& car = *it;
			json_car["plateNo"] = car.no;
			json_car["plateTypeCode"] = car.platetype;
			json_car["carTypeCode"] = car.cartype;
			json_cars.append(json_car);
		}
		json_root["car"] = json_cars;
	}

	Json::Value json_domicile;
	json_domicile["nationCode"] = nationcode_2_nation(person.nation);
	json_domicile["genderCode"] = person.gender;
	json_domicile["address"] = person.code3.address;
	if (game_config::provinces.count(person.code3.province) != 0) {
		json_domicile["provinceCode"] = person.code3.province;
	}
	if (person.code3.district != nullptr) {
		json_domicile["cityCode"] = person.code3.district->city_code;
		json_domicile["districtCode"] = person.code3.district->code;
	}
	if (person.code3.street != nullptr) {
		// json_domicile["streetCode"] = person.code3.street->code;
	}
	json_root["domicile"] = json_domicile;


	json_root["intervieweeCredentialType"] = gat517_idcard;
	json_root["intervieweeCredentialNo"] = "123456781";
	json_root["secondAuth"] = true;

	json_root["startTime"] = format_time_ymdhms3(person.startline + 120);
	json_root["expiryTime"] = format_time_ymdhms3(person.deadline);

	Json::FastWriter writer;
	body = writer.write(json_root);
	
	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_agbox_updateperson(const net::RoseDelegate& delegate, int status, const tperson& person)
{
	std::stringstream err;

	const std::string& data_received = delegate.data_received();
	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					if (code != ERRCODE_PERSON_EXISTED) {
						err << json_object["msg"].asString();
						if (err.str().empty()) {
							err << _("Unknown error.");
						}
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_agbox_updateperson(const tperson& person)
{
	VALIDATE(dtype_require_login(), null_str);
	tnet_send_wdg_lock wdg_lock;
	VALIDATE(current_user.valid(), null_str);

	int timeout = game_config::dtype == dtype_access? 5000: nposm;
	net::thttp_agent agent(form_url("village/agbox", "updateperson"), "POST", game_config::cert_file, timeout);
	agent.did_pre = std::bind(&did_pre_agbox_updateperson, _2, _3, std::ref(person));
	agent.did_post = std::bind(&did_post_agbox_updateperson, _2, _3, std::ref(person));

	return net::handle_http_request(agent);
}

//
// ipc
//
std::string ipc_form_url(const std::string& hostport, const std::string& category, const std::string& task)
{
	std::stringstream url;

	url << hostport << "/" << category << "/";
	url << task;

	return url.str();
}

bool did_pre_facedetected(net::HttpRequestHeaders& headers, std::string& body)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_facedetected(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool ipc_do_facedetected(const std::string& url, bool quiet)
{
	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(ipc_form_url(url, "access", "facedetected"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&did_pre_facedetected, _2, _3);
	agent.did_post = std::bind(&did_post_facedetected, _2, _3, quiet);

	return net::handle_http_request(agent);
}

bool did_post_validface(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool ipc_do_sendevent(const std::string& url, bool quiet, const tsend_event& sevent, const uint8_t* image_data)
{
	tnet_send_wdg_lock wdg_lock;

	char* body = nullptr;
	net::thttp_agent agent(ipc_form_url(url, "access", "validface"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&did_pre_sendevent, _1, _2, &body, std::ref(sevent), image_data, false);
	agent.did_post = std::bind(&did_post_validface, _2, _3, quiet);

	bool ret = net::handle_http_request(agent);
	if (body) {
		free(body);
	}
	return ret;
}

bool did_pre_hoverface(net::HttpRequestHeaders& headers, std::string& body, int64_t timestamp)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["time"] = timestamp;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_hoverface(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool ipc_do_hoverface(const std::string& url, bool quiet, int64_t timestamp)
{
	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(ipc_form_url(url, "access", "hoverface"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&did_pre_hoverface, _2, _3, timestamp);
	agent.did_post = std::bind(&did_post_hoverface, _2, _3, quiet);

	return net::handle_http_request(agent);
}


//
// special for gate
//

bool gate_did_pre_unlock(net::HttpRequestHeaders& headers, std::string& body, int channel, int reason)
{
	Json::Value json_root;
	json_root["version"] = game_config::version.str(false);
	json_root["channel"] = channel;
	json_root["reason"] = game_config::cardreasons.find(reason)->second;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool gate_did_post_unlock(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool gate_do_unlock(const std::string& url, int channel, int reason, bool quiet)
{
	VALIDATE(!url.empty(), null_str);
	VALIDATE(channel == 1 || channel == 2, null_str);
	VALIDATE(game_config::cardreasons.count(reason) != 0, null_str);

	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(ipc_form_url(url, "access", "unlock"), "POST", game_config::cert_file, TIMEOUT_NORMAL);
	agent.did_pre = std::bind(&gate_did_pre_unlock, _2, _3, channel, reason);
	agent.did_post = std::bind(&gate_did_post_unlock, _2, _3, quiet);

	return net::handle_http_request(agent);
}

//
// groupauto
//
std::string boundary_formdata_prefix(const std::string& boundary)
{
	std::stringstream ss;
	ss << "--" << boundary << "\r\n";
	ss << "Content-Disposition: form-data; name=";
	return ss.str();
}

std::string boundary_formdata_postfix(const std::string& boundary)
{
	std::stringstream ss;
	ss << "--" << boundary << "--\r\n";
	return ss.str();
}

std::string boundary_formdata_end_item(const std::string& boundary)
{
	std::stringstream ss;
	ss <<  "\r\n--" << boundary;
	return ss.str();
}

std::string formdata_body(const std::string& boundary, const std::map<std::string, std::string>& data)
{
	VALIDATE(!data.empty(), null_str);
	std::stringstream result_ss;
	const std::string prefix = boundary_formdata_prefix(boundary);
	for (std::map<std::string, std::string>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		result_ss << prefix << "\"" << it->first << "\"" << "\r\n\r\n";
		result_ss << it->second << "\r\n";
	}
	const std::string postfix = boundary_formdata_postfix(boundary);
	result_ss << postfix;
	return result_ss.str();
}

static std::string gauth_handle_response(const std::string& url, const std::string& method, const Json::Value& error_object, const Json::Value& result_object)
{
	std::stringstream err;
	utils::string_map symbols;
	
	if (error_object.isObject()) {
		const int code = error_object["code"].asInt();
		const std::string msg = error_object["message"].asString();
		err << "[" << code << "] ";
		if (!msg.empty()) {
			err << msg;
		} else {
			err << _("Unknown error");
		}
	} else if (!result_object.isObject()) {
		if (result_object.empty() || result_object.asString() != "success") {
			err << _("Except error, result neither object or value of success");
		}
	}

	if (false) {
		symbols["app"] = game_config::get_app_msgstr(null_str);
		err << vgettext2("The version is too low, please upgrade $app.", symbols);
	}

	if (!err.str().empty()) {
		err << "\n";
		err << url << " method:" << method;
	}
	return err.str();
}

std::string gauth_handle_response2(const Json::Value& error_object, const std::string& result)
{
	std::stringstream err;
	utils::string_map symbols;
	
	if (error_object.isObject()) {
		const int code = error_object["code"].asInt();
		const std::string msg = error_object["message"].asString();
		err << "[" << code << "] ";
		if (!msg.empty()) {
			err << msg;
		} else {
			err << _("Unknown error");
		}
	} else if (result != "success") {
		err << "Except error, result value isn't success";
	}

	return err.str();
}

const std::string formdata_boundary = "----RoseFormBoundaryrGKCBY7qhFd3TrwA";

bool gauth_did_pre_login(net::HttpRequestHeaders& headers, std::string& body, std::string& request_json)
{
	Json::Value json_root;
	json_root["jsonrpc"] = "2.0";
	json_root["method"] = "getGroupFuction";
	json_root["id"] = "63196C79A4FB453E9BF173D30E497A51";

	Json::Value json_params;
	// json_params["id"] = sevent.faceid;
	// json_params["similarity"] = sevent.similarity;
	// json_root["params"] = json_params;

	std::map<std::string, std::string> data;
	data.insert(std::make_pair("key", "f2c8f44c-a5d4-42f5-9784-360b08723dbb"));

	Json::FastWriter writer;
	request_json = writer.write(json_root);
	data.insert(std::make_pair("json", request_json));

	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;
	headers.SetHeader(net::HttpRequestHeaders::kContentType, contenttype_value);

	return true;
}

bool gauth_undownload_person_2_feature_task(gui2::tprogress_& progress)
{
	// current_user.undownload_persons ==> game_config::gauth_feature_tasks
	VALIDATE(current_user.valid() && !current_user.undownload_persons.empty(), null_str);

	tnet_send_wdg_lock wdg_lock;
	threading::lock lock(game_config::gauth_feature_tasks_mutex);
	std::vector<std::unique_ptr<tfeature_task> >& tasks = game_config::gauth_feature_tasks;

	std::stringstream ss;
	const int original_unfeature_person_size = current_user.undownload_persons.size();
	const int original_task_size = tasks.size();
	int person_at = 0;

	for (std::map<std::string, tgauth_person>::const_iterator it = current_user.undownload_persons.begin(); it != current_user.undownload_persons.end(); person_at ++) {
		const tgauth_person& person = it->second;

		surface surf;
		if (game_config::no_face_types.count(person.function) == 0) {
			ss.str("");
			ss << "#" << (person_at + 1) << "/" << original_unfeature_person_size << "  " << person.name << "(id: " << person.id << ")" << extract_file(person.url);
			progress.set_message(ss.str());
			progress.set_percentage(100 * person_at / original_unfeature_person_size);

			if (person.url.empty()) {
				// url is empty, it is invalid person.
				current_user.undownload_persons.erase(it ++);
				continue;
			}

			bool found = false;
			int at = 0;
			for (std::vector<std::unique_ptr<tfeature_task> >::const_iterator it2 = tasks.begin(); at < original_task_size && it2 != tasks.end(); ++ it2, at ++) {
				const tfeature_task& task = *(*it2).get();
				if (person.id == task.idnumber) {
					found = true;
					break;
				}
			}
			if (found) {
				current_user.undownload_persons.erase(it ++);
				continue;
			}

			const bool image_from_agbox = true;
			if (game_config::os != os_windows || image_from_agbox) {
				const int timeout = 10000; // 10 second
				std::string image_data;
				bool ret = net::fetch_url_2_mem(person.url, image_data, timeout);
				if (!ret) {
					// get url's image fail, try get again later.
					++ it;
					continue;
				}

				if (image_data.size() > 0) {
					surf = imread_mem(image_data.c_str(), image_data.size());
				}

			} else {
				surf = image::get_image("misc/st_fake_image.jpg");
			}
			if (surf.get() == nullptr) {
				// url isn't empty, but isn't valid image.
				current_user.undownload_persons.erase(it ++);
				continue;
			}

		} else {
			surf = image::get_image(std::string("misc/") + game_config::st_fake_image_jpg);
		}
		VALIDATE(surf.get(), null_str);

		tfeature_task* task = new tfeature_task(str_cast(gat517_idcard), person.id, 0, surf, featuretaskerrcode_ok);
		task->idtype2 = person.idtype;
		task->card = person.card;
		task->name = person.name;
		task->persontype = person.function;
		task->updatetime = person.updatetime;
		tasks.push_back(std::unique_ptr<tfeature_task>(task));
		current_user.undownload_persons.erase(it ++);
	}
	return true;
}

std::pair<int, int> parse_relaydelay(const std::string& str)
{
	const std::pair<int, int> def_result(DEFAULT_OPENTIME, DEFAULT_OPENTIME);
	if (str.empty()) {
		return def_result;
	}
	std::pair<int, int> result(0, 0);
	std::vector<std::string> vstr = utils::split(str);
	if (vstr.size() >= 2) {
		result.first = utils::to_int(vstr[0]);
		result.second = utils::to_int(vstr[1]);

	} else {
		VALIDATE(vstr.size() == 1, null_str);
		result.first = utils::to_int(vstr[0]);
		result.second = result.first;
	}

	return result;
}

bool gauth_did_post_login(const net::RoseDelegate& delegate, int status, const std::string& devicecode, bool quiet, const std::string& request_json, std::string& invalid_field_msg, int64_t* server_time)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			const Json::Value& error_object = json_object["error"];
			Json::Value& result = json_object["result"];
			err << gauth_handle_response("agbox/device/gauth/ICCard", "getGroupFuction", error_object, result);

			if (err.str().empty()) {
				if (game_config::os == os_windows) {
					std::stringstream file_ss;
					tfile file(game_config::preferences_dir + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
					VALIDATE(file.valid(), null_str);
					file_ss << "---request json---\n" << request_json << "\n\n" << "---response json---\n";
					posix_fwrite(file.fp, file_ss.str().c_str(), file_ss.str().size());

					posix_fwrite(file.fp, data_received.c_str(), data_received.size());
				}

				std::vector<tgauth_function> functions;
				Json::Value& function_list = result["functions"];
				if (function_list.isArray()) {
					for (int at = 0; at < (int)function_list.size(); at ++) {
						const Json::Value& item = function_list[at];
						const int function = item["function"].asInt();
						std::string group_str = item["group"].asString();
						if (game_config::os == os_windows) {
							if (function == 3) {
								// group_str = "-1";
							} else if (function == 14) {
								int ii = 0;
								group_str = "13";
							}
						}
						functions.push_back(tgauth_function(function, 
							item["actionName"].asString(), item["leader"].asInt(), item["leaderChkFace"].asBool(), item["name"].asString(),
							item["groupEnabled"].asBool(), item["carType"].asInt(),
							item["action"].asInt(), group_str));
						if (game_config::os == os_windows) {
							if (function == 2) {
								// functions.back().groupenabled = false;
							} else if (function == 3) {
								// functions.back().groupenabled = true;
							} else if (function == 14) {
								int ii = 0;
								// functions.back().cartype = 12;
							}
							// functions.back().cartype = 12;
						}
					}
				}

				if (!functions.empty()) {
					// current_user.functions don't clear when current_user.clear().
					// so here evaluate again.
					current_user.functions = functions;
					preferences::set_gauth_functions(current_user.functions);

					const std::string gauth_sessionid = "1234355";
					current_user.devicecode = devicecode;
					current_user.sessionid = gauth_sessionid;
					current_user.serverversion = version_info(MAX_VILLAGED_VERSION);

					Json::Value& deviceset = result["deviceSet"];
					if (deviceset.isObject()) {
						const std::string password = deviceset["pwd"].asString();
						if (!password.empty()) {
							preferences::set_password(password);
						}

						current_user.keepalive = deviceset["heart"].asInt();
						if (current_user.keepalive < 10 || current_user.keepalive > 600) {
							current_user.keepalive = DEFAULT_KEEPALIVE;
						}

						const int original_inputcardtype = current_user.inputcardtype;
						current_user.inputcardtype = deviceset["cardType"].asInt();
						if (!is_valid_inputcardtype(current_user.inputcardtype)) {
							current_user.inputcardtype = inputcardtype_wg34base10;
						}

						current_user.gauthscene = deviceset["sceneCode"].asInt();
						if (!is_valid_gauthscene(current_user.gauthscene)) {
							current_user.gauthscene = gauthscene_clearroom;
						}

						current_user.cardstrategy = deviceset["cardStrategy"].asInt();
						if (!is_valid_cardstrategy(current_user.cardstrategy)) {
							current_user.cardstrategy = cardstrategy_nocard;
						}

						const std::string relaydelay_str = deviceset["relayDelay"].asString();
						std::pair<int, int> relaydelays = parse_relaydelay(relaydelay_str);
						current_user.opentime = relaydelays.first;
						current_user.opentime2 = relaydelays.second;
						if (current_user.opentime < MIN_OPENTIME || current_user.opentime > MAX_OPENTIME) {
							current_user.opentime = DEFAULT_OPENTIME;
						}
						if (current_user.opentime2 < MIN_OPENTIME || current_user.opentime2 > MAX_OPENTIME) {
							current_user.opentime2 = DEFAULT_OPENTIME;
						}
						current_user.comparethreshold = deviceset["similarityThreshold"].asInt();
						if (current_user.comparethreshold < 1 || current_user.comparethreshold > 99) {
							current_user.comparethreshold = DEFAULT_COMPARETHRESHOLD;
						}
						current_user.areathreshold = deviceset["areaThreshold"].asInt();
						if (current_user.areathreshold < 0 || current_user.areathreshold > 100) {
							current_user.areathreshold = DEFAULT_AREATHRESHOLD;
						}
						current_user.liveness = deviceset["liveness"].asBool();
						
						// output cardtype
						enum {wiegandtype_26 = 1, wiegandtype_34 = 2};
						int wiegandtype = deviceset["wiegandType"].asInt();
						bool le = deviceset["littleEndian"].asBool();
						if (wiegandtype == wiegandtype_26) {
							current_user.cardtype = le? cardtype_wg26le: cardtype_wg26be;
						} else {
							// think wiegandtype_34
							current_user.cardtype = le? cardtype_wg34le: cardtype_wg34be;
						}

						preferences::set_inputcardtype(current_user.inputcardtype);
						preferences::set_gauthscene(current_user.gauthscene);
						preferences::set_cardstrategy(current_user.cardstrategy);

						preferences::set_cardtype(current_user.cardtype);
						// preferences::set_virtualcard(current_user.virtualcard);
						preferences::set_liveness(current_user.liveness);
						// preferences::set_openmethod(current_user.openmethod);
						preferences::set_opentime(0, current_user.opentime);
						preferences::set_opentime(1, current_user.opentime2);
						preferences::set_comparethreshold(current_user.comparethreshold);
						preferences::set_areathreshold(current_user.areathreshold);
						// preferences::set_reboottime(current_user.reboottime);
						// preferences::set_novideorebootthreshold(current_user.novideorebootthreshold);
						// preferences::set_personversion(current_user.personversion);
						// preferences::set_camera(current_user.camera);
						preferences::set_serverversion(current_user.serverversion.str(false));

						if (original_inputcardtype != current_user.inputcardtype) {
							faceprint::recalculate_card_u32();
						}

					} else {
						current_user.keepalive = 15;
					}

				} else {
					utils::string_map symbols;
					symbols["devicecode"] = devicecode;
					err << vgettext2("$devicecode did not find the bound function. Check to see if this device was created in Agbox, or this device must bind at least one function.", symbols);
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool gauth_do_login(gui2::tprogress_& progress, const std::string& devicecode, int timeout_ms, bool quiet)
{
	VALIDATE(game_config::dtype == dtype_gauth, null_str);
	VALIDATE(!game_config::fake_stsdk, null_str);
	tnet_send_wdg_lock wdg_lock;

	std::string invalid_field_msg;
	std::string request_json;
	current_user.clear();

	// http://sgdzpic.3322.org:8088/agbox/device/gauth/ICCard/SGT1TEST0010

	int64_t server_time = 0;
	net::thttp_agent agent(form_url("agbox/device/gauth/ICCard", devicecode), "POST", game_config::cert_file, timeout_ms == nposm? TIMEOUT_10S: timeout_ms);
	agent.did_pre = std::bind(&gauth_did_pre_login, _2, _3, std::ref(request_json));
	agent.did_post = std::bind(&gauth_did_post_login, _2, _3, devicecode, quiet, std::ref(request_json), std::ref(invalid_field_msg), &server_time);

	bool ret = net::handle_http_request(agent);
	if (!ret) {
		return false;
	}

	game_config::no_face_types.clear();

	utils::string_map symbols;
	game_config::runtime_errs[rterr_duplicatefunction].clear();
	game_config::runtime_errs[rterr_leader].clear();
	game_config::runtime_errs[rterr_conflictcheckface].clear();
	game_config::runtime_errs[rterr_group].clear();
	game_config::runtime_errs[rterr_nooptionperson].clear();
	std::set<int> function_ints;
	std::set<int> only_card_functions;
	std::set<int> error_leader_functions;
	std::set<int> error_group_functions;
	std::set<int> error_nooptionperson_functions;
	for (std::vector<tgauth_function>::const_iterator it = current_user.functions.begin(); it != current_user.functions.end(); ++ it) {
		const tgauth_function& function = *it;
		if (function.function > max_operatable_function) {
			// don't check non-operatable function
			continue;
		}
		if (!function_ints.count(function.function)) {
			function_ints.insert(function.function);
		} else {
			symbols.clear();
			symbols["function"] = str_cast(function.function);
			game_config::runtime_errs[rterr_duplicatefunction] = vgettext2("[getGroupFuction] duplicate function: $function", symbols);
		}
		if (!function.leader_check_face) {
			only_card_functions.insert(function.function);
			game_config::no_face_types.insert(function.leader);
		}
		if (function.leader < 0) {
			error_leader_functions.insert(function.function);
		}

		for (std::vector<int>::const_iterator it = function.group.begin(); it != function.group.end(); ++ it) {
			if (*it == GAUTH_NO_OPTION_PERSON) {
				error_nooptionperson_functions.insert(function.function);
				break;
			}
		}
	}
	if (current_user.cardstrategy == cardstrategy_nocard && !only_card_functions.empty()) {
		symbols.clear();
		symbols["functions"] = utils::join(only_card_functions);
		symbols["no_card"] = game_config::cardstrategies.find(cardstrategy_nocard)->second;
		game_config::runtime_errs[rterr_conflictcheckface] = vgettext2("[getGroupFuction] $no_card, but function($functions) request leader swipe card only", symbols);
	}
	if (!error_leader_functions.empty()) {
		symbols.clear();
		symbols["functions"] = utils::join(error_leader_functions);
		game_config::runtime_errs[rterr_leader] = vgettext2("[getGroupFuction] leader in function($functions) < 0", symbols);
	}
	if (!error_group_functions.empty()) {
		symbols.clear();
		symbols["functions"] = utils::join(error_group_functions);
		game_config::runtime_errs[rterr_group] = vgettext2("[getGroupFuction] groupEnabled function($functions) is false, but group isn' empty", symbols);
	}
	if (!error_nooptionperson_functions.empty()) {
		symbols.clear();
		symbols["functions"] = utils::join(error_nooptionperson_functions);
		symbols["digit"] = str_cast(GAUTH_NO_OPTION_PERSON);
		game_config::runtime_errs[rterr_nooptionperson] = vgettext2("[getGroupFuction] function($functions), $digit is up to one and must be placed at end of 'group'", symbols);
	}

	// to gauth device, only login, keepalive and getpersonlist are success, think login success.
	const int64_t start_time = time(nullptr);
	const uint32_t start_ticks = SDL_GetTicks();
	tkeepalive_result keepalive_result;
	ret = gauth_do_keepalive(progress, keepalive_result, false, quiet);
	if (!ret) {
		current_user.clear();
		return false;
	}
	server_time = keepalive_result.servertime;
	const uint32_t stop_ticks = SDL_GetTicks();
	if (stop_ticks - start_ticks < 2000 && server_time >= 1532800000) {
		int64_t abs_diff = posix_abs(server_time - start_time);
		const int time_sync_threshold = 4; // 4 second
		if (abs_diff > time_sync_threshold) {
			int64_t desire = server_time - (stop_ticks - start_ticks) / 1000;
			SDL_SetTime(desire);
		}
	}

	if (faceprint::facestore_initialized) {
		if (game_config::bbs_server.generate_furl() != preferences::bbs_server_furl() || devicecode != group.leader().name()) {
			faceprint::clear_list_feature();
			faceprint::facestore_initialized = false;
		}
	}

	// since login success, save url. avoid below exception quit, require input url again.
	preferences::set_bbs_server_furl(game_config::bbs_server.generate_furl());

	if (!faceprint::facestore_initialized || !faceprint::gauth_facestore_init_called) {
		// Since want to create a new facestore, person in current_user.undownload_persons is out, so clear it.
		current_user.undownload_persons.clear();
		gauth_do_facestoreinit(progress);
	}
	return true;
}

bool gauth_do_logout()
{
	VALIDATE(game_config::dtype == dtype_gauth, null_str);
	VALIDATE(current_user.valid(), null_str);

	return true;
}

bool gauth_did_pre_keepalive(net::HttpRequestHeaders& headers, std::string& body)
{
	Json::Value json_root;
	json_root["jsonrpc"] = "2.0";
	json_root["method"] = "heart";
	json_root["id"] = "63196C79A4FB453E9BF173D30E497A51";

	Json::Value json_params;
	json_params["status"] = 0;
	json_params["ver"] = game_config::version.str(true);
	json_root["params"] = json_params;


	std::map<std::string, std::string> data;
	data.insert(std::make_pair("key", "f2c8f44c-a5d4-42f5-9784-360b08723dbb"));

	Json::FastWriter writer;
	data.insert(std::make_pair("json", writer.write(json_root)));

	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;
	headers.SetHeader(net::HttpRequestHeaders::kContentType, contenttype_value);

	return true;
}


bool gauth_did_post_keepalive(const net::RoseDelegate& delegate, int status, bool quiet, int64_t* last_feature_ts, tkeepalive_result& keepalive_result)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, std::string("[heart] ") + err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("[heart] Invalid response");
		} else {
			Json::Value& error_object = json_object["error"];
			Json::Value& result = json_object["result"];
			err << gauth_handle_response("agbox/device/gauth/ICCard", "heart", error_object, result);

			if (err.str().empty()) {
				const std::string datetime = result["datetime"].asString();
				keepalive_result.servertime = datetime_str_2_ts(datetime);
				*last_feature_ts = datetime_str_2_ts(result["personUpdateTime"].asString()) * 1000; // ms
				int reloadType = result["reloadType"].asInt();
				if (reloadType == rt_relogin) {
					keepalive_result.relogin = true;
				} else if (reloadType == rt_reboot) {
					keepalive_result.reboot = true;
				}

				// Json::Value& update = result["appUpload"];
				Json::Value& update = result["appUpdate"];
				if (update.isObject()) {
					keepalive_result.update_ver = update["ver"].asString();
					keepalive_result.update_url = update["url"].asString();
				}
				if (game_config::os == os_windows) {
					keepalive_result.update_ver = "1.1.8";
					keepalive_result.update_url = "upload/idPic/ID_1234568790876.jpg";
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool gauth_do_keepalive(gui2::tprogress_& progress, tkeepalive_result& result, bool touch_facestore, bool quiet)
{
	result.clear();

	VALIDATE(current_user.valid(), null_str);
	int64_t last_feature_ts = 0;

	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(form_url("agbox/device/gauth/ICCard", current_user.devicecode), "POST", game_config::cert_file, TIMEOUT_10S);
	agent.did_pre = std::bind(&gauth_did_pre_keepalive, _2, _3);
	agent.did_post = std::bind(&gauth_did_post_keepalive, _2, _3, quiet, &last_feature_ts, std::ref(result));

	bool ret = net::handle_http_request(agent);
	if (!ret) {
		return false;
	}

	if (touch_facestore) {
		if (!faceprint::facestore_initialized || !faceprint::gauth_facestore_init_called) {
			// do_facestoreinit in do_login maybe fail.
			net::gauth_do_facestoreinit(progress);

		} else if (last_feature_ts > faceprint::facestore_ts) {
			net::gauth_do_facestoresync(progress);
			// rorce to use keepalive's ts
			faceprint::facestore_ts = last_feature_ts;
		}
	}
	return true;
}

bool gauth_did_pre_getpersonlist(net::HttpRequestHeaders& headers, std::string& body, std::string& request_json)
{
	Json::Value json_root;
	json_root["jsonrpc"] = "2.0";
	json_root["method"] = "getPersonList";
	json_root["id"] = "63196C79A4FB453E9BF173D30E497A51";

	// faceprint::facestore_ts is the person's update time that last updated.
	// Set startTime to faceprint::facestore_ts, and hopefully Agbox returns the person > this time, 
	// but Agbox returns the person who is >= this time.
	// so set startTime <== facestore_ts + 1000. 1000 is 1 second.
	int64_t facestore_ts = 0;
	if (faceprint::gauth_facestore_init_called && faceprint::facestore_ts != 0) {
		// faceprint::facestore_ts's unit is ms.
		facestore_ts = faceprint::facestore_ts + 1000;
	}

	Json::Value json_params;
	json_params["startTime"] = format_time_ymdhms3(facestore_ts / 1000);
	if (facestore_ts == 0) {
		// if enabled = true, only return person that is valid(it's enabled field is true).
		// only during init, enabled set true.
		json_params["enabled"] = true;
	}
	json_root["params"] = json_params;

	std::map<std::string, std::string> data;
	data.insert(std::make_pair("key", "f2c8f44c-a5d4-42f5-9784-360b08723dbb"));

	Json::FastWriter writer;
	request_json = writer.write(json_root);
	data.insert(std::make_pair("json", request_json));

	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;
	headers.SetHeader(net::HttpRequestHeaders::kContentType, contenttype_value);

	return true;
}

bool gauth_did_post_getpersonlist(const net::RoseDelegate& delegate, int status, bool quiet, std::set<std::string>& signouted_ids, int64_t* max_ts_ptr, const std::string& request_json)
{
	VALIDATE(signouted_ids.empty() && *max_ts_ptr == 0, null_str);

	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& error_object = json_object["error"];
			Json::Value& result = json_object["result"];
			err << gauth_handle_response("agbox/device/gauth/ICCard", "getPersonList", error_object, result);

			if (err.str().empty()) {
				if (game_config::os == os_windows) {
					std::stringstream file_ss;
					tfile file(game_config::preferences_dir + "/2.dat", GENERIC_WRITE, CREATE_ALWAYS);
					VALIDATE(file.valid(), null_str);
					file_ss << "---request json---\n" << request_json << "\n\n" << "---response json---\n";
					posix_fwrite(file.fp, file_ss.str().c_str(), file_ss.str().size());

					posix_fwrite(file.fp, data_received.c_str(), data_received.size());
				}

				Json::Value& person_list = result["persons"];
				if (person_list.isArray()) {
					int64_t max_ts = 0;
					for (int at = 0; at < (int)person_list.size(); at ++) {
						const Json::Value& item = person_list[at];
						int idtype = item["credentialType"].asInt();
						const std::string id = item["IdentifierCard"].asString();
						if (id.empty()) {
							// should not occur.
							continue;
						}
						if (id == "null") {
							// should not occur.
							continue;
						}
						bool enabled = item["enabled"].asBool();
						if (!faceprint::gauth_facestore_init_called || faceprint::facestore_ts == 0) {
							VALIDATE(enabled, null_str);
						}
						int64_t ts = datetime_str_2_ts(item["updateTime"].asString()) * 1000; // ms
						if (ts > max_ts) {
							max_ts = ts;
						}

						if (current_user.undownload_persons.count(id) != 0) {
							// Policy: Always use the newest values. why?
							// 1)if call by gauth_do_facestoreinit, undownload_persons is empty, so no effect.
							// 2)if call by gauth_do_facestoresync, old value maybe is error, for example err url.
							//   and newest value is corrected url. of couse new url should replace old value.
							std::map<std::string, tgauth_person>::iterator find_it = current_user.undownload_persons.find(id);
							current_user.undownload_persons.erase(find_it);
						}
						if (enabled) {
							current_user.undownload_persons.insert(std::make_pair(id, tgauth_person(idtype, id, item["name"].asString(), item["function"].asInt(), 
								item["id"].asString(), item["enabled"].asBool(), ts, 
								date_str_2_ts(item["expiryDate"].asString()), item["url"].asString())));
						} else {
							signouted_ids.insert(id);
						}
					}
					*max_ts_ptr = max_ts;
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

int is_valid_image_url(const std::string& _url)
{
	if (_url.empty()) {
		return nposm;
	}
	const GURL url(_url);
	const std::string scheme = url.scheme();
	if (scheme != "http" && scheme != "https") {
		return nposm;
	}
	const std::string file = url.ExtractFileName();
	std::string extname = utils::lowercase(file_ext_name(file));
	std::map<std::string, int>::const_iterator find_it = game_config::image_white_extnames.find(extname);
	if (find_it != game_config::image_white_extnames.end()) {
		return find_it->second;
	}
	return nposm;
}

bool gauth_do_getpersonlist(gui2::tprogress_& progress, std::set<std::string>& signouted_ids, int64_t* max_ts_ptr)
{
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(max_ts_ptr != nullptr, null_str);
	signouted_ids.clear();
	*max_ts_ptr = 0;

	bool quiet = true;
	std::string request_json;

	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(form_url("agbox/device/gauth/ICCard", current_user.devicecode), "POST", game_config::cert_file, TIMEOUT_45S);
	agent.did_pre = std::bind(&gauth_did_pre_getpersonlist, _2, _3, std::ref(request_json));
	agent.did_post = std::bind(&gauth_did_post_getpersonlist, _2, _3, quiet, std::ref(signouted_ids), max_ts_ptr, std::ref(request_json));

	return net::handle_http_request(agent);
}

bool gauth_do_facestoreinit(gui2::tprogress_& progress)
{
	VALIDATE(!faceprint::gauth_facestore_init_called || !faceprint::facestore_initialized, null_str);
	if (!faceprint::facestore_initialized) {
		VALIDATE(faceprint::facestore_ts == 0, null_str);
	}
	VALIDATE(current_user.undownload_persons.empty(), null_str);

	std::set<std::string> signouted_ids;
	int64_t max_ts = 0;
	bool ret = gauth_do_getpersonlist(progress, signouted_ids, &max_ts);
	if (!ret) {
		return false;
	}

	if (faceprint::facestore_initialized && !faceprint::gauth_facestore_init_called) {
		// get rid of some item. 1)delete sync. 2), 3),
		faceprint::gauth_green_local_facestore();
	}
	faceprint::gauth_facestore_init_called = true;

	if (!current_user.undownload_persons.empty()) {
		gauth_undownload_person_2_feature_task(progress);
	}
	return true;
}

bool gauth_do_facestoresync(gui2::tprogress_& progress)
{
	VALIDATE(faceprint::gauth_facestore_init_called && faceprint::facestore_initialized, null_str);

	std::set<std::string> signouted_ids;
	int64_t max_ts = 0;
	bool ret = gauth_do_getpersonlist(progress, signouted_ids, &max_ts);
	if (!ret) {
		return false;
	}

	if (!current_user.undownload_persons.empty() || !signouted_ids.empty()) {
		// has insert, update or erase.
		faceprint::gauth_sync_facestore_erase(signouted_ids, max_ts);
		if (!current_user.undownload_persons.empty()) {
			gauth_undownload_person_2_feature_task(progress);
		}
	}
	return true;
}

bool groupauth_did_pre_event(net::HttpRequestHeaders& headers, std::string& body, int id)
{
	Json::Value json_root;
	json_root["jsonrpc"] = "2.0";
	json_root["method"] = "event";
	json_root["id"] = "63196C79A4FB453E9BF173D30E497A51";

	Json::Value json_params;
	json_params["id"] = id;
	json_params["note"] = id == gauth_event_alarm? preferences::alarm_note(): game_config::gauth_events.find(id)->second;
	json_root["params"] = json_params;

	std::map<std::string, std::string> data;
	data.insert(std::make_pair("key", "f2c8f44c-a5d4-42f5-9784-360b08723dbb"));

	Json::FastWriter writer;
	data.insert(std::make_pair("json", writer.write(json_root)));

	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;
	headers.SetHeader(net::HttpRequestHeaders::kContentType, contenttype_value);

	return true;
}

bool groupauth_did_post_event(const net::RoseDelegate& delegate, int status, bool quiet)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& error_object = json_object["error"];
			std::string result = json_object["result"].asString();
			err << gauth_handle_response2(error_object, result);
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool gauth_do_event(int id)
{
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(game_config::gauth_events.count(id) > 0, null_str);
	bool quiet = false;

	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(form_url("agbox/device/gauth/ICCard", current_user.devicecode), "POST", game_config::cert_file, TIMEOUT_45S);
	agent.did_pre = std::bind(&groupauth_did_pre_event, _2, _3, id);
	agent.did_post = std::bind(&groupauth_did_post_event, _2, _3, quiet);

	return net::handle_http_request(agent);
}

bool agbox_did_pre_get_plate(net::HttpRequestHeaders& headers, std::string& body, int idtype, const std::string& idnumber)
{
	const int max_page_size = 500;

	Json::Value json_root;
	json_root["jsonrpc"] = "2.0";
	json_root["method"] = "getPlate";
	json_root["id"] = "63196C79A4FB453E9BF173D30E497A51";

	Json::Value json_params;
	json_params["pageSize"] = str_cast(max_page_size);
	json_params["page"] = 1;
	json_params["enabled"] = true;
	if (idtype != nposm) {
		VALIDATE(!idnumber.empty(), null_str);
		json_params["credentialType"] = idtype;
		json_params["credentialNo"] = idnumber;
	} else {
		VALIDATE(idnumber.empty(), null_str);
	}

	json_root["params"] = json_params;

	std::map<std::string, std::string> data;
	data.insert(std::make_pair("key", "f2c8f44c-a5d4-42f5-9784-360b08723dbb"));

	Json::FastWriter writer;
	data.insert(std::make_pair("json", writer.write(json_root)));

	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;
	headers.SetHeader(net::HttpRequestHeaders::kContentType, contenttype_value);
	return true;
}

bool agbox_did_post_get_plate(const net::RoseDelegate& delegate, int status, bool quiet, std::vector<tcs_car>& cars)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& error_object = json_object["error"];
			Json::Value& result = json_object["result"];
			err << gauth_handle_response("agbox/share/car", "getPlate", error_object, result);

			if (err.str().empty()) {
				Json::Value& plate_list = result["plateList"];
				if (plate_list.isArray()) {
					for (int at = 0; at < (int)plate_list.size(); at ++) {
						const Json::Value& item = plate_list[at];
						const int platetype = item["plateTypeCode"].asInt();
						std::string no = item["plateNo"].asString();
						const int cartype = item["carTypeCode"].asInt();
						bool enabled = item["enabled"].asBool();
						if (enabled) {
							tcs_car car(platetype, no, cartype);
							if (car.valid()) {
								cars.push_back(car); 
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool agbox_get_plate(int idtype, const std::string& idnumber, std::vector<tcs_car>& cars)
{
	VALIDATE(current_user.valid(), null_str);
	bool quiet = true;

	cars.clear();

	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(form_url("agbox/share", "car"), "POST", game_config::cert_file, TIMEOUT_6S);
	agent.did_pre = std::bind(&agbox_did_pre_get_plate, _2, _3, idtype, idnumber);
	agent.did_post = std::bind(&agbox_did_post_get_plate, _2, _3, quiet, std::ref(cars));

	return net::handle_http_request(agent);
}

bool agbox_did_pre_get_parking_events(net::HttpRequestHeaders& headers, std::string& body, int cartype, std::string& request_json)
{
	const int max_page_size = 500;

	Json::Value json_root;
	json_root["jsonrpc"] = "2.0";
	json_root["method"] = "getEventList";
	json_root["id"] = "63196C79A4FB453E9BF173D30E497A51";

	Json::Value json_params;
	json_params["carType"] = cartype;
	json_params["offset"] = 0;
	json_params["limit"] = MAX_SHAPSHOT_CAMERAS * 2;

	json_root["params"] = json_params;

	std::map<std::string, std::string> data;
	data.insert(std::make_pair("key", "f2c8f44c-a5d4-42f5-9784-360b08723dbb"));

	Json::FastWriter writer;
	request_json = writer.write(json_root);
	data.insert(std::make_pair("json", request_json));

	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;
	headers.SetHeader(net::HttpRequestHeaders::kContentType, contenttype_value);
	return true;
}

bool agbox_did_post_get_parking_events(const net::RoseDelegate& delegate, int status, bool quiet, const std::string& request_json, std::vector<tparking_event>& events)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& error_object = json_object["error"];
			Json::Value& result = json_object["result"];
			err << gauth_handle_response("agbox/device/parking", "getEventList", error_object, result);

				if (game_config::os == os_windows) {
					std::stringstream file_ss;
					tfile file(game_config::preferences_dir + "/3.dat", GENERIC_WRITE, CREATE_ALWAYS);
					VALIDATE(file.valid(), null_str);
					file_ss << "---request json---\n" << request_json << "\n\n" << "---response json---\n";
					posix_fwrite(file.fp, file_ss.str().c_str(), file_ss.str().size());

					posix_fwrite(file.fp, data_received.c_str(), data_received.size());
				}

			if (err.str().empty()) {
				Json::Value& event_list = result["eventList"];
				if (event_list.isArray()) {
					for (int at = 0; at < (int)event_list.size(); at ++) {
						const Json::Value& item = event_list[at];
						const int cartype = item["carType"].asInt();
						std::string triggertime = item["triggerTime"].asString();
						std::string no = item["plateNo"].asString();
						std::string deviceid = item["deviceId"].asString();
						std::string carpic = item["carPic"].asString();
						std::string secnepic = item["secnePic"].asString();
						tparking_event e(cartype, datetime_str_2_ts(triggertime), no, deviceid, carpic, secnepic);
						if (e.valid()) {
							events.push_back(e); 
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool agbox_get_parking_events(int cartype, std::vector<tparking_event>& events)
{
	VALIDATE(current_user.valid(), null_str);
	VALIDATE(cartype != 0, null_str);
	bool quiet = true;
	std::string request_json;

	events.clear();

	tnet_send_wdg_lock wdg_lock;
	net::thttp_agent agent(form_url("agbox/device", "parking"), "POST", game_config::cert_file, TIMEOUT_6S);
	agent.did_pre = std::bind(&agbox_did_pre_get_parking_events, _2, _3, cartype, std::ref(request_json));
	agent.did_post = std::bind(&agbox_did_post_get_parking_events, _2, _3, quiet, std::ref(request_json), std::ref(events));

	return net::handle_http_request(agent);
}
*/
//
// cswamp
//
class ihttp_cswamp
{
public:
	explicit ihttp_cswamp(gui2::tprogress_& _progress, const std::string& _url, bool _quiet)
		: progress(_progress)
		, url(_url)
		, quiet(_quiet)
	{}

	virtual void verify_param() {}
	virtual int timeout_ms() const { return nposm; }
	virtual void pre(Json::Value& json_params) const {}
	virtual void pre_binary(std::map<std::string, std::string>& data) const {}
	// if has error, return value is error-string, else return empty.
	virtual std::string post(Json::Value& results) { return null_str; }
	virtual std::string post_binary(const uint8_t* data, int size) { return null_str; }

public:
	gui2::tprogress_& progress;
	const std::string url;
	bool quiet;
};

bool cswamp_did_pre_agbox(net::HttpRequestHeaders& headers, std::string& body, const ihttp_cswamp& entity)
{
	Json::Value json_root;
	json_root["version"] = game_config::rose_version.str(true);
	json_root["nonce"] = rand();

	Json::Value json_params;
	entity.pre(json_root);

	// entity.pre_binary(data);

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool cswamp_did_post_agbox(const net::RoseDelegate& delegate, int status, bool quiet, ihttp_cswamp& entity)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			utils::string_map symbols;
			symbols["url"] = entity.url;
			err << vgettext2("$url   Response is invalid json foramt", symbols);
		} else {
			if (game_config::os == os_windows) {
				tfile file(game_config::preferences_dir + "/4.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
			Json::Value& code_json = json_object["code"];
			int code = code_json.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "login, Unknown error. code: " << code;
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						err << entity.post(results);
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool cswamp_did_post_with_binary(const net::RoseDelegate& delegate, int status, ihttp_cswamp& entity)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!entity.quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			utils::string_map symbols;
			symbols["url"] = entity.url;
			err << vgettext2("$url   Response is invalid json foramt", symbols);
		} else {
			if (game_config::os == os_windows) {
				tfile file(game_config::preferences_dir + "/4.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
			Json::Value& code_json = json_object["code"];
			int code = code_json.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "login, Unknown error. code: " << code;
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						err << entity.post(results);

						if (err.str().empty()) {
							const uint8_t* binary = (const uint8_t*)content + prefix_size + json_str.size();
							int binary_size = content_size - prefix_size - json_size;
							if (binary_size > binary_size) {
								err << _("SHA1 fail. not enoth bytes for SHA1 verify");
							} else {
								binary_size -= SHA_DIGEST_LENGTH;
							}
							if (err.str().empty()) {
								std::unique_ptr<uint8_t[]> md = utils::sha1(binary, binary_size);
								if (memcmp(binary + binary_size, md.get(), SHA_DIGEST_LENGTH) != 0) {
									err << _("SHA1 fail. verify data fail");
								}
							}
							if (err.str().empty()) {
								err << entity.post_binary(binary, binary_size);
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!entity.quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool cswamp_do_agbox(ihttp_cswamp& entity, bool req_with_binary, bool resp_with_binary)
{
	entity.verify_param();
	std::string request_json;

	int timeout = entity.timeout_ms();
	net::thttp_agent agent(entity.url, "POST", null_str, timeout == nposm? TIMEOUT_6S: timeout);
	if (!req_with_binary) {
		agent.did_pre = std::bind(&cswamp_did_pre_agbox, _2, _3, std::ref(entity));
	} else {
		VALIDATE(false, null_str);
	}
	if (!resp_with_binary) {
		agent.did_post = std::bind(&cswamp_did_post_agbox, _2, _3, entity.quiet, std::ref(entity));
	} else {
		agent.did_post = std::bind(&cswamp_did_post_with_binary, _2, _3, std::ref(entity));
	}

	return net::handle_http_request(agent);
}

// /agbox/device/personreg --> updateHeart
class thttp_cswamp_device_getapplet: public ihttp_cswamp
{
public:
	thttp_cswamp_device_getapplet(gui2::tprogress_& progress, int type, const std::string& bundleid, ::applet::tapplet& applet, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/getapplet"), quiet)
		, type_(type)
		, bundleid_(bundleid)
		, applet_(applet)
		, timeout_(timeout)
		, quiet_(false)
		, iconfsize_(nposm)
		, rspfsize_(nposm)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;
	std::string post_binary(const uint8_t* data, int size) override;

private:
	int type_;
	const std::string bundleid_;
	::applet::tapplet& applet_;
	const int timeout_;
	const bool quiet_;
	int iconfsize_;
	int rspfsize_;
};

void thttp_cswamp_device_getapplet::verify_param()
{
	VALIDATE(type_ == applet::type_distribution || type_ == applet::type_development, null_str);
	VALIDATE(!bundleid_.empty(), null_str);
}

void thttp_cswamp_device_getapplet::pre(Json::Value& json_params) const
{
	json_params["type"] = applet::srctypes.find(type_)->second;
	json_params["bundleid"] = bundleid_;
}

std::string thttp_cswamp_device_getapplet::post(Json::Value& results)
{
	applet_.title = results["title"].asString();
	applet_.subtitle = results["subtitle"].asString();
	applet_.ts = results["time"].asInt64();
	applet_.username = results["username"].asString();

	iconfsize_ = results["iconfsize"].asInt();
	rspfsize_ = results["rspfsize"].asInt();
	return null_str;
}

std::string thttp_cswamp_device_getapplet::post_binary(const uint8_t* data, int size)
{
	if (size != iconfsize_ + rspfsize_) {
		return _("binary data length error");
	}
	if (rspfsize_ <= RSP_HEADER_BYTES + SHA_DIGEST_LENGTH) {
		return _("rsp data length is too short");
	}
/*
	{
		SDL_RWops* src = SDL_RWFromMem((void*)(data), iconfsize_);
		applet_.icon = IMG_Load_RW(src, 0);
		SDL_RWclose(src);
		applet_.icon = makesure_neutral_surface(applet_.icon);
	}
*/
	const uint8_t* rsp_data = data + iconfsize_;
	if (!utils::verity_sha1(rsp_data, rspfsize_ - SHA_DIGEST_LENGTH, rsp_data + rspfsize_ - SHA_DIGEST_LENGTH)) {
		return _("Verify rsp data error");
	}
	memcpy(&applet_.rspheader, rsp_data, RSP_HEADER_BYTES);

	const std::string temp_aplt_dl_zip = "__tmp_aplt.zip";
	const std::string aplt_zip = game_config::preferences_dir + "/" + temp_aplt_dl_zip;
	{
		tfile file(aplt_zip, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, rsp_data + RSP_HEADER_BYTES, rspfsize_ - RSP_HEADER_BYTES - SHA_DIGEST_LENGTH);
	}

	const std::string temp_aplt_dl_dir = "__tmp_aplt_dl";
	const std::string dst_path = game_config::preferences_dir + "/" + temp_aplt_dl_dir;
	SDL_DeleteFiles(dst_path.c_str());
	// gui2::delete_file(progress, dst_path); 
	minizip::unzip_file(aplt_zip, dst_path.c_str(), null_str, null_str);

	applet_.bundleid = applet::get_settings_cfg(dst_path, false, &applet_.version);
	if (applet_.bundleid.empty()) {
		return _("Applet's settings.cfg is invalid");
	}

	const std::string lua_bundleid = utils::replace_all(applet_.bundleid, ".", "_");
	const std::string preferences_aplt_path = game_config::preferences_dir + "/" + lua_bundleid;

	SDL_DeleteFiles(preferences_aplt_path.c_str());
	// gui2::delete_file(progress, preferences_aplt_path);

	SDL_RenameFile(dst_path.c_str(), lua_bundleid.c_str());

	applet_.set(type_, applet_.title, applet_.subtitle, applet_.username, applet_.ts, 
		preferences_aplt_path, preferences_aplt_path + "/" + APPLET_ICON);

	applet_.write_distribution_cfg();

	return null_str;
}

bool cswamp_getapplet(gui2::tprogress_& progress, int type, const std::string& bundleid, ::applet::tapplet& applet, bool quiet)
{
	thttp_cswamp_device_getapplet entity(progress, type, bundleid, applet, TIMEOUT_10S, quiet);
	return cswamp_do_agbox(entity, false, true);
}


// agbox/village --> getInfo
class thttp_cswamp_device_login: public ihttp_cswamp
{
public:
	thttp_cswamp_device_login(gui2::tprogress_& progress, int type, const std::string& username, const std::string& password, const std::string& deviceid, const std::string& devicename, int timeout, bool quiet, tcswamp_login_result& result)
		: ihttp_cswamp(progress, form_url2("cswamp/device/login"), quiet)
		, type_(type)
		, username_(username)
		, password_(password)
		, deviceid_(deviceid)
		, devicename_(devicename)
		, timeout_(timeout)
		, quiet_(false)
		, result_(result)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	int type_;
	const std::string username_;
	const std::string password_;
	const std::string deviceid_;
	const std::string devicename_;
	const int timeout_;
	const bool quiet_;

	tcswamp_login_result& result_;
};

void thttp_cswamp_device_login::verify_param()
{
	VALIDATE(type_ >= 0 && type_ < cswamp_login_type_count, null_str);
	VALIDATE(!username_.empty(), null_str);
	VALIDATE(!password_.empty(), null_str);
	VALIDATE(!deviceid_.empty(), null_str);
	VALIDATE(!devicename_.empty(), null_str);
}

static std::map<int, std::string> cswamp_login_types;
void thttp_cswamp_device_login::pre(Json::Value& json_params) const
{
	if (cswamp_login_types.empty()) {
		cswamp_login_types.insert(std::make_pair(cswamp_login_type_password, "password"));
		cswamp_login_types.insert(std::make_pair(cswamp_login_type_cookie, "cookie"));
	}
	json_params["type"] = cswamp_login_types.find(type_)->second;
	json_params["username"] = username_;
	json_params["password"] = password_;
	json_params["deviceid"] = deviceid_;
	json_params["devicename"] = devicename_;
}

std::string thttp_cswamp_device_login::post(Json::Value& results)
{
	result_.sessionid = results["sessionid"].asString();
	result_.pwcookie = results["pwcookie"].asString();
	return null_str;
}

bool cswamp_login(gui2::tprogress_& progress, int type, const std::string& username, const std::string& password, const std::string& deviceid, const std::string& devicename, bool quiet, tcswamp_login_result& result)
{
	thttp_cswamp_device_login entity(progress, type, username, password, deviceid, devicename, TIMEOUT_10S, quiet, result);
	return cswamp_do_agbox(entity, false, false);
}


// agbox/village --> getBuilding
class thttp_cswamp_device_logout: public ihttp_cswamp
{
public:
	thttp_cswamp_device_logout(gui2::tprogress_& progress, const std::string& sessionid, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/logout"), quiet)
		, sessionid_(sessionid)
		, timeout_(timeout)
		, quiet_(quiet)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	const int timeout_;
	const bool quiet_;
};

void thttp_cswamp_device_logout::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
}

void thttp_cswamp_device_logout::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;
}

std::string thttp_cswamp_device_logout::post(Json::Value& results)
{
	return null_str;
}

bool cswamp_logout(gui2::tprogress_& progress, const std::string& sessionid, bool quiet)
{
	thttp_cswamp_device_logout entity(progress, sessionid, TIMEOUT_10S, quiet);
	return cswamp_do_agbox(entity, false, false);
}


// agbox/village --> getHouse
class thttp_cswamp_device_syncappletlist: public ihttp_cswamp
{
public:
	thttp_cswamp_device_syncappletlist(gui2::tprogress_& progress, const std::string& sessionid, int app, const std::vector<std::string>& adds, const std::vector<std::string>& removes, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/syncappletlist"), quiet)
		, sessionid_(sessionid)
		, app_(app)
		, adds_(adds)
		, removes_(removes)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	const int app_;
	const std::vector<std::string> adds_;
	const std::vector<std::string> removes_;
	const int timeout_;
	const bool quiet_;
	std::string bundleids_;
};

static std::map<int, std::string> app_types;
void thttp_cswamp_device_syncappletlist::verify_param()
{
	if (app_types.empty()) {
		app_types.insert(std::make_pair(app_kdesktop, "kdesktop"));
		app_types.insert(std::make_pair(app_launcher, "launcher"));
	}
	VALIDATE(!sessionid_.empty(), null_str);
	VALIDATE(app_types.count(app_) != 0, null_str);
}

void thttp_cswamp_device_syncappletlist::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;
	json_params["app"] = app_types.find(app_)->second;
	json_params["add"] = utils::join(adds_);
	json_params["remove"] = utils::join(removes_);
}

std::string thttp_cswamp_device_syncappletlist::post(Json::Value& results)
{
	bundleids_ = results["applet"].asString();
	return null_str;
}

bool cswamp_syncappletlist(gui2::tprogress_& progress, const std::string& sessionid, int app, const std::vector<std::string>& adds, const std::vector<std::string>& removes, bool quiet)
{
	thttp_cswamp_device_syncappletlist entity(progress, sessionid, app, adds, removes, quiet);
	return cswamp_do_agbox(entity, false, false);
}


/*
// agbox/village --> getEntrance
class thttp_agbox_village_getEntrance: public ihttp_agbox
{
public:
	thttp_agbox_village_getEntrance(const std::string& villagecode, std::vector<tentrance>& entrances, bool quiet)
		: ihttp_agbox(form_url2("agbox/village"), "getEntrance", quiet)
		, villagecode(villagecode)
		, entrances(entrances)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const std::string villagecode;
	std::vector<tentrance>& entrances;
};

void thttp_agbox_village_getEntrance::verify_param()
{
	VALIDATE(!villagecode.empty(), null_str);
}

void thttp_agbox_village_getEntrance::pre(Json::Value& json_params) const
{
	json_params["pageSize"] = 100000;
    json_params["page"] = 1;
	json_params["villageCode"] = villagecode;
}

std::string thttp_agbox_village_getEntrance::post(Json::Value& result)
{
	Json::Value& entrance_list = result["entranceList"];
	if (entrance_list.isArray()) {
		for (int at = 0; at < (int)entrance_list.size(); at ++) {
			const Json::Value& item = entrance_list[at];
			const std::string code = item["entranceCode"].asString();
			const std::string name = item["name"].asString();
			entrances.push_back(tentrance(code, name));
		}
	}
	return null_str;
}

bool multivlg_do_getentrance(const std::string& villagecode, std::vector<tentrance>& entrances, bool quiet)
{
	thttp_agbox_village_getEntrance entity(villagecode, entrances, quiet);
	return multivlg_do_agbox(entity);
}

// agbox/basic --> getLoginUser
class thttp_agbox_basic_getLoginUser: public ihttp_agbox
{
public:
	thttp_agbox_basic_getLoginUser(const std::string& username, const std::string& password, toperator& operator2, bool quiet)
		: ihttp_agbox(form_url2("agbox/basic"), "getLoginUser", quiet)
		, username(username)
		, password(password)
		, operator2(operator2)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const std::string username;
	const std::string password;
	toperator& operator2;
};

void thttp_agbox_basic_getLoginUser::verify_param()
{
	VALIDATE(!username.empty(), null_str);
	VALIDATE(!password.empty(), null_str);
}

void thttp_agbox_basic_getLoginUser::pre(Json::Value& json_params) const
{
	json_params["username"] = username;
	json_params["password"] = password;
}

std::string thttp_agbox_basic_getLoginUser::post(Json::Value& result)
{
	std::string name = result["name"].asString();
	bool is_admin = result["isAdmin"].asBool();
	int group = is_admin? operator_admin: operator_operator;
	operator2 = toperator(username, password, group);

	Json::Value& params_object = result["params"];
	if (params_object.isObject()) {
	}
	return null_str;
}

bool multivlg_do_verifyoperator(const std::string& username, const std::string& password, toperator& operator2)
{
	thttp_agbox_basic_getLoginUser entity(username, password, operator2, false);
	return multivlg_do_agbox(entity);
}

// agbox/device/entrance --> getPersonTypeCode
int get_category_from_persontype(int type)
{
	enum {category_owner = 0x1, category_visitor = 0x2};
	return category_owner | category_visitor;
}

class thttp_agbox_device_entrance_getPersonTypeCode: public ihttp_agbox
{
public:
	thttp_agbox_device_entrance_getPersonTypeCode(std::map<int, tpersontypecode>& persontypes, bool quiet)
		: ihttp_agbox(form_url2("agbox/device/entrance"), "getPersonTypeCode", quiet)
		, persontypes(persontypes)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, tpersontypecode>& persontypes;
};

std::string thttp_agbox_device_entrance_getPersonTypeCode::post(Json::Value& result)
{
	Json::Value& person_list = result["personType"];
	if (person_list.isArray()) {
		for (int at = 0; at < (int)person_list.size(); at ++) {
			const Json::Value& item = person_list[at];
			const int code = item["code"].asInt();
			const std::string name = code != game_config::blacklist_persontype? item["name"].asString(): _("Blacklist");
			persontypes.insert(std::make_pair(code, tpersontypecode(code, name, get_category_from_persontype(item["category"].asInt()))));
		}
	}
	return null_str;
}

// agbox/person --> getPeopleTypeCode
int get_category_from_peopletype(int type)
{
	enum {category_owner = 0x1, category_visitor = 0x2};
	return category_owner | category_visitor;
}

class thttp_agbox_person_getPeopleTypeCode: public ihttp_agbox
{
public:
	thttp_agbox_person_getPeopleTypeCode(std::map<int, tpeopletypecode>& peopletypes, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getPeopleTypeCode", quiet)
		, peopletypes(peopletypes)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, tpeopletypecode>& peopletypes;
};

std::string thttp_agbox_person_getPeopleTypeCode::post(Json::Value& result)
{
	Json::Value& people_list = result["peopleType"];
	if (people_list.isArray()) {
		for (int at = 0; at < (int)people_list.size(); at ++) {
			const Json::Value& item = people_list[at];
			const int code = utils::to_int(item["code"].asString());
			const std::string name = item["name"].asString();
			peopletypes.insert(std::make_pair(code, tpeopletypecode(code, name, get_category_from_peopletype(item["category"].asInt()))));
		}
	}
	return null_str;
}

// agbox/person --> getHouseRelCode
class thttp_agbox_person_getHouseRelCode: public ihttp_agbox
{
public:
	thttp_agbox_person_getHouseRelCode(std::map<int, std::string>& houserels, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getHouseRelCode", quiet)
		, houserels(houserels)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& houserels;
};

std::string thttp_agbox_person_getHouseRelCode::post(Json::Value& result)
{
	Json::Value& houserel_list = result["houseRel"];
	if (houserel_list.isArray()) {
		for (int at = 0; at < (int)houserel_list.size(); at ++) {
			const Json::Value& item = houserel_list[at];
			const int code = utils::to_int(item["code"].asString());
			const std::string name = item["name"].asString();
			houserels.insert(std::make_pair(code, name));
		}
	}
	return null_str;
}

// agbox/share/car --> getPlateTypeCode
class thttp_agbox_share_car_getPlateTypeCode: public ihttp_agbox
{
public:
	thttp_agbox_share_car_getPlateTypeCode(std::map<int, std::string>& platetypes, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/car"), "getPlateTypeCode", quiet)
		, platetypes(platetypes)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& platetypes;
};

std::string thttp_agbox_share_car_getPlateTypeCode::post(Json::Value& result)
{
	Json::Value& platetype_list = result["plateType"];
	if (platetype_list.isArray()) {
		for (int at = 0; at < (int)platetype_list.size(); at ++) {
			const Json::Value& item = platetype_list[at];
			platetypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
		}
	}
	return null_str;
}

// agbox/share/car --> getCarTypeCode
class thttp_agbox_share_car_getCarTypeCode: public ihttp_agbox
{
public:
	thttp_agbox_share_car_getCarTypeCode(std::map<int, std::string>& cartypes, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/car"), "getCarTypeCode", quiet)
		, cartypes(cartypes)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& cartypes;
};

std::string thttp_agbox_share_car_getCarTypeCode::post(Json::Value& result)
{
	Json::Value& cartype_list = result["carType"];
	if (cartype_list.isArray()) {
		for (int at = 0; at < (int)cartype_list.size(); at ++) {
			const Json::Value& item = cartype_list[at];
			cartypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
		}
	}
	return null_str;
}

// agbox/person --> getCredentialTypeCode
class thttp_agbox_person_getCredentialTypeCode: public ihttp_agbox
{
public:
	thttp_agbox_person_getCredentialTypeCode(std::map<int, std::string>& idtypes, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getCredentialTypeCode", quiet)
		, idtypes(idtypes)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& idtypes;
};

std::string thttp_agbox_person_getCredentialTypeCode::post(Json::Value& result)
{
	Json::Value& credential_list = result["credentialType"];
	if (credential_list.isArray()) {
		for (int at = 0; at < (int)credential_list.size(); at ++) {
			const Json::Value& item = credential_list[at];
			idtypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
		}
		if (idtypes.count(gat517_openid) == 0) {
			idtypes.insert(std::make_pair(gat517_openid, _("WeChat Openid")));
		}
	}
	return null_str;
}

// agbox/person --> getNationCode
class thttp_agbox_person_getNationCode: public ihttp_agbox
{
public:
	thttp_agbox_person_getNationCode(std::map<int, std::string>& nations, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getNationCode", quiet)
		, nations(nations)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& nations;
};

std::string thttp_agbox_person_getNationCode::post(Json::Value& result)
{
	Json::Value& nation_list = result["nation"];
	if (nation_list.isArray()) {
		// early villageserver hasn't nation block.
		int size = nation_list.size();
		for (int at = 0; at < (int)nation_list.size(); at ++) {
			const Json::Value& item = nation_list[at];
			int code = utils::to_int(item["code"].asString());
			VALIDATE(code >= 1, "Invalid nation code form agbox");
			nations.insert(std::make_pair(code, item["name"].asString()));
		}
	}
	return null_str;
}

// agbox/person --> getEducationCode
class thttp_agbox_person_getEducationCode: public ihttp_agbox
{
public:
	thttp_agbox_person_getEducationCode(std::map<int, std::string>& educations, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getEducationCode", quiet)
		, educations(educations)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& educations;
};

std::string thttp_agbox_person_getEducationCode::post(Json::Value& result)
{
	Json::Value& education_list = result["education"];
	if (education_list.isArray()) {
		for (int at = 0; at < (int)education_list.size(); at ++) {
			const Json::Value& item = education_list[at];
			educations.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
		}
	}
	return null_str;
}

// agbox/person --> getMaritalStatusCode
class thttp_agbox_person_getMaritalStatusCode: public ihttp_agbox
{
public:
	thttp_agbox_person_getMaritalStatusCode(std::map<int, std::string>& maritals, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getMaritalStatusCode", quiet)
		, maritals(maritals)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& maritals;
};

std::string thttp_agbox_person_getMaritalStatusCode::post(Json::Value& result)
{
	Json::Value& marital_list = result["maritalStatus"];
	if (marital_list.isArray()) {
		for (int at = 0; at < (int)marital_list.size(); at ++) {
			const Json::Value& item = marital_list[at];
			maritals.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
		}
	}
	return null_str;
}

// agbox/share/card --> getCardTypeCode
class thttp_agbox_share_card_getCardTypeCode: public ihttp_agbox
{
public:
	thttp_agbox_share_card_getCardTypeCode(std::map<int, std::string>& cardtypes, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/card"), "getCardTypeCode", quiet)
		, cardtypes(cardtypes)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<int, std::string>& cardtypes;
};

std::string thttp_agbox_share_card_getCardTypeCode::post(Json::Value& result)
{
	Json::Value& cardtype_list = result["cardType"];
	if (cardtype_list.isArray()) {
		for (int at = 0; at < (int)cardtype_list.size(); at ++) {
			const Json::Value& item = cardtype_list[at];
			cardtypes.insert(std::make_pair(item["code"].asInt(), item["name"].asString()));
		}
	}
	return null_str;
}

// agbox/device/parking --> getDeviceList
class thttp_agbox_device_parking_getDeviceList: public ihttp_agbox
{
public:
	thttp_agbox_device_parking_getDeviceList(std::map<std::string, std::string>& parkings, bool quiet)
		: ihttp_agbox(form_url2("agbox/device/parking"), "getDeviceList", quiet)
		, parkings(parkings)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<std::string, std::string>& parkings;
};

std::string thttp_agbox_device_parking_getDeviceList::post(Json::Value& result)
{
	Json::Value& parking_list = result["deviceList"];
	if (parking_list.isArray()) {
		for (int at = 0; at < (int)parking_list.size(); at ++) {
			const Json::Value& item = parking_list[at];
			const std::string code = item["deviceId"].asString();
			if (!code.empty()) {
				parkings.insert(std::make_pair(code, item["name"].asString()));
			}
		}
	}
	return null_str;
}

// agbox/basic --> getNationalityCode
class thttp_agbox_basic_getNationalityCode: public ihttp_agbox
{
public:
	thttp_agbox_basic_getNationalityCode(std::map<std::string, std::string>& nationalities, bool quiet)
		: ihttp_agbox(form_url2("agbox/basic"), "getNationalityCode", quiet)
		, nationalities(nationalities)
	{}

private:
	std::string post(Json::Value& result) override;

public:
	std::map<std::string, std::string>& nationalities;
};

std::string thttp_agbox_basic_getNationalityCode::post(Json::Value& result)
{
	Json::Value& nationality_list = result["nationality"];
	if (nationality_list.isArray()) {
		for (int at = 0; at < (int)nationality_list.size(); at ++) {
			const Json::Value& item = nationality_list[at];
			const std::string code = item["code"].asString();
			if (!code.empty()) {
				nationalities.insert(std::make_pair(code, item["name"].asString()));
			}
		}
	}
	return null_str;
}

bool multivlg_do_getmisccode_internal(tmisccode& misccode, const std::string& category, int64_t& last_datadictionary_ts)
{
	// VALIDATE(current_operator.valid(), null_str);
	VALIDATE(category.empty(), null_str);
	last_datadictionary_ts = 0;

	bool ret = true;
	if (ret) {
		thttp_agbox_device_entrance_getPersonTypeCode entity(misccode.persontypes, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_person_getPeopleTypeCode entity(misccode.peopletypes, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_person_getHouseRelCode entity(misccode.houserels, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_share_car_getPlateTypeCode entity(misccode.platetypes, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_share_car_getCarTypeCode entity(misccode.cartypes, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_person_getCredentialTypeCode entity(misccode.idtypes, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_person_getNationCode entity(misccode.nations, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		// 
		misccode.groups.insert(std::make_pair(0, "WHOLE"));
	}
	if (ret) {
		thttp_agbox_person_getEducationCode entity(misccode.educations, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_person_getMaritalStatusCode entity(misccode.maritals, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_share_card_getCardTypeCode entity(misccode.cardtypes, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_device_parking_getDeviceList entity(misccode.parkings, false);
		ret = multivlg_do_agbox(entity);
	}
	if (ret) {
		thttp_agbox_basic_getNationalityCode entity(misccode.nationalities, false);
		ret = multivlg_do_agbox(entity);
	}
	// misccode.causes
	// misccode.notes
	// misccode.lifts
	return ret;
}

// agbox/person --> getInfo
class thttp_agbox_person_getInfo: public ihttp_agbox
{
public:
	thttp_agbox_person_getInfo(tperson& person, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getInfo", quiet)
		, person(person)
		, valid(false)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	tperson& person;
	bool valid;
	std::string pic_url;
};

void thttp_agbox_person_getInfo::verify_param()
{
	VALIDATE(game_config::idtypes.count(person.idtype) != 0 && !person.idnumber.empty(), null_str);
	VALIDATE(!valid, null_str);
}

void thttp_agbox_person_getInfo::pre(Json::Value& json_params) const
{
	json_params["credentialType"] = person.idtype;
	json_params["credentialNo"] = person.idnumber;
	json_params["range"] = "detailed";
}

std::string agbox_base_person_from_json(const Json::Value& result, tperson& person, bool& valid)
{
	valid = false;

	// except: idtype, idnumber, imageformat
	const Json::Value& personinfo = result["personInfo"];
	if (!personinfo.isObject()) {
		return null_str;
	}

	int idtype = personinfo["credentialType"].asInt();
	const std::string idnumber = personinfo["credentialNo"].asString();
	valid = idtype == person.idtype && idnumber == person.idnumber;

	person.name = personinfo["peopleName"].asString();
	person.type = personinfo["entranceTypeCode"].asInt();
	person.peopletype = personinfo["peopleTypeCode"].asInt();
	person.nationality = personinfo["nationalityCode"].asString();
	person.education = utils::to_int(personinfo["educationCode"].asString());
	person.marital = personinfo["maritalStatusCode"].asInt();
	person.startline = date_str_2_ts(personinfo["entryTime"].asString());
	person.card = tcs_card(personinfo["SecurityCardNo"].asString());

	std::string pic_url;
	int64_t i64code = 0;
	const Json::Value& domicile = personinfo["domicile"];
	if (domicile.isObject()) {
		person.nation = utils::to_int(domicile["nationCode"].asString());
		person.birth = domicile["birthDate"].asString();
		person.gender = make_right_gender(domicile["sexCode"].asInt());
		pic_url = domicile["idCardPicUrl"].asString();

		// code3
		person.code3.province = domicile["provinceCode"].asInt64();
		i64code = domicile["districtCode"].asInt64();
		if (game_config::districts.count(i64code)) {
			person.code3.district = &(game_config::districts.find(i64code)->second);
		}
		i64code = domicile["streetCode"].asInt64();
		if (game_config::streets.count(i64code)) {
			person.code3.street = &(game_config::streets.find(i64code)->second);
		}
		if (!person.code3.valid()) {
			// person.code3.clear();
		}
		person.code3.address = domicile["address"].asString();
	}

	const Json::Value& residence = personinfo["residence"];
	if (residence.isObject()) {
		// residence code3
		person.residence_code3.province = residence["provinceCode"].asInt64();
		i64code = residence["districtCode"].asInt64();
		if (game_config::districts.count(i64code)) {
			person.residence_code3.district = &(game_config::districts.find(i64code)->second);
		}
		i64code = residence["streetCode"].asInt64();
		if (game_config::streets.count(i64code)) {
			person.residence_code3.street = &(game_config::streets.find(i64code)->second);
		}
		if (!person.residence_code3.valid()) {
			// person.residence_code3.clear();
		}
		person.residence_code3.address = residence["address"].asString();
	}

	std::vector<std::string> phone_keys;
	std::vector<std::string> phones;
	phone_keys.push_back("phone1");
	phone_keys.push_back("phone2");
	phone_keys.push_back("phone3");
	for (std::vector<std::string>::const_iterator it = phone_keys.begin(); it != phone_keys.end(); ++ it) {
		const std::string& key = *it;
		const Json::Value& phone_object = personinfo[key];
		if (phone_object.isObject()) {
			const std::string phone = phone_object["no"].asString();
			if (!phone.empty()) {
				phones.push_back(phone);
			}
		}
	}
	person.phones = utils::join(phones);

	return pic_url;
}

void agbox_person_image_from_idCardPicUrl(const std::string& pic_url, tperson& person)
{
	VALIDATE(!pic_url.empty(), null_str);
	std::stringstream err;
	std::stringstream url;
	url << "http://" << game_config::bbs_server.host;
	url << ":" << game_config::bbs_server.port;
	url << "/" << pic_url;

	const int timeout = 10000; // 10 second
	std::string image_data;
	bool ret = net::fetch_url_2_mem(url.str(), image_data, timeout);

	surface surf;
	if (ret && image_data.size() > 0) {
		surf = imread_mem(image_data.c_str(), image_data.size());
	}
	if (surf.get() == nullptr) {
		person.image_type = nposm;
		return;
	}

	person.feature_size = FEATURE_BLOB_SIZE;
	person.feature = (char*)malloc(FEATURE_BLOB_SIZE);
	{
		std::map<int, tst_model>::const_iterator find_it = game_config::st_models.find(FEATURE_BLOB_SIZE);
		const tst_model& mode = find_it->second;
		memcpy(person.feature, mode.example_feature_dat.c_str(), mode.example_feature_dat.size());
	}
	
	person.image_len = image_data.size();
	VALIDATE(person.image_len > 4, null_str);

	person.image_data = (uint8_t*)malloc(person.image_len);
	memcpy(person.image_data, image_data.c_str(), person.image_len);

	if (person.image_type == nposm) {
		// if register use combo-identify, image_type is nposm.
		if (person.image_data[0] == 0x89 &&
			person.image_data[1] == 'P' &&
			person.image_data[2] == 'N' &&
			person.image_data[3] == 'G' ) {
			person.image_type = img_png;
		} else {
			person.image_type = img_jpg;
		}
	}
}

std::string thttp_agbox_person_getInfo::post(Json::Value& result)
{
	pic_url = agbox_base_person_from_json(result, person, valid);
	return null_str;
}

// agbox/person --> getHouse
class thttp_agbox_person_getHouse: public ihttp_agbox
{
public:
	thttp_agbox_person_getHouse(const tvillage& village, bool exclude, int idtype, const std::string& idnumber, std::vector<tcs_house>& houses, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "getHouse", quiet)
		, village(village)
		, exclude(exclude)
		, idtype(idtype)
		, idnumber(idnumber)
		, houses(houses)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const tvillage& village;
	bool exclude;
	const int idtype;
	const std::string idnumber;
	std::vector<tcs_house>& houses;
};

void thttp_agbox_person_getHouse::verify_param()
{
	VALIDATE(game_config::idtypes.count(idtype) != 0 && !idnumber.empty(), null_str);
	VALIDATE(houses.empty(), null_str);
}

void thttp_agbox_person_getHouse::pre(Json::Value& json_params) const
{
	json_params["credentialType"] = idtype;
	json_params["credentialNo"] = idnumber;
}

std::string thttp_agbox_person_getHouse::post(Json::Value& result)
{
	Json::Value& house_list = result["house"];
	if (house_list.isArray()) {
		for (int at = 0; at < (int)house_list.size(); at ++) {
			const Json::Value& item = house_list[at];
			const std::string village_code = item["villageCode"].asString();
			if (village_code == village.uuid) {
				if (exclude) {
					continue;
				}
			} else if (!exclude) {
				continue;
			}
			const std::string building_code = item["buildingCode"].asString();
			const std::string house_code = item["houseCode"].asString();
			const int houserel = item["houseRelCode"].asInt();
			houses.push_back(tcs_house(village_code, building_code, house_code, houserel));
		}
	}
	return null_str;
}

// agbox/person --> bindingHouse
class thttp_agbox_person_bindingHouse: public ihttp_agbox
{
public:
	thttp_agbox_person_bindingHouse(int idtype, const std::string& idnumber, const std::vector<tcs_house>& houses, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "bindingHouse", quiet)
		, idtype(idtype)
		, idnumber(idnumber)
		, houses(houses)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	int idtype;
	const std::string idnumber;
	const std::vector<tcs_house>& houses;
};

void thttp_agbox_person_bindingHouse::verify_param()
{
	VALIDATE(game_config::idtypes.count(idtype) != 0 && !idnumber.empty(), null_str);
}

void thttp_agbox_person_bindingHouse::pre(Json::Value& json_params) const
{
	json_params["credentialType"] = idtype;
	json_params["credentialNo"] = idnumber;
	json_params["aodaDoorPlugin"] = false;

	Json::Value json_houses, json_house;
	for (std::vector<tcs_house>::const_iterator it = houses.begin(); it != houses.end(); ++ it) {
		const tcs_house& house = *it;
		json_house["villageCode"] = house.village;
		json_house["buildingCode"] = house.building;
		json_house["houseCode"] = house.house;
		json_house["HouseRelCode"] = house.houserel;
		json_houses.append(json_house);
	}
	if (!json_houses.empty()) {
		json_params["house"] = json_houses;
	} else {
		// must not "house: null", must be "house: []". else will result error.
		// must use empty array.
		json_params["house"].resize(0);
	}
}

std::string thttp_agbox_person_bindingHouse::post(Json::Value& result)
{
	return null_str;
}

// if use "agbox/person --> getCar", it will return both enabled=false and enabled=true cars.
class thttp_agbox_share_car_getPlate: public ihttp_agbox
{
public:
	thttp_agbox_share_car_getPlate(int idtype, const std::string& idnumber, std::vector<tcs_car>& cars, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/car"), "getPlate", quiet)
		, idtype(idtype)
		, idnumber(idnumber)
		, cars(cars)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const int idtype;
	const std::string idnumber;
	std::vector<tcs_car>& cars;
};

void thttp_agbox_share_car_getPlate::verify_param()
{
	VALIDATE(game_config::idtypes.count(idtype) != 0 && !idnumber.empty(), null_str);
}

void thttp_agbox_share_car_getPlate::pre(Json::Value& json_params) const
{
	json_params["pageSize"] = 100000;
    json_params["page"] = 1;
	json_params["credentialType"] = idtype;
	json_params["credentialNo"] = idnumber;
	json_params["enabled"] = true;
}

std::string thttp_agbox_share_car_getPlate::post(Json::Value& result)
{
	std::stringstream ss;
	Json::Value& car_list = result["plateList"];
	if (car_list.isArray()) {
		for (int at = 0; at < (int)car_list.size(); at ++) {
			const Json::Value& item = car_list[at];
			int platetype = item["plateTypeCode"].asInt();
			const std::string no = item["plateNo"].asString();
			int cartype = item["carTypeCode"].asInt();
			tcs_car car(platetype, no, cartype);
			if (car.valid()) {
				cars.push_back(car);
			}
		}
	}
	return null_str;
}

// agbox/share/car --> getPlateInfo
class thttp_agbox_share_car_getPlateInfo: public ihttp_agbox
{
public:
	thttp_agbox_share_car_getPlateInfo(tcs_car& car, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/car"), "getPlateInfo", quiet)
		, car(car)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	tcs_car& car;
};

void thttp_agbox_share_car_getPlateInfo::verify_param()
{
	VALIDATE(car.valid(), null_str);
}

void thttp_agbox_share_car_getPlateInfo::pre(Json::Value& json_params) const
{
	json_params["plateNo"] = car.no;
	json_params["plateTypeCode"] = car.platetype;
}

std::string thttp_agbox_share_car_getPlateInfo::post(Json::Value& result)
{
	std::stringstream ss;

	Json::Value& parking_list = result["set"];
	if (parking_list.isArray()) {
		int s = parking_list.size();
		for (int at = 0; at < (int)parking_list.size(); at ++) {
			const Json::Value& item = parking_list[at];
			const std::string id = item["device"].asString();
			int64_t starttime = datetime_str_2_ts(item["startTime"].asString());
			int64_t endtime = datetime_str_2_ts(item["endTime"].asString());
			tcs_car::tparking parking(id, starttime, endtime);
			if (parking.valid()) {
				car.parkings.push_back(parking);
			}
		}
	}
	return null_str;
}

// agbox/share/car --> updatePlate
class thttp_agbox_share_car_updatePlate: public ihttp_agbox
{
public:
	thttp_agbox_share_car_updatePlate(int idtype, const std::string& idnumber, const tcs_car& car, bool enabled, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/car"), "updatePlate", quiet)
		, idtype(idtype)
		, idnumber(idnumber)
		, car(car)
		, enabled(enabled)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const int idtype;
	const std::string idnumber;
	const tcs_car& car;
	const bool enabled;
};

void thttp_agbox_share_car_updatePlate::verify_param()
{
	VALIDATE(game_config::idtypes.count(idtype) != 0 && !idnumber.empty(), null_str);
	VALIDATE(car.valid(), null_str);
}

void thttp_agbox_share_car_updatePlate::pre(Json::Value& json_params) const
{
	json_params["credentialType"] = idtype;
	json_params["credentialNo"] = idnumber;

	json_params["plateNo"] = car.no;
    json_params["plateTypeCode"] = car.platetype;
    json_params["carTypeCode"] = car.cartype;
    json_params["enabled"] = enabled;

	// even if parkings is empty, still require to send set:[], overwrite previous value.
	Json::Value json_sets, json_set;
	for (std::vector<tcs_car::tparking>::const_iterator it = car.parkings.begin(); it != car.parkings.end(); ++ it) {
		const tcs_car::tparking& parking = *it;
		json_set["device"] = parking.id;
		json_set["startTime"] = format_time_ymdhms3(parking.starttime);
		json_set["endTime"] = format_time_ymdhms3(parking.endtime);
		json_set["channel"] = 0;
		json_sets.append(json_set);
	}
	json_params["set"] = json_sets;
}

std::string thttp_agbox_share_car_updatePlate::post(Json::Value& result)
{
	return null_str;
}

// agbox/share/card --> updateCard
class thttp_agbox_share_card_updateCard: public ihttp_agbox
{
public:
	thttp_agbox_share_card_updateCard(int idtype, const std::string& idnumber, const tcs_card& card, bool enabled, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/card"), "updateCard", quiet)
		, idtype(idtype)
		, idnumber(idnumber)
		, card(card)
		, enabled(enabled)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const int idtype;
	const std::string idnumber;
	const tcs_card& card;
	const bool enabled;
};

void thttp_agbox_share_card_updateCard::verify_param()
{
	VALIDATE(game_config::idtypes.count(idtype) != 0 && !idnumber.empty(), null_str);
	VALIDATE(card.valid(), null_str);
}

void thttp_agbox_share_card_updateCard::pre(Json::Value& json_params) const
{
	json_params["credentialType"] = idtype;
	json_params["credentialNo"] = idnumber;

	json_params["cardNo"] = card.number;
    json_params["typeCode"] = card.type;
    json_params["enabled"] = enabled;
}

std::string thttp_agbox_share_card_updateCard::post(Json::Value& result)
{
	return null_str;
}

// agbox/share/card --> getCardList
class thttp_agbox_share_card_getCardList: public ihttp_agbox
{
public:
	thttp_agbox_share_card_getCardList(int idtype, const std::string& idnumber, int cardtype, std::vector<tcs_card>& cards, bool quiet)
		: ihttp_agbox(form_url2("agbox/share/card"), "getCardList", quiet)
		, idtype(idtype)
		, idnumber(idnumber)
		, cardtype(cardtype)
		, cards(cards)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const int idtype;
	const std::string idnumber;
	const int cardtype;
	std::vector<tcs_card>& cards;
};

void thttp_agbox_share_card_getCardList::verify_param()
{
	VALIDATE(game_config::idtypes.count(idtype) != 0 && !idnumber.empty(), null_str);
	VALIDATE(cards.empty(), null_str);
	VALIDATE(cardtype == nposm || game_config::cardtypes.count(cardtype) != 0, null_str);
}

void thttp_agbox_share_card_getCardList::pre(Json::Value& json_params) const
{
	json_params["pageSize"] = 100000;
    json_params["page"] = 1;

	json_params["credentialType"] = idtype;
	json_params["credentialNo"] = idnumber;

	if (cardtype != nposm) {
		json_params["typeCode"] = cardtype;
	}
    json_params["enabled"] = true;
}

std::string thttp_agbox_share_card_getCardList::post(Json::Value& result)
{
	// int count = result["count"].asInt();
	Json::Value& card_list = result["cardList"];
	if (card_list.isArray()) {
		for (int at = 0; at < (int)card_list.size(); at ++) {
			const Json::Value& item = card_list[at];

			int type = item["cardTypeCode"].asInt();
			const std::string number = item["cardNo"].asString();

			if (!number.empty()) {
				cards.push_back(tcs_card(type, number));
			}
		}
	}
	return null_str;
}

bool multivlg_do_queryface(const tvillage& village, tperson& person, bool quiet)
{
	std::string pic_url;
	bool ret = true;
	{
		thttp_agbox_person_getInfo entity(person, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		if (!entity.valid) {
			// if this person isn't exist, return false.
			return false;
		}
		pic_url = entity.pic_url;
	}

	{
		thttp_agbox_person_getHouse entity(village, false, person.idtype, person.idnumber, person.houses, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		person.signout = person.houses.empty();
	}

	{
		thttp_agbox_share_car_getPlate entity(person.idtype, person.idnumber, person.cars, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}
	for (int at = 0; at < (int)person.cars.size(); at ++) {
		// get park information
		tcs_car& car = person.cars.at(at);
		thttp_agbox_share_car_getPlateInfo entity(car, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}

	if (person.card.valid()) {
		std::vector<tcs_card> cards;
		thttp_agbox_share_card_getCardList entity(person.idtype, person.idnumber, nposm, cards, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		for (std::vector<tcs_card>::const_iterator it = cards.begin(); it != cards.end(); ++ it) {
			const tcs_card& card = *it;
			if (card.number == person.card.number) {
				person.card.type = card.type;
			}
		}
	}

	// get image will generate heap data(feature/image_data), so last execute.
	if (!pic_url.empty()) {
		agbox_person_image_from_idCardPicUrl(pic_url, person);
	}
	if (person.image_type == nposm) {
		if (!quiet) {
			gui2::show_message(null_str, _("Xmit fail. Get feature or image fail. Try again."));
		}
		return false;
	}
	VALIDATE(person.feature != nullptr && person.image_data != nullptr && person.image_len > 0, null_str);
	return true;
}

// agbox/village --> getPerson
class thttp_agbox_village_getPerson: public ihttp_agbox
{
public:
	thttp_agbox_village_getPerson(const tvillage& village, const tqueryperson_key& key, int page_size, int page, std::vector<tperson>& persons, bool quiet)
		: ihttp_agbox(form_url2("agbox/village"), "getPerson", quiet)
		, village(village)
		, key(key)
		, page_size(page_size)
		, page(page)
		, persons(persons)
		, count(0)
	{}

private:
	void verify_param() override;
	void pre(Json::Value& json_params) const override;
	std::string post(Json::Value& result) override;

public:
	const tvillage& village;
	const tqueryperson_key& key;
	const int page_size;
	const int page;
	std::vector<tperson>& persons;
	int count;
};

void thttp_agbox_village_getPerson::verify_param()
{
	VALIDATE(key.valid(), null_str);
	VALIDATE(page_size > 0, null_str);
	VALIDATE(page >= 1, null_str);
}

void thttp_agbox_village_getPerson::pre(Json::Value& json_params) const
{
	json_params["pageSize"] = str_cast(page_size);
	json_params["page"] = str_cast(page);

	json_params["villageCode"] = village.uuid;
	if (key.idtype != nposm) {
		json_params["credentialType"] = key.idtype;
	}
	if (!key.idnumber.empty()) {
		json_params["credentialNo"] = key.idnumber;
	}
	if (!key.name.empty()) {
		json_params["name"] = key.name;
	}
	if (!key.types.empty()) {
		json_params["entranceTypes"] = utils::join(key.types);
	}
}

std::string thttp_agbox_village_getPerson::post(Json::Value& result)
{
	count = result["count"].asInt();

	std::stringstream key_ss, phones_ss;
	Json::Value& person_list = result["person"];
	if (person_list.isArray()) {
		for (int at = 0; at < (int)person_list.size(); at ++) {
			const Json::Value& item = person_list[at];
			persons.push_back(tperson());
			tperson& person = persons.back();

			person.idtype = item["credentialType"].asInt();
			person.idnumber = item["credentialNo"].asString();

			person.name = item["name"].asString();
			person.type = item["personEntranceTypeCode"].asInt();
			person.peopletype = item["personTypeCode"].asInt();
			person.card = tcs_card(item["cardNumber"].asString());

			const int max_phones = 3;
			for (int at2 = 0; at2 < max_phones; at2 ++) {
				key_ss.str("");
				key_ss << "phone" << (at2 + 1);
				std::string phone = item[key_ss.str()].asString();
				if (!phone.empty()) {
					if (!phones_ss.str().empty()) {
						phones_ss << ",";
					}
					phones_ss << phone;
				}
			}
			person.phones = phones_ss.str();
			person.registertime = datetime_str_2_ts(item["updateTime"].asString());
		}
	}
	return null_str;
}

bool multivlg_do_findface(const tvillage& village, const tqueryperson_key& key, int status_type, int* count_ptr, std::vector<tperson>& persons)
{
	if (count_ptr != nullptr) {
		VALIDATE(*count_ptr == 0, null_str);
	}

	bool quiet = false;
	bool ret = true;

	{
		int page_size = 20;
		VALIDATE(page_size <= FINDFACE_PAGE_SIZE, null_str);
		int page = 1;
		thttp_agbox_village_getPerson entity(village, key, page_size, page, persons, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		if (count_ptr != nullptr) {
			*count_ptr = entity.count;
		}
	}

	bool found = false;
	for (std::vector<tperson>::iterator it = persons.begin(); it != persons.end(); ++ it) {
		tperson& person = *it;
		thttp_agbox_person_getHouse entity(village, false, person.idtype, person.idnumber, person.houses, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		person.signout = person.houses.empty();
		if (person.idtype == key.idtype && person.idnumber == key.idnumber) {
			found = true;
		}
	}

	if (!found && key.idtype != nposm && game_config::idtypes.count(key.idtype) != 0 && !key.idnumber.empty()) {
		// getPerson is relative special villageCode, this person maybe in system and not in this villageCode.
		// since not in this villagCode, he always no house in this villageCode.
		tperson person;
		person.idtype = key.idtype;
		person.idnumber = key.idnumber;

		thttp_agbox_person_getInfo entity(person, quiet);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		if (entity.valid) {
			// this person always is first line.
			person.signout = person.houses.empty();
			persons.insert(persons.begin(), person);
			if (count_ptr != nullptr) {
				*count_ptr = *count_ptr + 1;
			}

		}
	}
	return true;
}

// agbox/person --> update
class thttp_agbox_person_update: public ihttp_agbox
{
public:
	thttp_agbox_person_update(tperson& person, bool quiet)
		: ihttp_agbox(form_url2("agbox/person"), "update", quiet)
		, person(person)
		, idpic_key("idpic")
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return TIMEOUT_10S; }
	void pre(Json::Value& json_params) const override;
	void pre_binary(std::map<std::string, std::string>& data) const override;
	std::string post(Json::Value& result) override;

public:
	tperson& person;
	const std::string idpic_key;
};

void thttp_agbox_person_update::verify_param()
{
	VALIDATE(game_config::idtypes.count(person.idtype) != 0 && !person.idnumber.empty(), null_str);
}

void thttp_agbox_person_update::pre(Json::Value& json_params) const
{
	json_params["credentialType"] = person.idtype;
	json_params["credentialNo"] = person.idnumber;

	json_params["peopleName"] = person.name;
	json_params["source"] = 4;
	json_params["entranceTypeCode"] = person.type;
	json_params["typeCode"] = person.peopletype;
	if (!person.nationality.empty()) {
		json_params["nationalityCode"] = person.nationality;
	}

	if (person.education != nposm) {
		json_params["educationCode"] = person.education;
	}
	if (person.marital != nposm) {
		json_params["maritalStatusCode"] = person.marital;
	}
	json_params["SecurityCardNo"] = person.card.number;

	// domicile
	Json::Value json_domicile;
	json_domicile["nationCode"] = nationcode_2_nation(person.nation);
	int64_t birth = date_str_2_ts(person.birth);
	if (birth != 0) {
		json_domicile["birthDate"] = format_time_ymd2(birth, '-', true);
	}
	json_domicile["genderCode"] = person.gender;
	json_domicile["IDPic"] = idpic_key;

	// residence
	Json::Value json_residence;

	std::vector<std::pair<const tcode3*, Json::Value*> > codes;
	codes.push_back(std::make_pair(&person.code3, &json_domicile));
	codes.push_back(std::make_pair(&person.residence_code3, &json_residence));

	for (std::vector<std::pair<const tcode3*, Json::Value*> >::const_iterator it = codes.begin(); it != codes.end(); ++ it) {
		const tcode3& code3 = *it->first;
		Json::Value& object = *it->second;

		object["address"] = code3.address;
		int64_t province_code = 0;
		if (game_config::provinces.count(code3.province) != 0) {
			province_code = code3.province;
		}
		object["provinceCode"] = province_code;
		int64_t city_code = 0, district_code = 0;
		if (code3.district != nullptr) {
			city_code = code3.district->city_code;
			district_code = code3.district->code;
		}
		object["cityCode"] = city_code;
		object["districtCode"] = district_code;
		int64_t street_code = 0;
		if (code3.street != nullptr) {
			street_code = code3.street->code;
		}
		object["streetCode"] = street_code;
	}

	json_params["domicile"] = json_domicile;
	json_params["residence"] = json_residence;

	const int max_phones = 3;
	std::stringstream key_ss;
	int at = 0;
	std::vector<std::string> phones = utils::split(person.phones, ',');
	for (int at = 0; at < max_phones; at ++) {
		// as if no phone, must be exsite and override previous value.
		Json::Value json_phone;
		json_phone["no"] = (int)phones.size() > at? phones.at(at): null_str;
		key_ss.str("");
		key_ss << "phone" << (at + 1);
		json_params[key_ss.str()] = json_phone;
	}
}

void thttp_agbox_person_update::pre_binary(std::map<std::string, std::string>& data) const
{
	data.insert(std::make_pair(idpic_key, std::string((const char*)person.image_data, person.image_len)));
}

std::string thttp_agbox_person_update::post(Json::Value& result)
{
	return null_str;
}

bool multivlg_do_updateface(const tvillage& village, tperson& person, bool insert, int* existed_code)
{
	bool ret = true;
	{
		thttp_agbox_person_update entity(person, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}

	//
	// update cars
	//
	// disalbe cars that are no longer in use(step#1/2)
	std::vector<tcs_car> previous_cars;
	{
		thttp_agbox_share_car_getPlate entity(person.idtype, person.idnumber, previous_cars, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}
	for (std::vector<tcs_car>::const_iterator it = person.cars.begin(); it != person.cars.end(); ++ it) {
		const tcs_car& car = *it;
		thttp_agbox_share_car_updatePlate entity(person.idtype, person.idnumber, car, true, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
		for (std::vector<tcs_car>::iterator it2 = previous_cars.begin(); it2 != previous_cars.end(); ++ it2) {
			const tcs_car& car2 = *it2;
			if (car2.platetype == car.platetype && car2.no == car.no) {
				previous_cars.erase(it2);
				break;
			}
		}
	}
	// disalbe cars that are no longer in use(step#2/2)
	for (std::vector<tcs_car>::const_iterator it = previous_cars.begin(); it != previous_cars.end(); ++ it) {
		const tcs_car& car = *it;
		thttp_agbox_share_car_updatePlate entity(person.idtype, person.idnumber, car, false, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}

	//
	// update card
	//
	std::vector<tcs_card> cards;
	thttp_agbox_share_card_getCardList entity(person.idtype, person.idnumber, nposm, cards, false);
	ret = multivlg_do_agbox(entity);
	if (!ret) {
		return false;
	}
	tcs_card enable_card = person.card;
	for (std::vector<tcs_card>::const_iterator it = cards.begin(); it != cards.end(); ++ it) {
		const tcs_card& card = *it;
		if (card.type == enable_card.type && card.number == enable_card.number) {
			person.card.type = card.type;
			enable_card.clear();

		} else {
			thttp_agbox_share_card_updateCard entity(person.idtype, person.idnumber, card, false, false);
			ret = multivlg_do_agbox(entity);
			if (!ret) {
				return false;
			}
		}
	}

	if (enable_card.valid()) {
		const tcs_card& card = enable_card;
		thttp_agbox_share_card_updateCard entity(person.idtype, person.idnumber, card, true, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}

	//
	// update house
	//
	std::vector<tcs_house> houses;
	{
		// other village houses
		thttp_agbox_person_getHouse entity(village, true, person.idtype, person.idnumber, houses, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}

	houses.insert(houses.end(), person.houses.begin(), person.houses.end());
	{
		thttp_agbox_person_bindingHouse entity(person.idtype, person.idnumber, houses, false);
		ret = multivlg_do_agbox(entity);
		if (!ret) {
			return false;
		}
	}
	return true;

}
*/
}