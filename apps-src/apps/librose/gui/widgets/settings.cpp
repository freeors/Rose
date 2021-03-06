/* $Id: settings.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Implementation of settings.hpp.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/settings.hpp"

#include "config_cache.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/keyboard.hpp"
#include "serialization/parser.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "xwml.hpp"
#include "font.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

namespace settings {
	unsigned screen_width = 0;
	unsigned screen_height = 0;
	unsigned keyboard_height = 0;

	unsigned double_click_time = 0;
	int longpress_time = 0;
	int clear_click_threshold = 0;

	std::string sound_button_click = "";
	std::string sound_toggle_button_click = "";
	std::string sound_toggle_panel_click = "";
	std::string sound_slider_adjust = "";

	std::string horizontal_scrollbar_id;
	std::string vertical_scrollbar_id;
	std::string tooltip_id;
	std::string drag_id;
	std::string magnifier_id;
	std::string track_id;
	std::string button_id;
	std::string edit_select_all_id;
	std::string edit_select_id;
	std::string edit_copy_id;
	std::string edit_paste_id;

	std::map<std::string, tbuilder_widget_ptr> portraits;

	bool actived = false;
} // namespace settings

/**
 * Returns the list of registered windows.
 *
 * The function can be used the look for registered windows or to add them.
 */
static std::vector<std::string>& registered_window_types()
{
	static std::vector<std::string> result;
	return result;
}

typedef std::map<
		  std::string
		, std::function<void(
			  tgui_definition&
			  , const std::string&
			  , const config&
			  , const char *key)> > tregistered_widget_type;

static tregistered_widget_type& registred_widget_type()
{
	static tregistered_widget_type result;
	return result;
}

const std::string& tgui_definition::read(const config& cfg)
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 1
 *
 * {{Autogenerated}}
 *
 * = GUI =
 *
 * The gui class contains the definitions of all widgets and windows used in
 * the game. This can be seen as a skin and it allows the user to define the
 * visual aspect of the various items. The visual aspect can be determined
 * depending on the size of the game window.
 *
 * Widgets have a definition and an instance, the definition contains the
 * general info/looks of a widget and the instance the actual looks. Eg the
 * where the button text is placed is the same for every button, but the
 * text of every button might differ.
 *
 * The default gui has the id ''default'' and must exist, in the default gui
 * there must a definition of every widget with the id ''default'' and every
 * window needs to be defined. If the definition of a widget with a certain
 * id doesn't exist it will fall back to default in the current gui, if it's
 * not defined there either it will fall back to the default widget in the
 * default theme. That way it's possible to slowly create your own gui and
 * test it.
 *
 * @begin{parent}{name="/"}
 * @begin{tag}{name="gui"}{min="0"}{max="1"}
 * The gui has the following data:
 * @begin{table}{config}
 *     id & string & &                  Unique id for this gui (theme). $
 *     description & t_string & &       Unique translatable name for this gui. $
 *
 *     widget_definitions & section & & The definitions of all
 *                                   [[#widget_list|widgets]]. $
 *     window & section & &             The definitions of all
 *                                   [[#window_list|windows]]. $
 *     settings & section & &           The settings for the gui. $
 * @end{table}
 *
 * <span id="widget_list"></span>List of available widgets:
 * @begin{table}{widget_overview}
 * Button &                       @macro = button_description $
 * Image &                        @macro = image_description $
 * Horizontal_listbox &           @macro = horizontal_listbox_description $
 * Horizontal_scrollbar &         @macro = horizontal_scrollbar_description $
 * Label &                        @macro = label_description $
 * Listbox &                      @macro = listbox_description $
 * Minimap &                      @macro = minimap_description $
 * Multi_page &                   @macro = multi_page_description $
 * Panel &                        @macro = panel_description $
 * Repeating_button &             @macro = repeating_button_description $
 * Scroll_label &                 @macro = scroll_label_description $
 * Slider &                       @macro = slider_description $
 * Spacer &                       @macro = spacer_description $
 * Stacked_widget &
 *     A stacked widget is a control several widgets can be stacked on top of
 *     each other in the same space. This is mainly intended for over- and
 *     underlays. (The widget is still experimental.) $
 *
 * Text_box &                     A single line text box. $
 * Tree_view &                    @macro = tree_view_description $
 * Toggle_button &
 *     A kind of button with two 'states' normal and selected. This is a more
 *     generic widget which is used for eg checkboxes and radioboxes. $
 *
 * Toggle_panel &
 *     Like a toggle button but then as panel so can hold multiple items in a
 *     grid. $
 *
 * Tooltip &                      A small tooltip with help. $
 * Tree_view &                    A tree view widget. $
 * Vertical_scrollbar &           A vertical scrollbar. $
 * Window &                       A window. $
 * @end{table}
 *
 * <span id="window_list"></span>List of available windows:
 * @begin{table}{window_overview}
 *     Addon_connect &                The dialog to connect to the addon server
 *                                   and maintain locally installed addons. $
 *     Addon_list &                   Shows the list of the addons to install or
 *                                   update. $
 *     Campaign_selection &           Shows the list of campaigns, to select one
 *                                   to play. $
 *     Language_selection &           The dialog to select the primairy language. $
 *     WML_message_left &             The ingame message dialog with a portrait
 *                                   on the left side. (Used for the WML messages.) $
 *     WML_message_right &            The ingame message dialog with a portrait
 *                                   on the right side. (Used for the WML
 *                                   messages.) $
 *     Message &                      A generic message dialog. $
 *     MP_connect &                   The dialog to connect to the MP server. $
 *     MP_method_selection &          The dialog to select the kind of MP game
 *                                   to play. Official server, local etc. $
 *     MP_server_list &               List of the 'official' MP servers. $
 *     MP_login &                     The dialog to provide a password for registered
 *                                   usernames, request a password reminder or
 *                                   choose a different username. $
 *     MP_cmd_wrapper &               Perform various actions on the selected user
 *                                   (e.g. whispering or kicking). $
 *     MP_create_game &               The dialog to select and create an MP game. $
 *     Title_screen &                 The title screen. $
 *     Editor_new_map &               Creates a new map in the editor. $
 *     Editor_generate_map &          Generates a random map in the editor. $
 *     Editor_resize_map &            Resizes a map in the editor. $
 *     Editor_settings &              The settings specific for the editor. $
 * @end{table}
 * @end{tag}{name=gui}
 * @end{parent}{name="/"}
 */
	id = cfg["id"].str();
	VALIDATE(id == "default", "gui's id must be default");

	/***** Control definitions *****/
	typedef std::pair<
			  const std::string
			, std::function<void(
				  tgui_definition&
				  , const std::string&
				  , const config&
				  , const char *key)> > thack;

	BOOST_FOREACH(thack& widget_type, registred_widget_type()) {
		widget_type.second(*this, widget_type.first, cfg, NULL);
	}

	/***** Template widget *****/
	// Some window contain template widegt, require parse them before window.
	BOOST_FOREACH (const config& tpl, cfg.child_range("tpl_widget")) {
		ttpl_widget tpl_widget(tpl);
		const std::string id = utils::generate_app_prefix_id(tpl_widget.app, tpl_widget.id);
		if (tpl_widgets.count(id) != 0) {
			const std::string error_msg("template widget is defined more than once: '" + id + "'");
			VALIDATE(false, error_msg);
		}

		// rose studio require rose__keyboard, so add it to tpl_widgets still.
		tpl_widgets.insert(std::make_pair(id, tpl_widget));

		if (id == utils::generate_app_prefix_id("rose", "keyboard")) {
			// keyboard_widget.reset(tpl_widget.landscape->build());
			// keyboard_widget.get()->set_id("keyboard");
		}
	}

	/***** Window types *****/
	BOOST_FOREACH (const config &w, cfg.child_range("window")) {
		std::pair<std::string, twindow_builder> child;
		child.first = child.second.read(w);
		window_types.insert(child);
	}

	// The default gui needs to define all window types since we're the
	// fallback in case another gui doesn't define the window type.
	for (std::vector<std::string>::const_iterator itor = registered_window_types().begin(); itor != registered_window_types().end(); ++ itor) {
        const std::string error_msg("Window not defined in WML: '" +
                                        *itor +
                                        "'. Perhaps a mismatch between data and source versions."
                                        " Try --data-dir <trunk-dir>" );
		VALIDATE(window_types.find(*itor) != window_types.end(), error_msg);
	}

	/***** settings *****/
/*WIKI
 * @page = GUIToolkitWML
 * @order = 1
 *
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="settings"}{min="0"}{max="1"}
 * A setting section has the following variables:
 * @begin{table}{config}
 *     double_click_time & unsigned & &   The time between two clicks to still be a
 *                                     double click. $
 *
 *     sound_button_click & string & "" &
 *                                     The sound played if a button is
 *                                     clicked. $
 *     sound_toggle_button_click & string & "" &
 *                                     The sound played if a toggle button is
 *                                     clicked. $
 *     sound_toggle_panel_click & string & "" &
 *                                     The sound played if a toggle panel is
 *                                     clicked. Normally the toggle panels
 *                                     are the items in a listbox. If a
 *                                     toggle button is in the listbox it's
 *                                     sound is played. $
 *     sound_slider_adjust & string & "" &
 *                                     The sound played if a slider is
 *                                     adjusted. $
 *
 *     has_helptip_message & t_string & &
 *                                     The string used to append the tooltip
 *                                     if there is also a helptip. The WML
 *                                     variable @$hotkey can be used to get show
 *                                     the name of the hotkey for the help. $
 * @end{table}
 * @end{tag}{name="settings"}
 */

/*WIKI
 * @begin{tag}{name="tip"}{min="0"}{max="-1"}
 * @begin{table}{config}
 *     source & t_string & & Author
 *     text & t_string & & Text of the tip.
 * @end{table}
 * @end{tag}{name="tip"}
 * @end{parent}{name="gui/"}
*/

/**
 * @todo Regarding sounds:
 * Need to evaluate but probably we want the widget definition be able to:
 * - Override the default (and clear it). This will allow toggle buttons in a
 *   listbox to sound like a toggle panel.
 * - Override the default and above per instance of the widget, some buttons
 *   can give a different sound.
 */
	const config &settings = cfg.child("settings");

	gui2::settings::double_click_time = settings["double_click_time"];
	gui2::settings::longpress_time = game_config::longpress_time == nposm? settings["longpress_time"]: game_config::longpress_time;
	gui2::settings::clear_click_threshold = settings["clear_click_threshold"].to_int() * twidget::hdpi_scale;

	gui2::settings::sound_button_click = settings["sound_button_click"].str();
	gui2::settings::sound_toggle_button_click = settings["sound_toggle_button_click"].str();
	gui2::settings::sound_toggle_panel_click = settings["sound_toggle_panel_click"].str();
	gui2::settings::sound_slider_adjust = settings["sound_slider_adjust"].str();

	gui2::settings::horizontal_scrollbar_id = settings["horizontal_scrollbar_id"].str();
	gui2::settings::vertical_scrollbar_id = settings["vertical_scrollbar_id"].str();
	gui2::settings::tooltip_id = settings["tooltip_id"].str();
	gui2::settings::drag_id = settings["drag_id"].str();
	gui2::settings::magnifier_id = settings["magnifier_id"].str();
	gui2::settings::track_id = settings["track_id"].str();
	gui2::settings::button_id = settings["button_id"].str();
	gui2::settings::edit_select_all_id = settings["edit_select_all_id"].str();
	gui2::settings::edit_select_id = settings["edit_select_id"].str();
	gui2::settings::edit_copy_id = settings["edit_copy_id"].str();
	gui2::settings::edit_paste_id = settings["edit_paste_id"].str();

	gui2::settings::actived = true;
	return id;
}

void tgui_definition::load_widget_definitions(
		  const std::string& definition_type
		, const std::vector<tcontrol_definition_ptr>& definitions)
{
	BOOST_FOREACH(const tcontrol_definition_ptr& def, definitions) {

		// We assume all definitions are unique if not we would leak memory.
		VALIDATE(control_definition[definition_type].find(def->id)
				== control_definition[definition_type].end(), "Multi-define id!");

		control_definition[definition_type].insert(std::make_pair(utils::generate_app_prefix_id(def->app, def->id), def));
	}

	utils::string_map symbols;
	symbols["definition"] = definition_type;
	symbols["id"] = "default";
	t_string msg(vgettext2( 
			  "Widget definition '$definition' "
			  "doesn't contain the definition for '$id'."
			, symbols));

	VALIDATE(control_definition[definition_type].find("default")
			!= control_definition[definition_type].end(), msg);

}

std::unique_ptr<tkeyboard> keyboard2;

void tgui_definition::create_keyboard_widget(tfloat_widget* vertical, tfloat_widget* horizontal)
{
	VALIDATE(vertical != nullptr && horizontal != nullptr, null_str);
	VALIDATE(keyboard_widget.get() == nullptr && keyboard_vertical_scrollbar == nullptr && keyboard_horizontal_scrollbar == nullptr, null_str);

	keyboard_vertical_scrollbar = vertical;
	keyboard_horizontal_scrollbar = horizontal;

	// rose studio require rose__keyboard, so add it to tpl_widgets still.
	std::map<std::string, ttpl_widget>::const_iterator find = tpl_widgets.find(utils::generate_app_prefix_id("rose", "keyboard"));
	VALIDATE(find != tpl_widgets.end(), null_str);
	keyboard_widget.reset(find->second.landscape->build());
	keyboard_widget->set_id("keyboard");

	keyboard_vertical_scrollbar->widget->set_parent(nullptr);
	keyboard_horizontal_scrollbar->widget->set_parent(nullptr);

	keyboard2.reset(new tkeyboard(*keyboard_widget.get()));
}

void tgui_definition::clear_texture()
{
	VALIDATE(keyboard_widget.get() != nullptr, null_str);
	keyboard_widget.get()->clear_texture();
	keyboard_vertical_scrollbar->clear_texture();
	keyboard_horizontal_scrollbar->clear_texture();
	keyboard2->clear_texture();
}

/** Map with all known windows, (the builder class builds a window). */
// std::map<std::string, twindow_builder> windows;

tgui_definition gui;

void register_window(const std::string& app, const std::string& id)
{
	std::string id2 = utils::generate_app_prefix_id(app, id);
	const std::vector<std::string>::iterator itor = std::find(
			  registered_window_types().begin()
			, registered_window_types().end()
			, id2);

	if (itor == registered_window_types().end()) {
		registered_window_types().push_back(id2);
	}
}

std::vector<std::string> tunit_test_access_only::get_registered_window_list()
{
	return gui2::registered_window_types();
}

void load_settings()
{
	// Init.
	twindow::update_screen_size();

	// Read file.
	config cfg;
	try {
		wml_config_from_file(game_config::path + "/xwml/" + "gui.bin", cfg);

	} catch(config::error&) {
		VALIDATE(false, "Setting: could not read file 'data/gui/default.cfg'");
	}

	// there is only one [gui], parse it.
	const config& gui_cfg = cfg.child("gui");
	VALIDATE(gui_cfg, _("No gui defined."));

	gui.read(gui_cfg);
}

void release()
{
	keyboard2.reset();
	if (gui.keyboard_widget.get() != nullptr) {
		const twidget* p = gui.keyboard_widget.get()->parent();
		gui.keyboard_widget.reset();
	}
	if (gui.keyboard_vertical_scrollbar) {
		VALIDATE(gui.keyboard_vertical_scrollbar->widget->parent() == nullptr, null_str);
		delete gui.keyboard_vertical_scrollbar;
		gui.keyboard_vertical_scrollbar = nullptr;
	}
	if (gui.keyboard_horizontal_scrollbar) {
		VALIDATE(gui.keyboard_horizontal_scrollbar->widget->parent() == nullptr, null_str);
		delete gui.keyboard_horizontal_scrollbar;
		gui.keyboard_horizontal_scrollbar = nullptr;
	}
}

tstate_definition::tstate_definition(const config &cfg) :
	canvas()
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 1_widget
 *
 * == State ==
 *
 * @begin{parent}{name="generic/"}
 * @begin{tag}{name="state"}{min=0}{max=1}
 * Definition of a state. A state contains the info what to do in a state.
 * Atm this is rather focussed on the drawing part, might change later.
 * Keys:
 * @begin{table}{config}
 *     draw & section & &                 Section with drawing directions for a canvas. $
 * @end{table}
 * @end{tag}{name="state"}
 * @end{parent}{name="generic/"}
 *
 */

	const config &draw = *(cfg ? &cfg.child("draw") : &cfg);

	VALIDATE(draw, _("No state or draw section defined."));

	canvas.set_cfg(draw);
}

void register_widget(const std::string& id
		, std::function<void(
			  tgui_definition& gui_definition
			, const std::string& definition_type
			, const config& cfg
			, const char *key)> functor)
{
	registred_widget_type().insert(std::make_pair(id, functor));
}

void load_widget_definitions(
	  tgui_definition& gui_definition
	, const std::string& definition_type
	, const std::vector<tcontrol_definition_ptr>& definitions)
{
	gui_definition.load_widget_definitions(definition_type, definitions);
}

tresolution_definition_ptr get_control(
		const std::string& control_type, const std::string& definition)
{
	const tgui_definition::tcontrol_definition_map::const_iterator
	control_definition = gui.control_definition.find(control_type);

	std::map<std::string, tcontrol_definition_ptr>::const_iterator
		control = control_definition->second.find(definition);

	if (control == control_definition->second.end()) {
		// Control: type 'control_type' definition 'definition' not found, falling back to 'default'.
		control = control_definition->second.find("default");
		VALIDATE(control != control_definition->second.end(), "Cannot find defnition, failling back to default!");
	}

	tpoint landscape_size = twidget::orientation_swap_size(settings::screen_width, settings::screen_height);

	for (std::vector<tresolution_definition_ptr>::const_iterator
			itor = (*control->second).resolutions.begin(),
			end = (*control->second).resolutions.end();
			itor != end;
			++itor) {

		if (landscape_size.x <= (**itor).window_width || landscape_size.y <= (**itor).window_height) {
			return *itor;

		} else if (itor == end - 1) {
			return *itor;
		}
	}

	VALIDATE(false, null_str);
	return NULL;
}

std::vector<twindow_builder::tresolution>::const_iterator get_window_builder(const std::string& type)
{
	twindow::update_screen_size();

	std::map<std::string, twindow_builder>::const_iterator window = gui.window_types.find(type);

	VALIDATE(window != gui.window_types.end(), null_str);
	VALIDATE(window->second.resolutions.size() == 1, null_str);

	return window->second.resolutions.begin();
}

void insert_window_builder_from_cfg(const config& cfg)
{
	BOOST_FOREACH (const config &w, cfg.child_range("window")) {
		std::pair<std::string, twindow_builder> child;
		child.first = child.second.read(w);
		VALIDATE(gui.window_types.count(child.first) == 0, null_str);
		gui.window_types.insert(child);
	}
}

void remove_window_builder_baseon_app(const std::string& app)
{
	VALIDATE(!app.empty(), null_str);
	const std::string prefix = app + "__";
	for (std::map<std::string, twindow_builder>::iterator it = gui.window_types.begin(); it != gui.window_types.end(); ) {
		const std::string& id = it->first;
		if (id.find(prefix) != 0) {
			++ it;
		} else {
			gui.window_types.erase(it ++);
		}
	}
}

const ttpl_widget& get_tpl_widget(const std::string& tpl_id)
{
	std::map<std::string, ttpl_widget>::const_iterator it = gui.tpl_widgets.find(tpl_id);

	VALIDATE(it != gui.tpl_widgets.end(), null_str);
	return it->second;
}

bool is_tpl_widget(const std::string& tpl_id)
{
	return gui.tpl_widgets.find(tpl_id) != gui.tpl_widgets.end();
}

bool is_control_definition(const std::string& type, const std::string& definition)
{
	const tgui_definition::tcontrol_definition_map& controls = gui.control_definition;
	const std::map<std::string, tcontrol_definition_ptr>& map = controls.find(type)->second;

	return map.find(definition) != map.end();
}

twidget* get_keyboard_widget()
{
	return gui.keyboard_widget.get();
}

tkeyboard* get_keyboard2()
{
	return keyboard2.get();
}

bool widget_in_keyboard(const twidget& widget)
{
	const twidget* keyboard_widget = get_keyboard_widget();
	const twidget* p = &widget;
	while (p != nullptr) {
		if (p == keyboard_widget) {
			return true;
		}
		p = p->parent();
	}
	return false;
}

/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 *
 */

} // namespace gui2
