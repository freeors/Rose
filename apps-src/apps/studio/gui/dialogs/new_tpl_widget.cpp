#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/new_tpl_widget.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, new_tpl_widget)

tnew_tpl_widget::tnew_tpl_widget(const std::string& app, const unit& u, const std::set<const gui2::ttpl_widget*>& used_tpl_widget, const std::string& default_id)
	: u_(u)
	, used_tpl_widget_(used_tpl_widget)
	, nested_(!used_tpl_widget.empty())
	, default_id_(default_id)
	, current_option_(nposm)
{
	result_.app = app;

	// apps_ = apps_in_res();
	apps_.insert(null_str);
}

void tnew_tpl_widget::pre_show()
{
	find_widget<tlabel>(window_, "title", false).set_label(_("Create or modify template widget"));

	treport* options = find_widget<treport>(window_, "options", false, true);
	tcontrol* item = &options->insert_item(null_str, _("Landscape"));
	item->set_cookie(option_landscape);
	item = &options->insert_item(null_str, _("Portrait"));
	item->set_cookie(option_portrait);
	options->set_did_item_changed(std::bind(&tnew_tpl_widget::did_option_changed, this, _2));
	options->select_item(0);

	std::stringstream ss;
	ss.str("");
	if (used_tpl_widget_.empty()) {
		ss << _("Specified template doesn't contain template widgets");
	} else {
		ss << _("Specified template contains other template widgets:");
		for (std::set<const gui2::ttpl_widget*>::const_iterator it = used_tpl_widget_.begin(); it != used_tpl_widget_.end(); ++ it) {
			const ttpl_widget& tpl_widget = **it;
			ss << " " << utils::generate_app_prefix_id(tpl_widget.app, tpl_widget.id);
		}
	}
	find_widget<tlabel>(window_, "remark", false, true)->set_label(ss.str());

	ttext_box* text_box = find_widget<ttext_box>(window_, "id", false, true);
	text_box->set_placeholder(_("id"));
	text_box->set_did_text_changed(std::bind(&tnew_tpl_widget::did_id_changed, this, std::ref(*window_), _1));
	
	treport* report = find_widget<treport>(window_, "apps", false, true);
	for (std::set<std::string>::const_iterator it = apps_.begin(); it != apps_.end(); ++ it) {
		std::string app = *it;
		if (app.empty()) {
			app = "rose";
		}
		report->insert_item(null_str, app);
	}
	report->set_did_item_changed(std::bind(&tnew_tpl_widget::did_app_changed, this, _2));
	report->select_item(0);

	text_box->set_label(default_id_);
}

void tnew_tpl_widget::post_show()
{
	treport* options = find_widget<treport>(window_, "options", false, true);
	result_.option = options->cursel()->cookie();
	result_.id = find_widget<ttext_box>(window_, "id", false).label();
	result_.file = generate_tpl_widget_cfg(result_.app, result_.id, nested_);
}

std::string tnew_tpl_widget::generate_tpl_widget_cfg(const std::string& app, const std::string& id, bool nested)
{
	// when new_tpl_widget, id maybe empty.
	VALIDATE(!app.empty(), null_str);

	std::stringstream ss;
	ss << game_config::path + "/data/gui/";
	if (app != "rose") {
		ss << game_config::generate_app_dir(app);
	} else {
		ss << "default";
	}
	ss << "/tpl_widget/";
	if (!nested) {
		ss << "_";
	}
	ss << id << ".cfg";

	return ss.str();
}

void tnew_tpl_widget::did_option_changed(ttoggle_button& widget)
{
	std::string options_desc;
	const int cookie = widget.cookie();
	if (cookie == option_landscape) {
		options_desc = _("Use to single orientation or double orientation's landscape");

	} else if (cookie == option_portrait) {
		VALIDATE(cookie == option_portrait, null_str);
		options_desc = _("Use to double orientation's portrait. (It requires the template widget must already exist landscape)");
	}
	current_option_ = cookie;
	find_widget<tlabel>(window_, "options_desc", false).set_label(options_desc);
	did_id_changed(*window_, find_widget<ttext_box>(window_, "id", false));
}

void tnew_tpl_widget::did_app_changed(ttoggle_button& widget)
{
	std::set<std::string>::iterator it = apps_.begin();
	std::advance(it, widget.at());

	result_.app = it->empty()? "rose": *it;
	refresh_file_tip(*window_, find_widget<ttext_box>(window_, "id", false, true)->label());
}

void tnew_tpl_widget::refresh_file_tip(twindow& window, const std::string& id) const
{
	std::string file = generate_tpl_widget_cfg(result_.app, id, nested_);
	tlabel& file_desc = find_widget<tlabel>(&window, "file_desc", false);
	file_desc.set_label(file);
}

void tnew_tpl_widget::did_id_changed(twindow& window, ttext_box& widget)
{
	const std::string id = widget.label();
	int size = id.size();
	bool active = utils::isvalid_id(id, true, 3, 24);
	if (active) {
		active = id.find(' ') == std::string::npos;
	}

	// must not registed window.
	const std::map<std::string, ttpl_widget>& tpl_widgets = gui.tpl_widgets;

	bool tpl_id_existed = false;
	const std::string key = utils::generate_app_prefix_id(result_.app, id);
	for (std::map<std::string, ttpl_widget>::const_iterator it = tpl_widgets.begin(); it != tpl_widgets.end(); ++ it) {
		if (it->first == key) {
			tpl_id_existed = true;
			break;
		}
	}
	if (current_option_ == option_landscape) {
		if (tpl_id_existed) {
			// active = false;
		}
	} else {
		if (!tpl_id_existed) {
			active = false;
		}
	}


	refresh_file_tip(window, id);
	std::string label = _("Modify");
	if (current_option_ == option_landscape && !tpl_id_existed) {
		label = _("Create");
	}
	find_widget<tbutton>(&window, "ok", false).set_label(label);
	find_widget<tbutton>(&window, "ok", false).set_active(active);
}

} // namespace gui2

