#define GETTEXT_DOMAIN "studio-lib"

#include "base_instance.hpp"
#include "preferences_display.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/tools.hpp"
#include "gui/dialogs/anim.hpp"
#include "gui/dialogs/opencv.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "formula_string_utils.hpp"
#include "version.hpp"
#include "mkwin_controller.hpp"
#include "help.hpp"

#include <errno.h>
#include <iostream>

class game_instance: public base_instance
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);

	void app_fill_anim_tags(std::map<const std::string, int>& tags) override;

	void start_mkwin(const std::map<std::string, std::string>& app_tdomains);
	void edit_animation();

private:
	void app_load_settings_config(const config& cfg) override;
	void app_pre_setmode(tpre_setmode_settings& settings) override;
};

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
{
	teditor_ editor_(game_config::path);
	editor_.make_system_bins_exist();
}

void game_instance::app_fill_anim_tags(std::map<const std::string, int>& tags)
{
	// although don't use below animation, but pass program verify, define them still.
}

void game_instance::start_mkwin(const std::map<std::string, std::string>& app_tdomains)
{
	hotkey::scope_changer changer(core_cfg(), "hotkey_mkwin");

	mkwin_controller mkwin(core_cfg(), video_, app_tdomains);
	mkwin.initialize(64 * gui2::twidget::hdpi_scale);
	mkwin.main_loop();
}

void game_instance::edit_animation()
{
	gui2::tanim3 dlg(app_cfg_);
	dlg.show();
}

void game_instance::app_load_settings_config(const config& cfg)
{
	game_config::logo_png = cfg["log_png"].str();
	game_config::version = game_config::rose_version;

	game_config::absolute_path = game_config::path + "/absolute";
	game_config::apps_src_path = extract_directory(game_config::path) + "/apps-src";
	if (!check_apps_src_folder(game_config::apps_src_path)) {
		game_config::apps_src_path.clear();
	}

	preferences::set_theme(utils::generate_app_prefix_id("rose", "default"));
}

void game_instance::app_pre_setmode(tpre_setmode_settings& settings)
{
	settings.landscape = true;
}

#include <expat.h>

void start_element(void* userdata, const char* name, const char** atts)
{
	int i;
	int* depth_ptr = (int*)userdata;
	SDL_Log("%s", name);

	for (i = 0; atts[i]; i += 2) {
		SDL_Log("\t%s='%s'", atts[i], atts[i + 1]);
	}
	*depth_ptr += 1;
}

void end_element(void* userdata, const char* name)
{
	int* depth_ptr = (int*)userdata;
	*depth_ptr -= 1;
}

void test_xml(const std::string& file_name)
{
	XML_Parser parser = XML_ParserCreate(NULL);
	int done;
	int depth = 0;

	const int one_block = 1024 * 1024;
	tfile file(file_name, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);
	file.resize_data(one_block);

	XML_SetUserData(parser, &depth);
	XML_SetElementHandler(parser, start_element, end_element);
	do {
		int byte_read = posix_fread(file.fp, file.data, one_block);
		if (byte_read == 0) {
			break;
		}
		done = byte_read < file.data_size;
		if (!XML_Parse(parser, file.data, byte_read, done)) {
			SDL_Log("%s at line %d\n", XML_ErrorString(XML_GetErrorCode (parser)), XML_GetCurrentLineNumber (parser));
			return;
		}
	} while (!done);
	XML_ParserFree(parser);		
}

bool did_walk_imageset_path(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::string>& files, const std::vector<std::string>& white_extname)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		// const std::string name = utils::lowercase(dirent->name);
		const std::string name = dirent->name;

		bool hit = false;
		for (std::vector<std::string>::const_iterator it = white_extname.begin(); it != white_extname.end(); ++ it) {
			const std::string& extname = *it;
			size_t pos = name.rfind(extname);
			if (pos != std::string::npos && pos + extname.size() == name.size()) {
				hit = true;
				break;
			}
		}
		if (hit) {
			files.push_back(dir + "/" + dirent->name);
		}
	}
	return true;
}

void rename_img_files()
{
	std::vector<std::string> paths;
	paths.push_back("C:/ddksample/apps-res/data/core");
	paths.push_back("C:/ddksample/apps-res/aplt_leagor_iaccess");
	paths.push_back("C:/ddksample/apps-res/aplt_leagor_key");
	paths.push_back("C:/ddksample/apps-res/app-aismart");
	paths.push_back("C:/ddksample/apps-res/app-iaccess");
	paths.push_back("C:/ddksample/apps-res/app-kdesktop");
	paths.push_back("C:/ddksample/apps-res/app-launcher");
	paths.push_back("C:/ddksample/apps-res/app-studio");

	std::vector<std::string> white_extname;
	white_extname.push_back(".png");
	white_extname.push_back(".jpg");
	white_extname.push_back(".jpeg");

	// except misc/assets or icon-192.png
	std::vector<std::string> images;
	for (std::vector<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
		const std::string& imageset_path = *it;
		images.clear();
		walk_dir(imageset_path, true, std::bind(&did_walk_imageset_path, _1, _2, std::ref(images), std::ref(white_extname)));

		std::vector<std::string> hit_images;
		for (std::vector<std::string>::const_iterator it = images.begin(); it != images.end(); ++ it) {
			const std::string file = extract_file(*it);
			if (file.find('-') != nposm) {
				hit_images.push_back(*it);
			}
		}

		for (std::vector<std::string>::const_iterator it = hit_images.begin(); it != hit_images.end(); ++ it) {
			const std::string& src = *it;
			const std::string file = extract_file(*it);
			VALIDATE(file.find('-') != nposm, null_str);
			if (src.find("assets") == nposm && file != "icon-192.png") {
				const std::string dst = utils::replace_all_char(file, '-', '_');
				SDL_Log("rename_img_files, change %s to %s", file.c_str(), dst.c_str());
				SDL_RenameFile(src.c_str(), dst.c_str());
			}
		}
	}
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "studio", "#rose");
	game_instance& game = manager.get();

	if (game_config::os == os_windows) {
		// test_xml("C:/ddksample/apps-src/apps/projectfiles/vc/applet.vcxproj.filters");
		rename_img_files();
	}

	try {
		// default name maybe all digit, not safied to irc nick protocol.
		lobby->set_nick2("studio");

		std::map<std::string, std::string> app_tdomains;
		for (;;) {
			game.loadscreen_manager().reset();

			gui2::thome::tresult res = gui2::thome::NOTHING;

			const font::floating_label_context label_manager;

			cursor::set(cursor::NORMAL);

			if (res == gui2::thome::NOTHING) {
				// load/reload hero_map from file
				gui2::thome dlg(group.leader());
				dlg.show();
				res = static_cast<gui2::thome::tresult>(dlg.get_retval());
				app_tdomains = dlg.get_app_tdomains();
			}

			if (res == gui2::thome::QUIT_GAME) {
				SDL_Log("do_gameloop, received QUIT_GAME, will exit!\n");
				return 0;

			} else if (res == gui2::thome::WINDOW) {
				game.start_mkwin(app_tdomains);

			} else if (res == gui2::thome::ANIMATION) {
				game.edit_animation();

			} else if (res == gui2::thome::DESIGN) {
				gui2::ttools dlg;
				dlg.show();

			} else if (res == gui2::thome::CHANGE_LANGUAGE) {
				if (game.change_language()) {
					t_string::reset_translations();
					image::flush_cache();
				}

			} else if (res == gui2::thome::OPENCV) {
				gui2::topencv dlg;
				dlg.show();
			
			} else if (res == gui2::thome::MESSAGE) {
				gui2::tchat2 dlg("chat_module");
				dlg.show();
			
			} else if (res == gui2::thome::EDIT_PREFERENCES) {
				preferences::show_preferences_dialog(game.video());

			}
		}

	} catch (twml_exception& e) {
		e.show();

	} catch (CVideo::quit&) {
		//just means the game should quit
		SDL_Log("SDL_main, catched CVideo::quit\n");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(e.what());
	} 

	return 0;
}

int main(int argc, char** argv)
{
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		SDL_SimplerMB("%s", e.user_message.c_str());
	}

	return 0;
}
