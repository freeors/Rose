/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/dialog.hpp"

#include "gui/widgets/integer_selector.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/visual_layout.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "base_instance.hpp"
#include "video.hpp"
#include "gettext.hpp"
#include "display.hpp"
#include "base_controller.hpp"
#include "hotkeys.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"

using namespace std::placeholders;

namespace gui2 {

static void post_context_button(gui2::tcontrol& widget, display& disp, const std::string& main, const std::string& id, size_t flags, int width, int height)
{
	const hotkey::hotkey_item& hotkey = hotkey::get_hotkey(id);
	if (!hotkey.null()) {
		widget.set_tooltip(hotkey.get_description());
	}

	connect_signal_mouse_left_click(
		widget
		, std::bind(
			&display::click_context_menu
			, &disp
			, main
			, id
			, flags));

	// widget->set_visible(gui2::twidget::INVISIBLE);

	std::stringstream file;
	file << "buttons/" << id << ".png";
	// set surface
	surface surf = image::get_image(file.str());
	if (surf) {
		widget.set_blits(tformula_blit(file.str(), null_str, null_str, "(width)", "(height)"));
	}
}

size_t tcontext_menu::decode_flag_str(const std::string& str)
{
	size_t result = 0;
	const char* cstr = str.c_str();
	if (strchr(cstr, 'h')) {
		result |= F_HIDE;
	}
	if (strchr(cstr, 'p')) {
		result |= F_PARAM;
	}
	return result;
}

void tcontext_menu::load(display& disp, const config& cfg)
{
	tpoint unit_size = report->get_unit_size();

	std::string str = cfg["main"].str();
	std::vector<std::string> main_items = utils::split(str, ',');
	VALIDATE(!main_items.empty(), "Must define main menu!");

	std::vector<std::string> vstr, main_items2;
	size_t flags;
	for (std::vector<std::string>::const_iterator it = main_items.begin(); it != main_items.end(); ++ it) {
		const std::string& item = *it;
		vstr = utils::split(item, '|');
		flags = 0;
		if (vstr.size() >= 2) {
			flags = decode_flag_str(vstr[1]);
		}
		main_items2.push_back(vstr[0]);

		// const std::string& id = vstr[0];
		gui2::tcontrol& widget = report->insert_item(vstr[0], null_str);
		post_context_button(widget, disp, main_id, vstr[0], flags, unit_size.x, unit_size.y);
		report->set_item_visible(widget, false);
	}
	menus.insert(std::make_pair(main_id, main_items2));

	std::vector<std::string> subitems;
	for (std::vector<std::string>::const_iterator it = main_items2.begin(); it != main_items2.end(); ++ it) {
		const std::string& item = *it;
		size_t pos = item.rfind("_m");
		if (item.size() <= 2 || pos != item.size() - 2) {
			continue;
		}
		const std::string sub = item.substr(0, pos);
		str = cfg[sub].str();
		std::vector<std::string> vstr2 = utils::split(str, ',');
		VALIDATE(!vstr.empty(), "must define submenu!");
		subitems.clear();
		for (std::vector<std::string>::const_iterator it2 = vstr2.begin(); it2 != vstr2.end(); ++ it2) {
			vstr = utils::split(*it2, '|');
			flags = 0;
			if (vstr.size() >= 2) {
				flags = decode_flag_str(vstr[1]);
			}
			str = sub + ":" + vstr[0];
			subitems.push_back(str);

			const std::string& id = str;
			gui2::tcontrol& widget = report->insert_item(id, null_str);
			post_context_button(widget, disp, main_id, id, flags, unit_size.x, unit_size.y);
			report->set_item_visible(widget, false);
		}
		menus.insert(std::make_pair(sub, subitems));
	}
}

void tcontext_menu::show(const display& disp, const base_controller& controller, const std::string& id) const
{
	size_t start_child = -1;
	std::string adjusted_id = id;
	if (id.empty()) {
		adjusted_id = main_id;
		start_child = 0;
	}
	if (adjusted_id.empty()) {
		return;
	}
	if (!menus.count(adjusted_id)) {
		return;
	}

	const gui2::tgrid::tchild* children = report->child_begin();
	size_t size = report->items();
	if (start_child == -1) {
		// std::string prefix = adjusted_id + "_c:";
		std::string prefix = adjusted_id + ":";
		for (size_t n = 0; n < size; n ++ ) {
			const std::string& id = children[n].widget_->id();
			if (id.find(prefix) != std::string::npos) {
				start_child = n;
				break;
			}
		}
	}
	if (start_child == -1) {
		return;
	}

	tpoint unit_size = report->get_unit_size();
	const std::vector<std::string>& items = menus.find(adjusted_id)->second;
	for (size_t n = 0; n < size; n ++) {
		gui2::tbutton* widget = dynamic_cast<gui2::tbutton*>(children[n].widget_);
		if (n >= start_child && n < start_child + items.size()) {
			const std::string& item = items[n - start_child];
			// Remove commands that can't be executed or don't belong in this type of menu
			if (controller.app_in_context_menu(item)) {
				report->set_item_visible(n, true);
				widget->set_active(controller.actived_context_menu(item));
				controller.prepare_show_menu(*widget, item, unit_size.x, unit_size.y);
			} else {
				report->set_item_visible(n, false);
			}
		} else {
			report->set_item_visible(n, false);
		}
	}
}

void tcontext_menu::hide() const
{
	report->hide_children();
}

std::pair<std::string, std::string> tcontext_menu::extract_item(const std::string& id)
{
	size_t pos = id.find(":");
	if (pos == std::string::npos) {
		return std::make_pair(id, null_str);
	}

	std::string major = id.substr(0, pos);
	std::string minor = id.substr(pos + 1);
	return std::make_pair(major, minor);
}

void tdialog::ttexture_holder::register_holder()
{
	VALIDATE(dlg_.texture_holders_.count(this) == 0, null_str);
	dlg_.texture_holders_.insert(this);
}

void tdialog::ttexture_holder::unregister_holder()
{
	std::set<ttexture_holder*>::iterator it = dlg_.texture_holders_.find(this);
	VALIDATE(it != dlg_.texture_holders_.end(), null_str);
	dlg_.texture_holders_.erase(this);
}

tdialog::tdialog(base_controller* controller)
	: video_(anim2::manager::instance->video())
	, window_(nullptr)
	, controller_(controller)
	, timer_interval_(0)
	, retval_(0)
	, restore_(true)
	, longblock_threshold_(game_config::min_longblock_threshold_s * 1000)
	, last_send_longblock_ticks_(SDL_GetTicks())
{
}

tdialog::~tdialog()
{
	VALIDATE(texture_holders_.empty(), null_str);
	if (timer_interval_ != 0) {
		// if use timer_handler, think will use wdg. send long wdg before this dilaog destructed.
		instance->will_longblock();
	}

	if (controller_) {
		VALIDATE(window_, null_str);
		delete window_;
	}
}

void tdialog::set_timer_interval(int interval)
{ 
	VALIDATE(interval >= 0, null_str);
	VALIDATE(window_ == nullptr, "call set_timer_interval must be before tdialog::show(...)");

	// const int min_interval = 30; // 30 ms
	const int max_interval = (game_config::min_longblock_threshold_s - 1) * 1000; // at least send one wdg in max_interval.
	if (interval != 0) {
		std::stringstream err;
		err << "interval must be range of [0, " << max_interval << "]";
		VALIDATE(interval >= 30 && interval <= max_interval, err.str());
	}
	timer_interval_ = interval;
}

void tdialog::base_timer_handler()
{
	const uint32_t now = SDL_GetTicks();

	if (now >= last_send_longblock_ticks_ + longblock_threshold_) {
		instance->will_longblock();
		last_send_longblock_ticks_ = now;
	}

	app_timer_handler(now);
}

bool tdialog::show(const unsigned explicit_x, const unsigned explicit_y)
{
	if (timer_interval_ != 0) {
		// why requrie timer_interval_ != 0? for example progoressbar must not call it. 
		// if use timer_handler, think will use wdg. send long wdg before this dilaog will show.
		// Better to put it in the constructor.
		instance->will_longblock();
	}

	std::vector<twindow*> connected = gui2::connectd_window();

	if (twidget::disalbe_popup_window) {
		twidget::disalbe_popup_window = false; // make sure show_message can popup new window
		utils::string_map symbols;
		symbols["reason"] = twidget::tdisable_popup_window_lock::err_msg;
		symbols["id"] = connected.empty()? "null_str": connected.back()->id();
		VALIDATE(false, vgettext2("$reason, trigger window: $id", symbols));
	}

	if (tprogress_::instance == nullptr) {
		keyboard::set_visible(false);
	}

	// hide unit tip if necessary.
	display* disp = display::get_singleton();
	if (disp) {
		disp->hide_tip();
	}

	std::string windowid = window_id();
	if (!connected.empty() && windowid != twindow::visual_layout_id) {
		twindow& window = *connected.back();
		bool require_redraw = window.popup_new_window();
		if (require_redraw) {
			absolute_draw();
		}
	}

	// here, twindow::eat_left_up maybe true.
	// 1. because down, 1)close A window. 2)create B window, this is in B window.
	// of couse, next up will discard, but can left show pass.

	{
		std::unique_ptr<twindow> window(build_window(video_, explicit_x, explicit_y));
		VALIDATE(window.get(), null_str);
		window_ = window.get();

		try {
			if (timer_interval_ != 0) {
				timer_.reset(timer_interval_, *window_, std::bind(&tdialog::base_timer_handler, this));
			}
			pre_show();

			// window->set_transition(video.getTexture(), SDL_GetTicks());

			retval_ = window->show(restore_);

			post_show();
			// this dialog maybe Post msg, and these msg maybe not execute still.
			instance->sdl_thread().clear_msg(this);

			if (instance->terminating() || retval_ == twindow::APP_TERMINATE) {
				throw CVideo::quit();
			}

		} catch (twindow::tlayout_exception& e) {
			if (window->id() != twindow::visual_layout_id) {
				gui2::tvisual_layout dlg(e.target, e.reason);
				dlg.show();
				throw CVideo::quit();

			} else {
				throw twml_exception(e.reason);
			}
		} catch (twml_exception&) {
			// exception usually occur in twindow::show, As normal end logic, it should call post_show()

			// other exception handled by caller.
			post_show();
			// this dialog maybe Post msg, and these msg maybe not execute still.
			instance->sdl_thread().clear_msg(this);
			throw;
		}
		window_ = nullptr;
	}
	connected = gui2::connectd_window();
	if (!connected.empty()) {
		twindow& window = *connected.back();
		window.undraw_float_widgets();
		if (window.is_scene()) {
			display::get_singleton()->invalidate_all();
		}
	}

	if (disp && !disp->in_theme()) {
		image::flush_cache();
	}

	if (tprogress_::instance == nullptr) {
		keyboard::set_visible(false);
	}
	return retval_ == twindow::OK;
}

void tdialog::scene_show()
{
	twindow* window = build_window(video_, nposm, nposm);
	// below code mybe exception, destruct can relase, evalue async_window at first.
	window_ = window;

	scene_pre_show();

	window->set_suspend_drawing(false);

	try {
		window->layout();
		post_layout();

	} catch (twindow::tlayout_exception& e) {
		VALIDATE(window->id() != twindow::visual_layout_id, null_str);

		gui2::tvisual_layout dlg(e.target, e.reason);
		dlg.show();
		throw CVideo::quit();
	}
}

twindow* tdialog::build_window(CVideo& video, const unsigned explicit_x, const unsigned explicit_y)
{
	return build(*this, video, window_id(), explicit_x, explicit_y);
}

void tdialog::scene_pre_show()
{
	hotkey::insert_hotkey(HOTKEY_ZOOM_IN, "zoomin", _("Zoom In"));
	hotkey::insert_hotkey(HOTKEY_ZOOM_OUT, "zoomout", _("Zoom Out"));
	hotkey::insert_hotkey(HOTKEY_ZOOM_DEFAULT, "zoomdefault", _("Default Zoom"));
	hotkey::insert_hotkey(HOTKEY_SCREENSHOT, "screenshop", _("Screenshot"));
	hotkey::insert_hotkey(HOTKEY_MAP_SCREENSHOT, "mapscreenshop", _("Map Screenshot"));
	hotkey::insert_hotkey(HOTKEY_CHAT, "chat", _("Chat"));
	hotkey::insert_hotkey(HOTKEY_UNDO, "undo", _("Undo"));
	hotkey::insert_hotkey(HOTKEY_REDO, "redo", _("Redo"));
	hotkey::insert_hotkey(HOTKEY_COPY, "copy", _("Copy"));
	hotkey::insert_hotkey(HOTKEY_CUT, "cut", _("Cut"));
	hotkey::insert_hotkey(HOTKEY_PASTE, "paste", _("Paste"));
	hotkey::insert_hotkey(HOTKEY_HELP, "help", _("Help"));
	hotkey::insert_hotkey(HOTKEY_SYSTEM, "system", _("System"));

	tcontrol* main_map = find_widget<tcontrol>(window_, "_main_map", false, true);
	main_map->connect_signal<event::MOUSE_LEAVE>(std::bind(
				&tdialog::signal_handler_mouse_leave, this, _5, _6));

	pre_show();

	utils::string_map symbols;
	std::string no_widget_msgid = "Cannot find report widget: $id.";
	std::string not_fix_msgid = "Report widget($id) must use fix rect.";
		
	for (std::map<int, const std::string>::const_iterator it = reports_.begin(); it != reports_.end(); ++ it) {
		twidget* widget = get_object(it->second);
		symbols["id"] = it->second;
		if (!widget) {
			// some widget maybe remove. for example undo in tower mode.
			// VALIDATE(false, vgettext2("Cannot find report widget: $id.", symbols));
			continue;
		}
	}
}

void tdialog::resize_screen()
{
	VALIDATE(game_config::os == os_windows, null_str);
	app_resize_screen();
}

void tdialog::destruct_widget(const twidget* widget)
{
	for (std::map<std::string, twidget*>::iterator it = cached_widgets_.begin(); it != cached_widgets_.end(); ++ it) {
		if (widget == it->second) {
			cached_widgets_.erase(it);
			return;
		}
	}
}

twidget* tdialog::get_object(const std::string& id) const
{
	std::map<std::string, twidget*>::const_iterator it = cached_widgets_.find(id);
	if (it != cached_widgets_.end()) {
		return it->second;
	}
	twidget* widget = find_widget<twidget>(window_, id, false, false);
	cached_widgets_.insert(std::make_pair(id, widget));
	return widget;
}

void tdialog::set_object_active(const std::string& id, bool active) const
{
	tcontrol* widget = dynamic_cast<tcontrol*>(get_object(id));
	if (widget) {
		widget->set_active(active);
	}
}

void tdialog::set_object_visible(const std::string& id, const twidget::tvisible visible) const
{
	twidget* widget = get_object(id);
	if (widget) {
		widget->set_visible(visible);
	}
}

void tdialog::post_layout()
{
	VALIDATE(controller_, null_str);

	utils::string_map symbols;

	const config& context_menus_cfg = *window_->context_menus();
	BOOST_FOREACH (const config &cfg, context_menus_cfg.child_range("context_menu")) {
		const std::string id = cfg["report"].str();
		if (id.empty()) {
			continue;
		}
		symbols["id"] = ht::generate_format(id, color_to_uint32(font::GRAY_COLOR));
		treport* report = dynamic_cast<treport*>(get_object(id));
		VALIDATE(report, vgettext2("Defined $id conext menu, but not define same id report widget!", symbols));

		context_menus_.push_back(tcontext_menu(id));
		tcontext_menu& m = context_menus_.back();
		m.report = report;
		m.load(controller_->get_display(), cfg);
	}
}


void tdialog::click_generic_handler(tcontrol& widget, const std::string& sparam)
{
	const hotkey::hotkey_item& hotkey = hotkey::get_hotkey(widget.id());
	if (!hotkey.null()) {
		widget.set_tooltip(hotkey.get_description());
	}

	connect_signal_mouse_left_click(
		widget
		, std::bind(
			&base_controller::execute_command
			, controller_
			, hotkey.get_id()
			, sparam));
}

twidget* tdialog::get_report(int num) const
{
	std::map<int, const std::string>::const_iterator it = reports_.find(num);
	if (it == reports_.end()) {
		return NULL;
	}
	return get_object(it->second);
}

void tdialog::set_report_label(int num, const std::string& label) const
{
	tcontrol* widget = dynamic_cast<tcontrol*>(get_report(num));
	if (widget) {
		widget->set_label(label);
	}
}

void tdialog::set_report_blits(int num, const std::vector<tformula_blit>& blits) const
{
	tcontrol* widget = dynamic_cast<tcontrol*>(get_report(num));
	if (widget) {
		widget->set_blits(blits);
	}
}

tcontext_menu* tdialog::context_menu(const std::string& id)
{
	for (std::vector<tcontext_menu>::iterator it = context_menus_.begin(); it != context_menus_.end(); ++ it) {
		return &*it;
	}
	return NULL;
}

void tdialog::signal_handler_mouse_leave(const tpoint& coordinate, const tpoint& coordinate2)
{
	VALIDATE(controller_, null_str);
	controller_->handle_mouse_leave_main_map(coordinate.x, coordinate.y, is_up_result_leave(coordinate2));
}

void tdialog::OnMessage(rtc::Message* msg)
{
	if (msg->message_id == POST_MSG_DRAG_MOUSE_LEAVE) {
		tmsg_data_drag_mouse_leave* pdata = static_cast<tmsg_data_drag_mouse_leave*>(msg->pdata);
		pdata->did_mouse_leave(pdata->x, pdata->y, pdata->is_up_result);
		delete pdata;

	} else if (msg->message_id == POST_MSG_PULLREFRESH) {
		tmsg_data_pullrefresh* pdata = static_cast<tmsg_data_pullrefresh*>(msg->pdata);
		tscroll_container* widget = static_cast<tscroll_container*>(pdata->widget);
		widget->pullrefresh_refresh(pdata->rect);
		delete pdata;

	} else if (msg->message_id == POST_MSG_POPUP_EDITBOX) {
		if (tprogress_::instance != nullptr) {
			// POST_MSG_POPUP_EDITBOX use the "Post + app_OnMessage" mechanism, 
			// the Post and app_OnMessage may have run tprogress_:instance, in which case the pop-up should be ignored.
			return;
		}
		tmsg_data_popup_editbox* pdata = static_cast<tmsg_data_popup_editbox*>(msg->pdata);
		ttext_box* widget = static_cast<ttext_box*>(&pdata->widget);
		tedit_box_param& param = widget->edit_box_param();
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 4);
		}
		widget->set_label(param.result);
		delete pdata;

	} else {
		app_OnMessage(msg);
	}
}

bool show_confirm_delete(const std::vector<std::string>& files, bool fs)
{
	std::string msg;
	utils::string_map symbols;
	if (files.size() == 1) {
		symbols["file"] = extract_file(files.at(0));
		msg = vgettext2("Do you want to delete: $file", symbols);

	} else {
		symbols["count"] = str_cast(files.size());
		msg = vgettext2("Do you want to delete these $count items", symbols);
	}
	int ret;
	if (fs) {
		ret = gui2::show_messagefs(msg, _("Delete"));
	} else {
		ret = gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons);
	}
	return ret == gui2::twindow::OK;
}

} // namespace gui2


/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 1
 *
 * {{Autogenerated}}
 *
 * = Window definition =
 *
 * The window definition define how the windows shown in the dialog look.
 */

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 */

