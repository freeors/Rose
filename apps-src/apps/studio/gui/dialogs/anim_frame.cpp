#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/anim_frame.hpp"
#include "gui/dialogs/anim.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, anim_frame)

tanim_frame::tanim_frame(tanim2& anim, const int at)
	: anim_(anim)
	, at_(at)
{
}

void tanim_frame::pre_show()
{
	find_widget<tlabel>(window_, "title", false).set_label(_("anim^Edit frame"));

	variables_to_lv(*window_, anim_.id_);
	std::pair<tparticular*, int>& cookie = anim_.frame_cookie_[at_];
	cookie.first->update_to_ui_frame_edit(*window_, cookie.second);

	tstack* image_halo = find_widget<tstack>(window_, "image_halo", false, true);
	image_halo->set_radio_layer(cookie.first->use_image(cookie.second)? image_layer: halo_layer);

	ttoggle_button* use_image = find_widget<ttoggle_button>(window_, "use_image", false, true);
	use_image->set_did_state_changed(std::bind(&tanim_frame::did_use_image_changed, this, _1));
	use_image->set_value(cookie.first->use_image(cookie.second));

	connect_signal_mouse_left_click(
			  find_widget<tcontrol>(window_, "save", false)
			, std::bind(
				&tanim_frame::save
				, this
				, std::ref(*window_)));
}

void tanim_frame::variables_to_lv(twindow& window, const std::string& id)
{
	tlistbox* variables = find_widget<tlistbox>(&window, "variables", false, true);

	int index = 0;
	const tanim_type2& type = tanim2::anim_type(id);

	std::map<std::string, std::string> data;
	for (std::map<std::string, std::string>::const_iterator it = type.variables_.begin(); it != type.variables_.end(); ++ it) {
		data["id"] = it->first;
		data["remark"] = it->second;
		variables->insert_row(data);
	}
}

void tanim_frame::did_use_image_changed(ttoggle_button& widget)
{
	tstack* image_halo = find_widget<tstack>(window_, "image_halo", false, true);
	image_halo->set_radio_layer(widget.get_value()? image_layer: halo_layer);
}

void  tanim_frame::save(twindow& window)
{
	ttext_box* text = find_widget<ttext_box>(&window, "text_color", false, true);
	if (!utils::check_color(text->label())) {
		return;
	}

	std::pair<tparticular*, int>& cookie = anim_.frame_cookie_[at_];
	anim_.from_ui_frame_edit(window, *cookie.first, cookie.second, find_widget<ttoggle_button>(&window, "use_image", false).get_value());

	window.set_retval(twindow::OK);
}

} // namespace gui2

