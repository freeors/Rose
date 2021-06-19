#define GETTEXT_DOMAIN "simple-lib"

#include "unit_map.hpp"
#include "simple_display.hpp"
#include "simple_controller.hpp"
#include "gui/dialogs/simple_scene.hpp"

unit_map::unit_map(simple_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
{
}

void unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const unit* u = dynamic_cast<const unit*>(base_u);
	insert(loc, new unit(*u));
}

unit* unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<unit*>(find_base_unit(loc));
}

unit* unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<unit*>(find_base_unit(loc, overlay));
}