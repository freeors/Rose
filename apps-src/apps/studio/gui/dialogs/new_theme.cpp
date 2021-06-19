#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/new_theme.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "font.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_DIALOG(studio, new_theme)

tnew_theme::tnew_theme(const tapp_copier& app, const config& core_cfg, tsave_theme& save_theme, tremove_theme& remove_theme)
	: app_(app)
	, core_cfg_(core_cfg)
	, save_theme_(save_theme)
	, remove_theme_(remove_theme)
	, current_at_(nposm)
{
	BOOST_FOREACH(const config& cfg, core_cfg_.child_range("theme")) {
		if (cfg["app"].str() == "rose") {
			themes_.push_back(theme::tfields());
			themes_.back().from_cfg(cfg);
			cfg_themes_.push_back(tfields2(themes_.back(), true));
		}
	}
	BOOST_FOREACH(const config& cfg, core_cfg_.child_range("theme")) {
		if (cfg["app"].str() == app.app) {
			themes_.push_back(theme::tfields());
			themes_.back().from_cfg(cfg);
			cfg_themes_.push_back(tfields2(themes_.back(), true));
		}
	}
}

void tnew_theme::pre_show()
{
	window_->set_canvas_variable("border", variant("default_border"));

	std::stringstream ss;
	utils::string_map symbols;

	symbols["app"] = app_.app;

	ss.str("");
	ss << vgettext2("$app|'s theme", symbols); 
	find_widget<tlabel>(window_, "title", false).set_label(ss.str());

	navigate_ = find_widget<treport>(window_, "navigate", false, true);
	navigate_->set_did_item_pre_change(std::bind(&tnew_theme::did_item_pre_change_report, this, _1, _2, _3));
	navigate_->set_did_item_changed(std::bind(&tnew_theme::did_item_changed_report, this, _1, _2));
	// navigate->set_boddy(find_widget<twidget>(float_widget_page, "navigate_panel", false, true));

	int at = 0;
	for (std::vector<theme::tfields>::const_iterator it = themes_.begin(); it != themes_.end(); ++ it, at ++) {
		const theme::tfields& fields = *it;
		tcontrol& widget = navigate_->insert_item(fields.id, form_tab_label(at));
		widget.set_cookie(at);
	}
	if (!themes_.empty()) {
		navigate_->select_item(0);
	}

	tbutton* button = find_widget<tbutton>(window_, "new", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&tnew_theme::new_theme
			, this
			, std::ref(*window_)));

	button = find_widget<tbutton>(window_, "erase", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&tnew_theme::erase_theme
			, this
			, std::ref(*window_)));
	if (themes_.size() < 2) {
		button->set_active(false);
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "save", false)
			, std::bind(
				&tnew_theme::save
				, this
				, std::ref(*window_)));
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "close", false)
			, std::bind(
				&tnew_theme::close
				, this
				, std::ref(*window_)
				, _3, _4));

}

void tnew_theme::post_show()
{
}

void tnew_theme::reload_theme(twindow& window, const int desire_at, const bool initial)
{
	VALIDATE(desire_at >= 0 && desire_at < (int)themes_.size(), null_str);
	tswitch_lock lock(*this);
	const theme::tfields& fields = themes_[desire_at];

	ttext_box* text_box = find_widget<ttext_box>(&window, "id", false, true);
	text_box->set_label(fields.id);
	if (initial) {
		text_box->set_did_text_changed(std::bind(&tnew_theme::did_id_changed, this, std::ref(window), _1));
		text_box->set_placeholder("new id");
		window.keyboard_capture(text_box);
	}
	text_box->set_active(desire_at != 0);

	std::string prefix;
	for (int tpl = 0; tpl < theme::color_tpls; tpl ++) {
		prefix.clear();
		if (tpl == theme::inverse_tpl) {
			prefix = "inverse_";
		} else if (tpl == theme::title_tpl) {
			prefix = "title_";
		}

		text_box = find_widget<ttext_box>(&window, prefix + "normal_color", false, true);
		text_box->set_label(encode_color(fields.text_color_tpls[tpl].normal));
		if (initial) {
			text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, tpl, theme::normal));
			text_box->set_placeholder("alpha, red, green, blue");
		}
		text_box->set_active(desire_at != 0);

		text_box = find_widget<ttext_box>(&window, prefix + "disable_color", false, true);
		text_box->set_label(encode_color(fields.text_color_tpls[tpl].disable));
		if (initial) {
			text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, tpl, theme::disable));
			text_box->set_placeholder("alpha, red, green, blue");
		}
		text_box->set_active(desire_at != 0);

		text_box = find_widget<ttext_box>(&window, prefix + "focus_color", false, true);
		text_box->set_label(encode_color(fields.text_color_tpls[tpl].focus));
		if (initial) {
			text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, tpl, theme::focus));
			text_box->set_placeholder("alpha, red, green, blue");
		}
		text_box->set_active(desire_at != 0);

		text_box = find_widget<ttext_box>(&window, prefix + "placeholder_color", false, true);
		text_box->set_label(encode_color(fields.text_color_tpls[tpl].placeholder));
		if (initial) {
			text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, tpl, theme::placeholder));
			text_box->set_placeholder("alpha, red, green, blue");
		}
		text_box->set_active(desire_at != 0);
	}

	// other color
	text_box = find_widget<ttext_box>(&window, "item_focus_color", false, true);
	text_box->set_label(encode_color(fields.item_focus_color));
	if (initial) {
		text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, theme::default_tpl, theme::item_focus));
		text_box->set_placeholder("alpha, red, green, blue");
	}
	text_box->set_active(desire_at != 0);

	text_box = find_widget<ttext_box>(&window, "item_highlight_color", false, true);
	text_box->set_label(encode_color(fields.item_highlight_color));
	if (initial) {
		text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, theme::default_tpl, theme::item_highlight));
		text_box->set_placeholder("alpha, red, green, blue");
	}
	text_box->set_active(desire_at != 0);

	text_box = find_widget<ttext_box>(&window, "menu_focus_color", false, true);
	text_box->set_label(encode_color(fields.menu_focus_color));
	if (initial) {
		text_box->set_did_text_changed(std::bind(&tnew_theme::did_color_changed, this, std::ref(window), _1, theme::default_tpl, theme::menu_focus));
		text_box->set_placeholder("alpha, red, green, blue");
	}
	text_box->set_active(desire_at != 0);

	find_widget<tbutton>(&window, "new", false).set_active(check_total(fields, desire_at));
	find_widget<tbutton>(&window, "erase", false, true)->set_active(desire_at > 0);
	find_widget<tbutton>(&window, "save", false).set_active(themes_[desire_at] != cfg_themes_[desire_at].fields);
}

bool tnew_theme::check_id(const std::string& id, const int current_at) const
{
	int size = id.size();
	bool active = utils::isvalid_id(id, true, 3, 24);
	if (active) {
		active = id.find(' ') == std::string::npos;
	}
	if (active) {
		active = id.find('_') == std::string::npos;
	}
	if (active) {
		int at = 0;
		for (std::vector<theme::tfields>::const_iterator it = themes_.begin(); it != themes_.end(); ++ it, at ++) {
			if (current_at == at) {
				continue;
			}
			if (id == it->id) {
				active = false;
				break;
			}
		}
	}
	return active;
}

uint32_t tnew_theme::check_color(const std::string& color_str) const
{
	std::vector<std::string> fields = utils::split(color_str);
	if (fields.size() != 4) {
		return 0;
	}

	int val;
	uint32_t result = 0;
	for (int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		
		val = lexical_cast_default<int>(fields[i]);
		if (val < 0 || val > 255) {
			return 0;
		}
		result |= lexical_cast_default<int>(fields[i]);
	}
	if (!(result & 0xff000000)) {
		// avoid to confuse with special color.
		return 0;
	}
	return result;
}

bool tnew_theme::check_total(const theme::tfields& result, const int at) const
{
	if (!check_id(result.id, at)) {
		return false;
	}

	for (int tpl = 0; tpl < theme::color_tpls; tpl ++) {
		if (!result.text_color_tpls[tpl].normal) {
			return false;
		}
		if (!result.text_color_tpls[tpl].disable) {
			return false;
		}
		if (!result.text_color_tpls[tpl].focus) {
			return false;
		}
		if (!result.text_color_tpls[tpl].placeholder) {
			return false;
		}
	}
	if (!result.item_focus_color) {
		return false;
	}
	if (!result.item_highlight_color) {
		return false;
	}
	if (!result.menu_focus_color) {
		return false;
	}
	return true;
}

void tnew_theme::did_id_changed(twindow& window, ttext_box& widget)
{
	if (switching_) {
		return;
	}
	theme::tfields& result = themes_[current_at_];

	result.id = utils::lowercase(widget.label());

	bool valid = check_total(result, current_at_);

	find_widget<tbutton>(&window, "new", false).set_active(valid);
	find_widget<tbutton>(&window, "save", false).set_active(valid && themes_[current_at_] != cfg_themes_[current_at_].fields);
}

void tnew_theme::did_color_changed(twindow& window, ttext_box& widget, const int tpl, const int at)
{
	if (switching_) {
		return;
	}

	const std::string color_str = utils::lowercase(widget.label());
	const uint32_t color = check_color(color_str);
	if (!color) {
		find_widget<tbutton>(&window, "new", false).set_active(false);
		find_widget<tbutton>(&window, "save", false).set_active(false);
		return;
	}

	theme::tfields& result = themes_[current_at_];
	if (at < theme::tpl_colors) {
		if (at == theme::normal) {
			result.text_color_tpls[tpl].normal = color;
		} else if (at == theme::disable) {
			result.text_color_tpls[tpl].disable = color;
		} else if (at == theme::focus) {
			result.text_color_tpls[tpl].focus = color;
		} else {
			result.text_color_tpls[tpl].placeholder = color;
		}
		
	} else {
		VALIDATE(tpl == theme::default_tpl, null_str);
		if (at == theme::item_focus) {
			result.item_focus_color = color;
		} else if (at == theme::item_highlight) {
			result.item_highlight_color = color;
		} else {
			VALIDATE(at == theme::menu_focus, null_str);
			result.menu_focus_color = color;
		}
	}

	bool valid = check_total(result, current_at_);

	find_widget<tbutton>(&window, "new", false).set_active(valid);
	find_widget<tbutton>(&window, "save", false).set_active(valid && themes_[current_at_] != cfg_themes_[current_at_].fields);
}

bool tnew_theme::did_item_pre_change_report(treport& report, ttoggle_button& from, ttoggle_button& widget)
{
	const int previous_at = (int)from.cookie();

	const theme::tfields& result = themes_[previous_at];
	bool ok = check_total(result, previous_at);
	if (!ok) {
		std::stringstream err;
		utils::string_map symbols;

		symbols["id"] = app_.app;
		err << vgettext2("There is invalid value in current theme.", symbols);
		gui2::show_message("", err.str());

	}
	from.set_label(form_tab_label(previous_at));
	return ok;
}

void tnew_theme::did_item_changed_report(treport& report, ttoggle_button& widget)
{
	const int desire_at = (int)widget.cookie();

	reload_theme(*window_, desire_at, current_at_ == nposm);
	current_at_ = desire_at;
}

std::string tnew_theme::form_tab_label(const int at) const
{
	const theme::tfields& result = themes_[at];

	std::stringstream ss;
	ss << result.id;
	if (themes_[at] != cfg_themes_[at].fields) {
		ss << ht::generate_img("misc/dot.png");
	}
	return ss.str();
}

void tnew_theme::new_theme(twindow& window)
{
	const int at = themes_.size();

	const std::string default_id = "_id";
	theme::tfields fields = themes_[0]; // copy from default
	fields.app = app_.app;
	cfg_themes_.push_back(tfields2(fields, false)); // fake config's themes.
	
	fields.id.clear();
	themes_.push_back(fields);

	tcontrol& widget = navigate_->insert_item(default_id, form_tab_label(at));
	widget.set_cookie(at);
	navigate_->select_item(at);
}

void tnew_theme::erase_theme(twindow& window)
{
	VALIDATE(current_at_ > 0, null_str);

	std::string msg = _("Do you really want to erase theme?");
	int res = gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	const theme::tfields& fields = themes_[current_at_];
	if (cfg_themes_[current_at_].exist_cfg) {
		remove_theme_.set_theme(fields);
		remove_theme_.handle();
	}

	std::vector<theme::tfields>::iterator it = themes_.begin();
	std::advance(it, current_at_);
	themes_.erase(it);

	std::vector<tfields2>::iterator it2 = cfg_themes_.begin();
	std::advance(it2, current_at_);
	cfg_themes_.erase(it2);

	for (int at = current_at_; at < (int)themes_.size(); at ++) {
		tcontrol& widget = navigate_->item(at + 1);
		widget.set_cookie(at);
	}
	navigate_->erase_item(current_at_);

	if (current_at_ == (int)themes_.size()) {
		current_at_ --;
	}
}

void tnew_theme::save(twindow& window)
{
	VALIDATE(current_at_ > 0, null_str);

	const theme::tfields& fields = themes_[current_at_];
	VALIDATE(check_total(fields, current_at_) && fields != cfg_themes_[current_at_].fields, null_str);

	save_theme_.set_theme(fields);
	save_theme_.handle();

	cfg_themes_[current_at_].fields = fields;
	cfg_themes_[current_at_].exist_cfg = true;

	navigate_->item(current_at_).set_label(form_tab_label(current_at_));
	find_widget<tbutton>(&window, "save", false).set_active(false);
}

void tnew_theme::close(twindow& window, bool& handled, bool& halt)
{
	bool same = true;
	for (int at = 0; at < (int)themes_.size(); ++ at) {
		const theme::tfields& fields = themes_[at];
		if (fields != cfg_themes_[at].fields) {
			same = false;
			break;
		}
	}

	if (!same) {
		std::string msg = _("Do you really want to close?");

		utils::string_map symbols;
		// symbols["window"] = preview_? _("Theme"): _("Dialog");
		const std::string str = vgettext2("Changes in the themes since the last save will be lost.", symbols);
		msg += "\n\n" + ht::generate_format(str, color_to_uint32(font::BAD_COLOR));

		int res = gui2::show_message2(_("Close"), msg, gui2::tmessage::yes_no_buttons);
		if (res != gui2::twindow::OK) {
			handled = true;
			halt = true;
			return;
		}
	}

	window.set_retval(twindow::OK);
}

} // namespace gui2

