#ifndef SIMPLE_CONTROLLER_HPP_INCLUDED
#define SIMPLE_CONTROLLER_HPP_INCLUDED

#include "base_controller.hpp"
#include "mouse_handler_base.hpp"
#include "simple_display.hpp"
#include "unit_map.hpp"
#include "map.hpp"

class simple_controller : public base_controller, public events::mouse_handler_base
{
public:
	simple_controller(const config &app_cfg, CVideo& video);
	~simple_controller();

private:
	void app_create_display(int initial_zoom) override;
	void app_post_initialize() override;

	void app_execute_command(int command, const std::string& sparam) override;

	simple_display& gui() { return *gui_; }
	const simple_display& gui() const { return *gui_; }
	events::mouse_handler_base& get_mouse_handler_base() override { return *this; }
	simple_display& get_display() override { return *gui_; }
	const simple_display& get_display() const override { return *gui_; }

	unit_map& get_units() override { return units_; }
	const unit_map& get_units() const override { return units_; }

private:
	unit_map units_;
	tmap map_;
	simple_display* gui_;
};

#endif
