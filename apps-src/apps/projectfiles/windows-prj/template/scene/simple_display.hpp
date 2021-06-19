#ifndef SIMPLE_DISPLAY_HPP_INCLUDED
#define SIMPLE_DISPLAY_HPP_INCLUDED

#include "display.hpp"

class simple_controller;
class unit_map;
class unit;

class simple_display : public display
{
public:
	simple_display(simple_controller& controller, unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~simple_display();

	bool in_theme() const override { return true; }
	simple_controller& get_controller() { return controller_; }
	
protected:
	void draw_sidebar();

private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;

private:
	simple_controller& controller_;
	unit_map& units_;
};

#endif
