/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_KEYBOARD_HPP_INCLUDED
#define GUI_WIDGETS_KEYBOARD_HPP_INCLUDED

#include "gui/widgets/control.hpp"

namespace gui2 {

class tpanel;
class tbutton;
class tstack;
class tgrid;
class treport;
class twindow;
class ttext_box;

class tkeyboard
{
public:
	enum {ALPHA_PAGE, SYMBOL_PAGE, MORE_PAGE};
	enum {LINE1_DIGIT, LINE1_IME};
	// english(true): ALPHA_CATEGORY is alpha page.
	// english(false): ALPHA_CATEGORY is chinese alpha page.
	enum {ALPHA_CATEGORY, SYMBOL_CATEGORY, IME_CATEGORY, CATEGORY_COUNT};
	explicit tkeyboard(twidget& widget);
	~tkeyboard();

	void did_shown(twindow& window);
	void did_hidden();
	void clear_texture();

private:
	void click_expand(tgrid& grid, tbutton& twidget);
	void click_capslock(tgrid& grid, tbutton& twidget);
	void click_backspace(tgrid& grid);
	void click_fullwidth(tbutton& twidget);
	void click_tone(tbutton& twidget);
	void click_left_allow(tbutton& twidget);
	void click_right_allow(tbutton& twidget);
	void click_category(tbutton& widget);
	void click_goback_ime(tbutton& widget);
	void click_more_tone(tbutton& widget);
	void click_space();
	void click_action();
	void click_close();

private:
	void pre_alpha(tgrid& grid);
	void pre_symbol(tgrid& grid);
	void pre_more(tgrid& grid);
	void did_alpha_clicked(treport& report, tbutton& widget, tgrid& grid);
	void pinyin_changed(tgrid& grid);
	void category_changed();
	void capslock_changed();
	void fullwidth_changed(int category, bool to_english);
	void rest_chinese_session();
	void did_symbol_item_clicked(treport& report, tbutton& widget, tgrid& grid);
	void did_more_item_clicked(treport& report, tbutton& widget, tgrid& grid);
	ttext_box* get_keyboard_capture_text_box();
	bool equal_pinyin(const char* pinyin_table, int code, const std::string& pinyin) const;
	void more_tone_changed();

public:
	texture tex_buf;
	bool use_tex_buf;

private:
	const bool frequence_only_;
	const int max_chinese_line1_items_;
	const int max_symbols_items_;
	const int max_more_items_;
	const std::string expand_label_;
	const char* pinyin_table_;
	std::map<int, std::string> category_labels_;
	std::map<int, std::string> tone_labels_;
	struct teng_2_chinese {
		uint32_t english;
		uint32_t chinese;
	};
	teng_2_chinese* e2c_symbol_table_;
	int e2c_symbols_;
	int current_page_;
	int current_category_;
	tpanel* widget_;
	tstack* stack_;
	tbutton* category_alpha_;
	tbutton* category_symbol_;
	tbutton* category_ime_;
	tbutton* space_;
	tbutton* action_;
	tbutton* close_;
	tstack* line1_stack_;
	tbutton* expand_;
	tbutton* fullwidth_alpha_;
	tbutton* fullwidth_symbol_;
	tbutton* capslock_widget_;
	tbutton* tone_alpha_;

	bool english_;
	bool capslock_;

	enum {CHINESE_FLAG_TONE12 = 0x1};
	struct tchinese_item {
		int at; // unicode - chinese::min_unicode
		uint32_t flag;
	};
	tchinese_item* chinese_frequence_;
	int frequence_size_;
	enum {tone_all, tone_12, tone_34, tone_count};
	int current_tone_;

	// if frequence_only_ = true, value in chinese_candidate_ is index of chinese_frequence_.
	// else(frequence_only_ = false) is index of chinese::pinyin_code.
	int* chinese_candidate_;
	// <start, size>
	std::vector<std::pair<int, int> > chinese_candidate_segments_;
	std::string pinyin_;

	// if frequence_only_ = true, value in ime_more_indexs_ is index of chinese_frequence_.
	std::vector<int> ime_more_indexs_;
	int current_more_tone_;

	twindow* window_;
};

} // namespace gui2

#endif

