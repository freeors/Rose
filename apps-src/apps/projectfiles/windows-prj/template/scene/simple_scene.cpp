#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/simple_scene.hpp"

#include "simple_controller.hpp"
#include "hotkeys.hpp"
#include "gettext.hpp"

namespace gui2 {

REGISTER_DIALOG(studio, simple_scene);

tsimple_scene::tsimple_scene(simple_controller& controller)
	: tdialog(&controller)
{
}

void tsimple_scene::pre_show()
{
	// prepare status report.
	reports_.insert(std::make_pair(ZOOM, "zoom"));
	reports_.insert(std::make_pair(POSITION, "position"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_RETURN, "return", _("Return to title"));
	hotkey::insert_hotkey(HOTKEY_SHARE, "share", _("Share"));

	tbutton* widget = dynamic_cast<tbutton*>(get_object("return"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("share"));
	click_generic_handler(*widget, null_str);
}

} //end namespace gui2
