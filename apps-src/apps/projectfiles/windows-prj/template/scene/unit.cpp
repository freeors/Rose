#define GETTEXT_DOMAIN "studio-lib"

#include "unit.hpp"
#include "gettext.hpp"
#include "simple_display.hpp"
#include "simple_controller.hpp"
#include "gui/dialogs/simple_scene.hpp"

unit::unit(simple_controller& controller, simple_display& disp, unit_map& units)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
{
}

void unit::app_draw_unit(const int xsrc, const int ysrc)
{
}