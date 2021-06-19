#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/anim_particular.hpp"
#include "gui/dialogs/anim.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, anim_particular)

tanim_particular::tanim_particular(tanim2& anim, const int at)
	: anim_(anim)
	, at_(at)
{
}

void tanim_particular::pre_show()
{
	find_widget<tlabel>(window_, "title", false).set_label(_("anim^Edit particular"));

	if (at_ == 0) {
		anim_.unit_anim_.update_to_ui_particular_edit(*window_, "frame");
	} else {
		std::map<std::string, tparticular>::iterator it = anim_.sub_anims_.begin();
		std::advance(it, at_ - 1);
		it->second.update_to_ui_particular_edit(*window_, it->first);

		ttext_box* text_box = find_widget<ttext_box>(window_, "prefix", false, true);
		text_box->set_did_text_changed(std::bind(&tanim_particular::did_prefix_changed, this, _1));
	}


	connect_signal_mouse_left_click(
			  find_widget<tcontrol>(window_, "save", false)
			, std::bind(
				&tanim_particular::save
				, this
				, std::ref(*window_)));
}

void tanim_particular::did_prefix_changed(ttext_box& widget)
{
	VALIDATE(at_ >= 1, null_str);
	const std::string& label = widget.label();

	static std::set<std::string> reserved_particular_frame_tag;
	if (reserved_particular_frame_tag.empty()) {
		const std::string default_particular_frame_tag = "__new_frame";
		reserved_particular_frame_tag.insert(default_particular_frame_tag);
		reserved_particular_frame_tag.insert("_add_text");
	}

	std::string str = utils::lowercase(label);
	const int min_id_len = 1;
	const int max_id_len = 32;
	if (!utils::isvalid_id(str, true, min_id_len, max_id_len)) {
		find_widget<tlabel>(window_, "status", false).set_label(_("Invalid string"));
		return;
	}
	if (reserved_particular_frame_tag.find(str) != reserved_particular_frame_tag.end()) {
		find_widget<tlabel>(window_, "status", false).set_label(_("Reserved string"));
		return;
	}

	// cannot be exist same.
	int n = at_ - 1;
	for (std::map<std::string, tparticular>::iterator it = anim_.sub_anims_.begin(); it != anim_.sub_anims_.end(); ++ it) {
		int index = std::distance(anim_.sub_anims_.begin(), it);
		if (index == n) {
			continue;
		}
		const std::string prefix = tparticular::prefix_from_tag(it->first);
		if (str == prefix) {
			find_widget<tlabel>(window_, "status", false).set_label(_("Other has holden it"));
			return;
		}
	}
	find_widget<tlabel>(window_, "status", false).set_label(null_str);

	std::stringstream ss;
	ss.str("");
	ss << str << "_frame";
	find_widget<tlabel>(window_, "tag", false).set_label(ss.str());
}

void tanim_particular::save(twindow& window)
{
	anim_.from_ui_particular_edit(window, at_);
	window.set_retval(twindow::OK);
}

} // namespace gui2

