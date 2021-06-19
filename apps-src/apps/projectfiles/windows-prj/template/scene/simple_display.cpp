#define GETTEXT_DOMAIN "studio-lib"

#include "simple_display.hpp"
#include "simple_controller.hpp"
#include "unit_map.hpp"
#include "gui/dialogs/simple_scene.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "halo.hpp"
#include "formula_string_utils.hpp"

simple_display::simple_display(simple_controller& controller, unit_map& units, CVideo& video, const tmap& map, int initial_zoom)
	: display(game_config::tile_square, controller, video, &map, gui2::tsimple_scene::NUM_REPORTS, initial_zoom)
	, controller_(controller)
	, units_(units)
{
	min_zoom_ = 64;
	max_zoom_ = 1024;

	show_hover_over_ = false;
	set_grid(true);
}

simple_display::~simple_display()
{
}

gui2::tdialog* simple_display::app_create_scene_dlg()
{
	return new gui2::tsimple_scene(controller_);
}

void simple_display::app_post_initialize()
{
}

void simple_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tsimple_scene::POSITION, reports::report(reports::report::LABEL, lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::tsimple_scene::ZOOM, reports::report(reports::report::LABEL, ss.str(), null_str));
}