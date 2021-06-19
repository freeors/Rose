#define GETTEXT_DOMAIN "studio-lib"

/*
 * How to use...
 * display_lock lock(game.disp());
 * hotkey::scope_changer changer(game.app_cfg(), "hotkey_ocr");
 * simple_controller controller(game.app_cfg(), game.video());
 * controller.initialize(display::ZOOM_72);
 * int ret = controller.main_loop();
 */

#include "simple_controller.hpp"
#include "simple_display.hpp"
#include "gui/dialogs/simple_scene.hpp"

void square_fill_frame(t_translation::t_map& tiles, size_t start, const t_translation::t_terrain& terrain, bool front, bool back)
{
	if (front) {
		// first column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[start][n] = terrain;
		}
		// first line(border)
		for (size_t n = start + 1; n < tiles.size() - start; n ++) {
			tiles[n][start] = terrain;
		}
	}

	if (back) {
		// last column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[tiles.size() - start - 1][n] = terrain;
		}
		// last line(border)
		for (size_t n = start; n < tiles.size() - start - 1; n ++) {
			tiles[n][tiles[0].size() - start - 1] = terrain;
		}
	}
}

std::string generate_map_data(int width, int height, bool colorful)
{
	VALIDATE(width > 0 && height > 0, "map must not be empty!");

	const t_translation::t_terrain normal = t_translation::read_terrain_code("Gg");
	const t_translation::t_terrain border = t_translation::read_terrain_code("Gs");
	const t_translation::t_terrain control = t_translation::read_terrain_code("Gd");
	const t_translation::t_terrain forbidden = t_translation::read_terrain_code("Gll");

	// t_translation::t_map tiles(width + 2, t_translation::t_list(height + 2, normal));
	t_translation::t_map tiles(width + 2, t_translation::t_list(height + 2, forbidden));
	if (colorful) {
		square_fill_frame(tiles, 0, border, true, true);
		square_fill_frame(tiles, 1, control, true, false);

		const size_t border_size = 1;
		tiles[border_size][border_size] = forbidden;
	}

	// tiles[border_size][tiles[0].size() - border_size - 1] = forbidden;

	// tiles[tiles.size() - border_size - 1][border_size] = forbidden;
	// tiles[tiles.size() - border_size - 1][tiles[0].size() - border_size - 1] = forbidden;

	std::string str = tmap::default_map_header + t_translation::write_game_map(t_translation::t_map(tiles));

	return str;
}

simple_controller::simple_controller(const config& app_cfg, CVideo& video)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, gui_(nullptr)
	, map_(null_str)
	, units_(*this, map_, false)
{
	map_ = tmap(generate_map_data(12, 8, true));
	units_.create_coor_map(map_.w(), map_.h());
}

simple_controller::~simple_controller()
{
	if (gui_) {
		delete gui_;
		gui_ = nullptr;
	}
}

void simple_controller::app_create_display(int initial_zoom)
{
	gui_ = new simple_display(*this, units_, video_, map_, initial_zoom);
}

void simple_controller::app_post_initialize()
{
}

void simple_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;

	switch (command) {
		case tsimple_scene::HOTKEY_RETURN:
			do_quit_ = true;
			break;

		case tsimple_scene::HOTKEY_SHARE:
			// share();
			break;

		case HOTKEY_ZOOM_IN:
			gui_->set_zoom(gui_->zoom());
			break;
		case HOTKEY_ZOOM_OUT:
			gui_->set_zoom(- gui_->zoom() / 2);
			break;

		case HOTKEY_SYSTEM:
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}