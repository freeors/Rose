#ifndef STUDIO_EDITOR_HPP_INCLUDED
#define STUDIO_EDITOR_HPP_INCLUDED

#include "config_cache.hpp"
#include "config.hpp"
#include "version.hpp"
#include "task.hpp"
#include "theme.hpp"
#include "game_config.hpp"

#include <set>

using namespace std::placeholders;

extern const std::string studio_guid;

std::string extract_android_lib_stem(const std::string& lib);

class tapp_capabilities
{
public:
	struct tos {
		std::vector<std::string> extra_libs;
		std::vector<std::string> extra_LOCAL_CFLAGS;
		std::vector<std::string> extra_external_mks;
	};

	tapp_capabilities(const config& cfg)
		: app(cfg["app"].str())
		, bundle_id(cfg["bundle_id"].str())
		, ble(cfg["ble"].to_bool())
		, wifi(cfg["wifi"].to_bool())
		, bootup(cfg["bootup"].to_bool())
		, path_(cfg["path"].str())
	{
		if (path_.empty()) {
			path_ = "..";
		}

		std::stringstream ss;
		std::map<int, std::string> support_os;
		support_os.insert(std::make_pair(os_windows, "windows"));
		support_os.insert(std::make_pair(os_ios, "ios"));
		support_os.insert(std::make_pair(os_android, "android"));
		for (std::map<int, std::string>::const_iterator it = support_os.begin(); it != support_os.end(); ++ it) {
			std::pair<std::map<int, tos>::iterator, bool> ins = os_special.insert(std::make_pair(it->first, tos()));
			const std::string& os_name = it->second;
			const config& os_cfg = cfg.child(os_name);
			if (os_cfg) {
				ins.first->second.extra_libs = utils::split(os_cfg["extra_libs"].str());
				for (std::vector<std::string>::const_iterator it2 = ins.first->second.extra_libs.begin(); it2 != ins.first->second.extra_libs.end(); ++ it2) {
					const std::string& lib = *it2;
					VALIDATE(!extract_android_lib_stem(*it2).empty(), *it2 + " is not a " + os_name + " library");
				}
				ins.first->second.extra_LOCAL_CFLAGS = utils::split(os_cfg["extra_LOCAL_CFLAGS"].str());

				ins.first->second.extra_external_mks = utils::split(os_cfg["extra_external_mks"].str());
				for (std::vector<std::string>::const_iterator it2 = ins.first->second.extra_external_mks.begin(); it2 != ins.first->second.extra_external_mks.end(); ++ it2) {
					const std::string& external_mk = *it2;
					const std::string mk = game_config::apps_src_path + "/apps/external/" + external_mk;
					VALIDATE(SDL_IsFile(mk.c_str()), mk + " isn't existed");
				}
			}
		}
	}

	bool operator==(const tapp_capabilities& that) const
	{
		if (app != that.app) return false;
		if (bundle_id.id() != that.bundle_id.id()) return false;
		if ((ble && !that.ble) || (!ble && that.ble)) return false;
		if ((wifi && !that.wifi) || (!wifi && that.wifi)) return false;
		if ((bootup && !that.bootup) || (!bootup && that.bootup)) return false;
		return true;
	}
	bool operator!=(const tapp_capabilities& that) const { return !operator==(that); }

	void reset(const tapp_capabilities& that)
	{
		app = that.app;
		bundle_id.reset(that.bundle_id.id());
		ble = that.ble;
		wifi = that.wifi;
		bootup = that.bootup;
	}

	void generate2(std::stringstream& ss, const std::string& prefix) const;

public:
	std::string app;
	tdomain bundle_id;
	bool ble;
	bool wifi;
	bool bootup;
	std::set<std::string> tdomains;
	std::map<int, tos> os_special;

protected:
	std::string path_;
};

bool is_apps_kit(const std::string& res_folder);
bool is_studio_app(const std::string& app);
bool is_private_app(const std::string& app);
bool is_reserve_app(const std::string& app);
bool is_reserve2_app(const std::string& app);
bool is_reserve_aplt(const std::string& lua_bundleid);

bool check_res_folder(const std::string& folder);
bool check_apps_src_folder(const std::string& folder);

class tapp_copier;

class texporter: public ttask
{
public:
	texporter(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	bool app_filter(const tsubtask& subtask, const std::string& src, bool dir) override;
	void app_complete_paths() override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

	bool collect_cpp_files(const std::string& dir, const SDL_dirent2* dirent, std::map<std::string, uint32_t>& files) const;
	bool collect_cpp_files2(const std::string& dir, const SDL_dirent2* dirent, uint32_t& flags) const;
	bool export_android_prj() const;
	bool generate_kos_prj() const;
	bool generate_window_prj() const;

private:
	tapp_copier& copier_;
};

class tstudio_extra_exporter: public ttask
{
public:
	tstudio_extra_exporter(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	void app_complete_paths() override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;
	bool studio_export_android_prj() const;

private:
	tapp_copier& copier_;
};

class tother_extra_exporter: public ttask
{
public:
	tother_extra_exporter(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	tapp_copier& copier_;
};

class tandroid_res: public ttask
{
public:
	tandroid_res(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	void app_complete_paths() override;
	bool app_can_execute(const tsubtask& subtask, const bool last) override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

	std::string get_android_res_path();

private:
	tapp_copier& copier_;
};

class tios_kit: public ttask
{
public:
	static const std::string kit_alias;
	static const std::string studio_alias;

	tios_kit(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	bool app_post_handle(const tsubtask& subtask, const bool last) override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	tapp_copier& copier_;
};

class tapp_copier: public tapp_capabilities
{
public:
	static const std::string windows_prj_alias;
	static const std::string android_prj_alias;
	static const std::string ios_prj_alias;
	static const std::string kos_prj_alias;
	static const std::string app_windows_prj_alias;
	static const std::string app_android_prj_alias;
	static const std::string app_ios_prj_alias;
	static const std::string app_kos_prj_alias;

	tapp_copier(const config& cfg, const std::string& app = null_str);

	void app_complete_paths(std::map<std::string, std::string>& paths) const;
	bool generate_ios_prj(const ttask& task) const;

public:
	std::unique_ptr<texporter> exporter;
	std::unique_ptr<tstudio_extra_exporter> studio_extra_exporter;
	std::unique_ptr<tother_extra_exporter> other_extra_exporter;
	std::unique_ptr<tandroid_res> android_res_copier;
	std::unique_ptr<tios_kit> ios_kiter;
};

class tnewer: public ttask
{
public:
	static const std::string windows_prj_alias;

	tnewer(const config& cfg, void*)
		: ttask(cfg)
		, capab_(null_cfg)
	{}
	void set_capabilities(const tapp_capabilities& capabilities) { capab_ = capabilities; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;
	bool new_android_prj() const;

private:
	tapp_capabilities capab_;
};

class taplt_newer: public ttask
{
public:
	static const std::string windows_prj_alias;

	taplt_newer(const config& cfg, void*)
		: ttask(cfg)
		, bundleid_(null_str)
	{}
	void set_bundleid(const std::string& bundleid) { bundleid_.reset(bundleid); }

	std::vector<std::string> sln_files() const;

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	tdomain bundleid_;
};

class timporter: public ttask
{
public:
	timporter(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app, const std::string& res_path, const std::string& src2_path);

private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;

	bool import_android_prj() const;

private:
	std::string app_;
};

class tremover: public ttask
{
public:
	tremover(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app, const std::set<std::string>& tdomains)
	{ 
		app_ = app; 
		tdomains_ = tdomains;
	}

private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;
	bool did_language(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::string>& languages);

	bool remove_android_prj() const;

private:
	std::string app_;
	std::set<std::string> tdomains_;
};

class taplt_remover: public ttask
{
public:
	taplt_remover(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_aplt(const std::string& lua_bundleid)
	{ 
		lua_bundleid_ = lua_bundleid; 
	}

private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;

private:
	std::string lua_bundleid_;
};

class tnew_window: public ttask
{
public:
	tnew_window(const config& cfg)
		: ttask(cfg)
	{}
	void set_app(const std::string& app, const std::string& id);

	virtual std::vector<std::string> sln_files() = 0;
private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

protected:
	std::string app_;
	std::string id_;
};

class tnew_dialog: public tnew_window
{
public:
	tnew_dialog(const config& cfg, void*)
		: tnew_window(cfg)
	{}
	std::vector<std::string> sln_files() override;
};

class tnew_scene: public tnew_window
{
public:
	tnew_scene(const config& cfg, void*)
		: tnew_window(cfg)
		, unit_files_(false)
	{}
	std::vector<std::string> sln_files() override;
	void set_unit_files(bool val) { unit_files_ = val; }

private:
	bool app_can_execute(const tsubtask& subtask, const bool last) override;

private:
	bool unit_files_;
};

class taplt_new_dialog: public ttask
{
public:
	taplt_new_dialog(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_aplt(const std::string& lua_bundleid_, const std::string& id);

	std::vector<std::string> sln_files() const;
private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

protected:
	std::string lua_bundleid_;
	std::string id_;
};

class tvalidater: public ttask
{
public:
	static const std::string windows_prj_alias;

	tvalidater(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app) { app_ = app; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string app_;
};

class taplt_validater: public ttask
{
public:
	static const std::string windows_prj_alias;

	taplt_validater(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_aplt(const std::string& lua_bundleid) { lua_bundleid_ = lua_bundleid; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string lua_bundleid_;
};

class tsave_theme: public ttask
{
public:
	static const std::string windows_prj_alias;

	tsave_theme(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_theme(const theme::tfields& fields) { fields_ = fields; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(const tsubtask& subtask, const bool last) override;

private:
	std::string app_;
	theme::tfields fields_;
};

class tremove_theme: public ttask
{
public:
	static const std::string windows_prj_alias;

	tremove_theme(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_theme(const theme::tfields& fields) { fields_ = fields; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string app_;
	theme::tfields fields_;
};

class topencv_header: public ttask
{
public:
	static const std::string windows_prj_alias;

	topencv_header(const config& cfg, void*)
		: ttask(cfg)
	{}

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string project_;
};

#endif