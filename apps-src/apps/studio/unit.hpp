/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef STUDIO_UNIT_HPP_INCLUDED
#define STUDIO_UNIT_HPP_INCLUDED

#include "base_unit.hpp"
#include "gui/auxiliary/widget_definition.hpp"
#include "gui/dialogs/cell_setting.hpp"

class mkwin_controller;
class mkwin_display;
class unit_map;

class unit: public base_unit
{
public:
	enum {NONE, WIDGET, WINDOW, COLUMN, ROW};
	struct tchild
	{
		tchild()
			: window(NULL)
		{}

		void clear(bool del)
		{
			if (del) {
				if (window) {
					delete window;
				}
				for (std::vector<unit*>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
					delete *it;
				}
				for (std::vector<unit*>::const_iterator it = cols.begin(); it != cols.end(); ++ it) {
					delete *it;
				}
				for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
					delete *it;
				}
			}
			window = NULL;
			rows.clear();
			cols.clear();
			units.clear();
		}

		bool is_all_spacer() const
		{
			for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
				const unit* u = *it;
				if (!u->is_spacer()) {
					return false;
				}
			}
			return true;
		}

		void generate(config& cfg) const;
		void from(mkwin_controller& controller, mkwin_display& disp, unit_map& units2, unit* parent, int number, const config& cfg);

		void draw_minimap_architecture(surface& screen, const SDL_Rect& minimap_location, const double xscaling, const double yscaling, int level) const;

		// delete unit in unit_map
		void erase(unit_map& units2);

		unit* find_unit(const std::string& id) const;
		std::pair<unit*, int> find_layer(const std::string& id) const;

		// hit must be in first units.
		tpoint locate_u(const unit& hit) const;

		tpoint calculate_best_size(const game_logic::map_formula_callable& variables) const;
		void place(mkwin_controller& controller, mkwin_display& disp, unit_map& units2, const game_logic::map_formula_callable& variables, const tpoint& origin, const tpoint& size, std::vector<unit*>& to_units) const;

		unit* window;
		std::vector<unit*> rows;
		std::vector<unit*> cols;
		std::vector<unit*> units;
	};

	struct tparent
	{
		tparent(unit* u, int number = -1)
			: u(u)
			, number(number)
		{}

		unit* u;
		int number;
	};

	static int preview_widget_best_width;
	static int preview_widget_best_height;
	static const std::string widget_prefix;

	static std::string form_widget_png(const std::string& type, const std::string& definition);
	static std::string formual_fill_str(const std::string& core);
	static std::string formual_extract_str(const std::string& str);

	unit(mkwin_controller& controller, mkwin_display& disp, unit_map& units, const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget, unit* parent, int number);
	unit(mkwin_controller& controller, mkwin_display& disp, unit_map& units, int type, unit* parent, int number);
	unit(const unit& that);
	virtual ~unit();

	bool require_sort() const { return true; }
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget() const { return widget_; }

	std::string image() const;
	void set_click_dismiss(bool val) { cell_.window.click_dismiss = val; }
	gui2::tcell_setting& cell() { return cell_; }
	const gui2::tcell_setting& cell() const { return cell_; }
	void set_cell(const gui2::tcell_setting& cell);

	void set_child(int number, const tchild& child);
	bool has_child() const { return !children_.empty(); }
	tchild& child(int index) { return children_[index]; }
	const tchild& child(int index) const { return children_[index]; }
	const std::vector<tchild>& children() const { return children_; }
	std::vector<tchild>& children() { return children_; }
	int children_layers() const;

	bool is_spacer() const { return type_ == WIDGET && widget_.first == "spacer"; }
	bool is_label() const { return type_ == WIDGET && widget_.first == "label"; }
	bool is_button() const { return type_ == WIDGET && widget_.first == "button"; }
	bool is_toggle_button() const { return type_ == WIDGET && widget_.first == "toggle_button"; }
	bool is_track() const { return type_ == WIDGET && widget_.first == "track"; }
	bool is_grid() const { return type_ == WIDGET && widget_.first == "grid"; }
	bool is_stack() const { return type_ == WIDGET && widget_.first == "stack"; }
	bool is_listbox() const { return type_ == WIDGET && widget_.first == "listbox"; }
	bool is_scroll_label() const { return type_ == WIDGET && widget_.first == "scroll_label"; }
	bool is_scroll_text_box() const { return type_ == WIDGET && widget_.first == "scroll_text_box"; }
	bool is_scroll_panel() const { return type_ == WIDGET && widget_.first == "scroll_panel"; }
	bool is_tree() const { return type_ == WIDGET && widget_.first == "tree"; }
	bool is_report() const { return type_ == WIDGET && widget_.first == "report"; }
	bool is_slider() const { return type_ == WIDGET && widget_.first == "slider"; }
	bool is_text_box() const { return type_ == WIDGET && widget_.first == "text_box"; }
	bool is_scroll() const { return is_listbox() || is_scroll_label() || is_scroll_text_box() || is_scroll_panel() || is_tree() || is_report(); }
	bool is_panel() const { return type_ == WIDGET && (widget_.first == "panel" || widget_.first == "toggle_panel"); }
	bool is_image() const { return type_ == WIDGET && widget_.first == "image"; }
	bool has_size() const { return !is_grid(); }
	bool has_mtwusc() const { return type_ == WIDGET && (is_label() || (widget_.second && widget_.second->id == "mtwusc")); }
	bool has_size_is_max() const { return is_scroll() || is_label(); }
	bool has_atom_markup() const { return widget_.first == "text_box" || widget_.first == "scroll_text_box"; }
	bool has_margin() const { return type_ == WIDGET && (widget_.first == "panel" || widget_.first == "toggle_panel" || widget_.first == "scroll_panel"); }
	bool is_extensible() const { return type_ == WIDGET && (widget_.first == "panel" || widget_.first == "scroll_panel" || widget_.first == "grid"); }
	bool is_main_map() const;

	const tparent& parent() const { return parent_; }
	void set_parent(unit* parent, int number) { parent_ = tparent(parent, number); }
	void set_parent_number(int number) { parent_.number = number; }
	const unit* parent_at_top() const;
	int type() const { return type_; }

	void insert_child(int w, int h);
	void erase_child(int index);
	void insert_listbox_child(int w, int h);
	void insert_treeview_child();
	std::string child_tag(int index) const;

	void to_cfg(config& cfg) const;
	void from_cfg(const config& cfg);
	void from_widget(const config& cfg, bool unpack);

	void set_widget_definition(const gui2::tcontrol_definition_ptr& definition) { widget_.second = definition; }
	std::string widget_tag() const;

	bool is_tpl() const;

protected:
	void redraw_widget(int xsrc, int ysrc, int width, int height) const;

	void generate_main_map_border(config& cfg) const;

	void to_cfg_window(config& cfg) const;
	void to_cfg_row(config& cfg) const;
	void to_cfg_column(config& cfg) const;
	void to_cfg_widget(config& cfg) const;
	void to_cfg_widget_tpl(config& subcfg) const;
	void generate_grid(config& cfg) const;
	void generate_stack(config& cfg) const;
	void generate_listbox(config& cfg) const;
	void generate_toggle_panel(config& cfg) const;
	void generate_scroll_panel(config& cfg) const;
	void generate_tree_view(config& cfg) const;
	void generate_report(config& cfg) const;
	void generate_slider(config& cfg) const;
	void generate_text_box(config& cfg) const;
	void generate_label(config& cfg) const;

	void from_window(const config& cfg);
	void from_row(const config& cfg);
	void from_column(const config& cfg);
	bool from_cfg_widget_tpl();
	void from_grid(const config& cfg);
	void from_stack(const config& cfg);
	void from_listbox(const config& cfg);
	void from_toggle_panel(const config& cfg);
	void from_scroll_panel(const config& cfg);
	void from_tree_view(const config& cfg);
	void from_report(const config& cfg);
	void from_slider(const config& cfg);
	void from_text_box(const config& cfg);
	void from_label(const config& cfg);

private:
	void app_draw_unit(const int xsrc, const int ysrc) override;

protected:
	mkwin_controller& controller_;
	mkwin_display& disp_;
	unit_map& units_;

	int type_;
	tparent parent_;
	std::pair<std::string, gui2::tcontrol_definition_ptr> widget_;
	gui2::tcell_setting cell_;
	std::vector<tchild> children_;
};

std::string formual_extract_str(const std::string& str);

#endif
