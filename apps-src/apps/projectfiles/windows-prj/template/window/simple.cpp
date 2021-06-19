#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/simple.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

namespace gui2 {

REGISTER_DIALOG(studio, simple)

tsimple::tsimple()
{
}

void tsimple::pre_show()
{
	window_->set_label("misc/white_background.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("Hello World"));
}

void tsimple::post_show()
{
}

} // namespace gui2

