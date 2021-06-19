/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/keyboard.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/widgets/panel.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
#include "gettext.hpp"
#include "font.hpp"
#include "sound.hpp"
#include "filesystem.hpp"

#include "rose_config.hpp"
#include "config_cache.hpp"

#include <algorithm>
#include <iomanip>

using namespace std::placeholders;

namespace gui2 {

tkeyboard::tkeyboard(twidget& widget)
	: frequence_only_(true)
	, current_page_(ALPHA_PAGE)
	, current_category_(ALPHA_CATEGORY)
	, max_chinese_line1_items_(30)
	, max_symbols_items_(6 * 9)
	, max_more_items_(4 * 9)
	, expand_label_("http://")
	, capslock_(false)
	, english_(true)
	, window_(nullptr)
	, widget_(dynamic_cast<tpanel*>(&widget))
	, stack_(find_widget<tstack>(widget_, "stack", false, true))
	, category_alpha_(find_widget<tbutton>(&widget, "category_alpha", false, true))
	, category_symbol_(find_widget<tbutton>(&widget, "category_symbol", false, true))
	, category_ime_(find_widget<tbutton>(&widget, "category_ime", false, true))
	, space_(find_widget<tbutton>(&widget, "space", false, true))
	, action_(find_widget<tbutton>(&widget, "action", false, true))
	, close_(find_widget<tbutton>(&widget, "close", false, true))
	, line1_stack_(nullptr)
	, expand_(nullptr)
	, fullwidth_alpha_(nullptr)
	, fullwidth_symbol_(nullptr)
	, capslock_widget_(nullptr)
	, tone_alpha_(nullptr)
	, chinese_candidate_(nullptr)
	, chinese_frequence_(nullptr)
	, frequence_size_(0)
	, e2c_symbol_table_(nullptr)
	, e2c_symbols_(0)
	, current_tone_(tone_all)
	, current_more_tone_(tone_all)
	, pinyin_table_(chinese::get_pinyin_code())
	, use_tex_buf(false)
{
	widget_->set_can_invalidate_layout();

	category_labels_.insert(std::make_pair(ALPHA_CATEGORY, "1ab"));
	category_labels_.insert(std::make_pair(SYMBOL_CATEGORY, "1#+"));
	category_labels_.insert(std::make_pair(IME_CATEGORY, _("ime^chinese")));
	VALIDATE(category_labels_.size() == CATEGORY_COUNT, null_str);

	tone_labels_.insert(std::make_pair(tone_all, _("ime^tone all")));
	tone_labels_.insert(std::make_pair(tone_12, _("ime^tone12")));
	tone_labels_.insert(std::make_pair(tone_34, _("ime^tone34")));
	VALIDATE(tone_labels_.size() == tone_count, null_str);

	//
	// ime.cfg
	//
	config cfg;
	{
		config_cache_transaction transaction;
		config_cache& cache = config_cache::instance();
		cache.clear_defines();

		cache.get_config(game_config::path + "/data/core/cert/ime.cfg", cfg);
	}
	// read english_2_chinese symbols
	const int max_e2c_symbols = 1024;
	e2c_symbol_table_ = (teng_2_chinese*)malloc(max_e2c_symbols * sizeof(teng_2_chinese));
	memset(e2c_symbol_table_, 0, max_e2c_symbols * sizeof(teng_2_chinese));

	const config& symbols_cfg = cfg.child("symbols");
	VALIDATE(symbols_cfg, null_str);
	std::vector<std::string> vstr;
	std::set<int> ats, english_codes, chinese_codes;
	const std::string key_prefix = "key";
	BOOST_FOREACH (const config::attribute &istrmap, symbols_cfg.attribute_range()) {
		const std::string& key = istrmap.first;
		VALIDATE(key.find(key_prefix) == 0, null_str);
		int at = utils::to_int(key.substr(key_prefix.size()));
		VALIDATE(at >= 0 && ats.count(at) == 0, null_str);
		ats.insert(at);

		const std::string& value = istrmap.second;
		vstr = utils::split(value);
		VALIDATE(vstr.size() == 2, null_str);
		// english
		uint32_t english = utils::to_uint32(vstr[0]);
		VALIDATE(english_codes.count(english) == 0, null_str);
		english_codes.insert(english);

		// chinese
		uint32_t chinese = utils::to_uint32(vstr[1]);
		VALIDATE(chinese_codes.count(chinese) == 0, null_str);
		chinese_codes.insert(chinese);

		teng_2_chinese* e2c = e2c_symbol_table_ + at;
		e2c->english = english;
		e2c->chinese = chinese;
	}
	e2c_symbols_ = english_codes.size();
	VALIDATE(e2c_symbols_ > 0 && *ats.rbegin() == e2c_symbols_ - 1, null_str);
	VALIDATE(e2c_symbols_ <= max_symbols_items_, null_str);

	pre_alpha(*stack_->layer(ALPHA_PAGE));
	pre_symbol(*stack_->layer(SYMBOL_PAGE));
	pre_more(*stack_->layer(MORE_PAGE));

	connect_signal_mouse_left_click(
		*category_alpha_
		, std::bind(
			&tkeyboard::click_category
			, this, std::ref(*category_alpha_)));
	// category_alpha_->set_sound("button.wav");
	category_alpha_->set_cookie(ALPHA_CATEGORY);

	connect_signal_mouse_left_click(
		*category_symbol_
		, std::bind(
			&tkeyboard::click_category
			, this, std::ref(*category_symbol_)));
	// category_symbol_->set_sound("button.wav");
	category_symbol_->set_cookie(SYMBOL_CATEGORY);

	connect_signal_mouse_left_click(
		*category_ime_
		, std::bind(
			&tkeyboard::click_category
			, this, std::ref(*category_ime_)));
	// category_ime_->set_sound("button.wav");
	category_ime_->set_cookie(IME_CATEGORY);

	connect_signal_mouse_left_click(
		*space_
		, std::bind(
			&tkeyboard::click_space
			, this));
	space_->set_border("textbox");
	space_->set_sound("button.wav");

	connect_signal_mouse_left_click(
		*action_
		, std::bind(
			&tkeyboard::click_action
			, this));
	action_->set_visible(twidget::INVISIBLE);

	connect_signal_mouse_left_click(
		*close_
		, std::bind(
			&tkeyboard::click_close
			, this));

	// I think chinese_candidate_ must not than candidate_size.
	const int candidate_size = posix_align_ceil(chinese::get_pinyin_code_bytes(), 1024);
	chinese_candidate_ = (int*)malloc(candidate_size * sizeof(int));

	std::set<int> absents;
	{
		std::string filename = game_config::path + "/data/core/cert/chinesefrequence.code";
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		VALIDATE(fsize > 0 && (fsize & 1) == 0, null_str);
		frequence_size_ = fsize / 2;

		chinese_frequence_ = (tchinese_item*)malloc(frequence_size_ * sizeof(tchinese_item));
		memset(chinese_frequence_, 0, frequence_size_ * sizeof(tchinese_item));
		for (int i = 0; i < frequence_size_; i ++) {
			wchar_t code = posix_mku16(file.data[i << 1], file.data[(i << 1) + 1]);
			VALIDATE(code >= chinese::min_unicode && code <= chinese::max_unicode, null_str);
			tchinese_item* tmp = chinese_frequence_ + i;
			tmp->at = code - chinese::min_unicode;
			const char* ptr = pinyin_table_ + tmp->at * chinese::fix_bytes_in_code;
			while (ptr[0] >= 'a' && ptr[0] <= 'z') {
				ptr ++;
			}
			VALIDATE(ptr[0] == '\0' || (ptr[0] >= '1' && ptr[0] <= '5'), null_str);
			if (ptr[0] == '\0' || ptr[0] == '1' || ptr[0] == '2' || ptr[0] == '5') {
				// think 1, 5 as 1.
				tmp->flag |= CHINESE_FLAG_TONE12;
			}
			if (game_config::os == os_windows && ptr[0] == '\0') {
				absents.insert(code);
			}
		}
	}

	if (game_config::os == os_windows && !absents.empty()) {
/*
		std::stringstream fp_ss;
		for (std::set<int>::const_iterator it = absents.begin(); it != absents.end(); ++ it) {
			wchar_t ch = *it;
			fp_ss << UCS2_to_UTF8(ch) << "(";
			fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
		}
		tfile file(game_config::preferences_dir + "/absents.dat", GENERIC_WRITE, CREATE_ALWAYS);
		posix_fwrite(file.fp, fp_ss.str().c_str(), fp_ss.str().size());
*/
	}

	category_changed();
}

tkeyboard::~tkeyboard()
{
	if (chinese_candidate_ == nullptr) {
		free(chinese_candidate_);
		chinese_candidate_ = nullptr;
	}
	if (chinese_frequence_ != nullptr) {
		free(chinese_frequence_);
		chinese_frequence_ = nullptr;
	}
}

std::string get_border(wchar_t code)
{
	return code == 'f' || code == 'j'? "red_textbox": "textbox";
}

std::string key_unicode_2_label(wchar_t code)
{
	if (code >= 'a' && code <= 'z') {
		code -= 'a' - 'A'; 
	}
	return UCS2_to_UTF8(code);
}

void tkeyboard::pre_alpha(tgrid& grid)
{
	line1_stack_ = find_widget<tstack>(&grid, "line1_stack", false, true);

	std::vector<wchar_t> unicodes;
	unicodes.push_back('1');
	unicodes.push_back('2');
	unicodes.push_back('3');
	unicodes.push_back('4');
	unicodes.push_back('5');
	unicodes.push_back('6');
	unicodes.push_back('7');
	unicodes.push_back('8');
	unicodes.push_back('9');
	unicodes.push_back('0');

	std::stringstream id_ss;
	treport* english_line1 = find_widget<treport>(&grid, "english_line1", false, true);
	for (std::vector<wchar_t>::const_iterator it = unicodes.begin(); it != unicodes.end(); ++ it) {
		const wchar_t code = *it;
		id_ss.clear();
		id_ss << "keyboard_" << (char)(code);
		tcontrol& widget = english_line1->insert_item(id_ss.str(), UCS2_to_UTF8(code));
		widget.set_sound("button.wav");
		widget.set_border(null_str);
		// widget.set_border("textbox");
		widget.set_cookie(code);
	}
	english_line1->set_did_item_click(std::bind(&tkeyboard::did_alpha_clicked, this, _1, _2, std::ref(grid)));

	treport* chinese_line1 = find_widget<treport>(&grid, "chinese_line1", false, true);
	for (int at = 0; at < max_chinese_line1_items_; at ++) {
		id_ss.clear();
		id_ss << "keyboard_chinese_" << at;
		tcontrol& widget = chinese_line1->insert_item(id_ss.str(), null_str);
		widget.set_sound("button.wav");
		widget.set_border(null_str);
		widget.set_cookie(nposm);
	}
	chinese_line1->set_did_item_click(std::bind(&tkeyboard::did_alpha_clicked, this, _1, _2, std::ref(grid)));

	unicodes.clear();
	unicodes.push_back('q');
	unicodes.push_back('w');
	unicodes.push_back('e');
	unicodes.push_back('r');
	unicodes.push_back('t');
	unicodes.push_back('y');
	unicodes.push_back('u');
	unicodes.push_back('i');
	unicodes.push_back('o');
	unicodes.push_back('p');
	treport* line2 = find_widget<treport>(&grid, "line2", false, true);
	for (std::vector<wchar_t>::const_iterator it = unicodes.begin(); it != unicodes.end(); ++ it) {
		const wchar_t code = *it;
		id_ss.clear();
		id_ss << "keyboard_" << (char)(code);
		tcontrol& widget = line2->insert_item(id_ss.str(), UCS2_to_UTF8(code));
		widget.set_sound("button.wav");
		widget.set_border(get_border(code));
		widget.set_cookie(code);
	}
	line2->set_did_item_click(std::bind(&tkeyboard::did_alpha_clicked, this, _1, _2, std::ref(grid)));

	unicodes.clear();
	unicodes.push_back('a');
	unicodes.push_back('s');
	unicodes.push_back('d');
	unicodes.push_back('f');
	unicodes.push_back('g');
	unicodes.push_back('h');
	unicodes.push_back('j');
	unicodes.push_back('k');
	unicodes.push_back('l');
	treport* line3 = find_widget<treport>(&grid, "line3", false, true);
	for (std::vector<wchar_t>::const_iterator it = unicodes.begin(); it != unicodes.end(); ++ it) {
		const wchar_t code = *it;
		id_ss.clear();
		id_ss << "keyboard_" << (char)(code);
		tcontrol& widget = line3->insert_item(id_ss.str(), UCS2_to_UTF8(code));
		widget.set_sound("button.wav");
		widget.set_border(get_border(code));
		widget.set_cookie(code);
	}
	line3->set_did_item_click(std::bind(&tkeyboard::did_alpha_clicked, this, _1, _2, std::ref(grid)));

	unicodes.clear();
	unicodes.push_back('z');
	unicodes.push_back('x');
	unicodes.push_back('c');
	unicodes.push_back('v');
	unicodes.push_back('b');
	unicodes.push_back('n');
	unicodes.push_back('m');
	// unicodes.push_back('z');
	treport* line4 = find_widget<treport>(&grid, "line4", false, true);
	for (std::vector<wchar_t>::const_iterator it = unicodes.begin(); it != unicodes.end(); ++ it) {
		const wchar_t code = *it;
		id_ss.clear();
		id_ss << "keyboard_" << (char)(code);
		tcontrol& widget = line4->insert_item(id_ss.str(), UCS2_to_UTF8(code));
		widget.set_sound("button.wav");
		widget.set_border(get_border(code));
		widget.set_cookie(code);
	}
	line4->set_did_item_click(std::bind(&tkeyboard::did_alpha_clicked, this, _1, _2, std::ref(grid)));

	expand_ = find_widget<tbutton>(&grid, "expand", false, true);
	connect_signal_mouse_left_click(
		*expand_
		, std::bind(
			&tkeyboard::click_expand
			, this, std::ref(grid), std::ref(*expand_)));
	expand_->set_label(expand_label_);
	expand_->set_sound("button.wav");

	capslock_widget_ = find_widget<tbutton>(&grid, "capslock", false, true);
	connect_signal_mouse_left_click(
		*capslock_widget_
		, std::bind(
			&tkeyboard::click_capslock
			, this, std::ref(grid), std::ref(*capslock_widget_)));
	capslock_widget_->set_label("misc/ime-capslock.png");
	// capslock_widget_->set_sound("button.wav");

	tone_alpha_ = find_widget<tbutton>(&grid, "tone", false, true);
	connect_signal_mouse_left_click(
		*tone_alpha_
		, std::bind(
			&tkeyboard::click_tone
			, this, std::ref(*tone_alpha_)));
	tone_alpha_->set_sound("button.wav");
	tone_alpha_->set_cookie(ALPHA_CATEGORY);

	fullwidth_alpha_ = find_widget<tbutton>(&grid, "fullwidth", false, true);
	connect_signal_mouse_left_click(
		*fullwidth_alpha_
		, std::bind(
			&tkeyboard::click_fullwidth
			, this, std::ref(*fullwidth_alpha_)));
	fullwidth_alpha_->set_sound("button.wav");
	fullwidth_alpha_->set_cookie(ALPHA_CATEGORY);

	tbutton* button = find_widget<tbutton>(&grid, "backspace", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tkeyboard::click_backspace
			, this, std::ref(grid)));
	button->set_label("misc/ime-backspace.png");
	button->set_sound("button.wav");
}

static std::string get_border_symbol(wchar_t code)
{
	return code >= '0' && code <= '9'? null_str: "textbox";
}

#define symbol_has_tow_code(key)	((key) >= 0x10000)

static std::string symbol_key_string(uint32_t key)
{
	uint32_t size = sizeof(size_t);
	if (!symbol_has_tow_code(key)) {
		return UCS2_to_UTF8(key);
	} else {
		return UCS2_to_UTF8(posix_hi16(key)) + UCS2_to_UTF8(posix_lo16(key));
	}
}

void tkeyboard::pre_symbol(tgrid& grid)
{
	std::stringstream id_ss;
	treport* report = find_widget<treport>(&grid, "report", false, true);
	for (int at = 0; at < e2c_symbols_; at ++) {
		const teng_2_chinese& e2c = *(e2c_symbol_table_ + at);
		uint32_t code = e2c.english;
		id_ss.clear();
		id_ss << "keyboard_symbol_" << at;
		tcontrol& widget = report->insert_item(id_ss.str(), symbol_key_string(code));
		widget.set_sound("button.wav");
		widget.set_border(get_border_symbol(code));
		widget.set_cookie(code);
	}
	report->set_did_item_click(std::bind(&tkeyboard::did_symbol_item_clicked, this, _1, _2, std::ref(grid)));

	fullwidth_symbol_ = find_widget<tbutton>(&grid, "fullwidth", false, true);
	connect_signal_mouse_left_click(
		*fullwidth_symbol_
		, std::bind(
			&tkeyboard::click_fullwidth
			, this, std::ref(*fullwidth_symbol_)));
	fullwidth_symbol_->set_sound("button.wav");
	fullwidth_symbol_->set_cookie(SYMBOL_CATEGORY);

	tbutton* button = find_widget<tbutton>(&grid, "left1", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tkeyboard::click_left_allow
			, this, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "right1", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tkeyboard::click_right_allow
			, this, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "backspace", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tkeyboard::click_backspace
			, this, std::ref(grid)));
	button->set_label("misc/ime-backspace.png");
	button->set_sound("button.wav");
}

void tkeyboard::pre_more(tgrid& grid)
{
	std::stringstream id_ss;
	treport* report = find_widget<treport>(&grid, "report", false, true);
	for (int at = 0; at < max_more_items_; at ++) {
		id_ss.clear();
		id_ss << "keyboard_more_" << at;
		tcontrol& widget = report->insert_item(id_ss.str(), null_str);
		// widget.set_sound("button.wav");
		widget.set_border(null_str);
		widget.set_border("textbox");
		widget.set_cookie(nposm);
	}
	report->set_did_item_click(std::bind(&tkeyboard::did_more_item_clicked, this, _1, _2, std::ref(grid)));

	tbutton* button = find_widget<tbutton>(&grid, "goback", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tkeyboard::click_goback_ime
			, this, std::ref(*button)));
	button->set_label("misc/ime-goback.png");
	button->set_cookie(MORE_PAGE);

	button = find_widget<tbutton>(&grid, "tone", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tkeyboard::click_more_tone
			, this, std::ref(*button)));
	button->set_cookie(IME_CATEGORY);
}

bool tkeyboard::equal_pinyin(const char* pinyin_table, int code, const std::string& pinyin) const
{
	code -= chinese::min_unicode;
	int size = pinyin.size();
	const char* c_str = pinyin.c_str();
	const char* ptr = pinyin_table + code * chinese::fix_bytes_in_code;
	int tmp = 0;
	while (tmp < size) {
		if (ptr[tmp] != c_str[tmp]) {
			return false;
		}
		tmp ++;
	}
	const char ch = ptr[tmp];
	return ch == '\0' || (ch >= '0' && ch <= '9');
}

void tkeyboard::pinyin_changed(tgrid& grid)
{
	VALIDATE(!english_ && (current_page_ == ALPHA_PAGE || current_page_ == MORE_PAGE), null_str);
	VALIDATE(current_category_ == IME_CATEGORY, null_str);
	VALIDATE(chinese_candidate_segments_.size() == pinyin_.size(), null_str);

	tlabel* pinyin_widget = find_widget<tlabel>(&grid, "pinyin", false, true);
	pinyin_widget->set_label(pinyin_);

	treport* chinese_line1 = find_widget<treport>(&grid, "chinese_line1", false, true);
	int line1_items = chinese_line1->items();
	VALIDATE(line1_items == max_chinese_line1_items_, null_str);

	int segment_start_at = nposm;
	int segment_count = 0;
	if (!chinese_candidate_segments_.empty()) {
		const std::pair<int, int>& last_segment = chinese_candidate_segments_.back();
		segment_start_at = last_segment.first;
		segment_count = last_segment.second;
	}

	int equals = 0;
	std::vector<int> codes;
	const tchinese_item* frequence_item = nullptr;
	if (segment_start_at != nposm) {				
		for (int at = 0; at < segment_count; at ++) {
			const int value = chinese_candidate_[segment_start_at + at];
			bool tone_satisfied = true;
			if (frequence_only_ && current_tone_ != tone_all) {
				frequence_item = chinese_frequence_ + value;
				if (frequence_item->flag & CHINESE_FLAG_TONE12) {
					if (current_tone_ == tone_34) {
						tone_satisfied = false;
					}
				} else if (current_tone_ == tone_12) {
					tone_satisfied = false;
				}
			}
			int code = (frequence_only_? (chinese_frequence_ + value)->at: value) + chinese::min_unicode;
			if (tone_satisfied) {
				codes.push_back(code);
			}
			if (equal_pinyin(pinyin_table_, code, pinyin_)) {
				equals ++;
			}
		}
	}

	std::string label;
	for (int at = 0; at < line1_items; at ++) {
		tcontrol& widget = chinese_line1->item(at);
		int code = nposm;
		label.clear();
		if (at < (int)codes.size()) {				
			code = codes[at];
			label = UCS2_to_UTF8(code);
		}
		widget.set_label(label);
		widget.set_cookie(code);
	}

	expand_->set_label(str_cast(equals));
	expand_->set_active(equals > 0);

	// set tone
	VALIDATE(tone_alpha_->get_visible() == twidget::VISIBLE, null_str);
	tone_alpha_->set_label(tone_labels_.find(current_tone_)->second);
}

void tkeyboard::category_changed()
{
	//
	// update relative fields base on language_ and current_category_.
	//

	// set current_page
	int desire_page = nposm;
	if (current_category_ == ALPHA_CATEGORY) {
		desire_page = ALPHA_PAGE;

	} else if (current_category_ == SYMBOL_CATEGORY) {
		desire_page = SYMBOL_PAGE;

	} else {
		VALIDATE(!english_, null_str);
		desire_page = ALPHA_PAGE;
	}

	if (desire_page != current_page_) {
		stack_->set_radio_layer(desire_page);
		current_page_ = desire_page;
	}

	// set line1_stack
	int desire_line1_layer = nposm;
	if (current_category_ == ALPHA_CATEGORY) {
		desire_line1_layer = LINE1_DIGIT;

	} else if (current_category_ == IME_CATEGORY) {
		VALIDATE(!english_, null_str);
		desire_line1_layer = LINE1_IME;
	}
	if (desire_line1_layer != nposm) {
		line1_stack_->set_radio_layer(desire_line1_layer);
	}

	// set expand widget
	std::string expand_label;
	bool expand_active = false;
	if (current_category_ == ALPHA_CATEGORY) {
		expand_label = expand_label_;
		expand_active = true;

	} else if (current_category_ == IME_CATEGORY) {
		expand_label = str_cast(0);
		expand_active = false;
	}
	if (!expand_label.empty()) {
		expand_->set_label(expand_label);
		expand_->set_active(expand_active);
	}

	// set tone
	if (current_category_ == ALPHA_CATEGORY) {
		capslock_widget_->set_visible(twidget::VISIBLE);
		tone_alpha_->set_visible(twidget::INVISIBLE);

	} else if (current_category_ == IME_CATEGORY) {
		capslock_widget_->set_visible(twidget::INVISIBLE);
		tone_alpha_->set_visible(twidget::VISIBLE);
		tone_alpha_->set_label(tone_labels_.find(current_tone_)->second);
	}

	// set fullwidth widget
	std::string fullwidth_label;
	if (english_) {
		fullwidth_label = _("ime^fullwidth");
	} else {
		fullwidth_label = ht::generate_format(_("ime^fullwidth"), color_to_uint32(font::GOOD_COLOR));
	}
	if (current_category_ == ALPHA_CATEGORY) {
		fullwidth_alpha_->set_label(fullwidth_label);
		fullwidth_alpha_->set_visible(twidget::VISIBLE);

	} else if (current_category_ == SYMBOL_CATEGORY) {
		fullwidth_symbol_->set_label(fullwidth_label);

	} else {
		fullwidth_alpha_->set_visible(twidget::INVISIBLE);
	}

	// space
	std::string space_label = _("Space");
	if (!english_) {
		space_label = _("ime^Chinese space");
	}
	space_->set_label(space_label);
	
	// set category_alpha_, category_symbol_, category_ime_ widget's appearance.
	std::string alpha_label, symbol_label, ime_label;
	if (current_category_ == ALPHA_CATEGORY) {
		VALIDATE(current_page_ == ALPHA_PAGE, null_str);
		alpha_label = ht::generate_format(category_labels_.find(ALPHA_CATEGORY)->second, color_to_uint32(font::GOOD_COLOR));

	} else if (current_category_ == SYMBOL_CATEGORY) {
		VALIDATE(current_page_ == SYMBOL_PAGE, null_str);
		symbol_label = ht::generate_format(category_labels_.find(SYMBOL_CATEGORY)->second, color_to_uint32(font::GOOD_COLOR));

	} else {
		VALIDATE(current_category_ == IME_CATEGORY, null_str);
		VALIDATE(current_page_ == ALPHA_PAGE || current_page_ == MORE_PAGE, null_str);
		VALIDATE(!english_, null_str);

		ime_label = ht::generate_format(category_labels_.find(IME_CATEGORY)->second, color_to_uint32(font::GOOD_COLOR));
	}

	category_alpha_->set_label(alpha_label.empty()? category_labels_.find(ALPHA_CATEGORY)->second: alpha_label);
	category_symbol_->set_label(symbol_label.empty()? category_labels_.find(SYMBOL_CATEGORY)->second: symbol_label);
	category_ime_->set_label(ime_label.empty()? category_labels_.find(IME_CATEGORY)->second: ime_label);
}

void tkeyboard::capslock_changed()
{
	tgrid& grid = *stack_->layer(ALPHA_PAGE);

	std::vector<std::string> line_ids;
	line_ids.push_back("line2");
	line_ids.push_back("line3");
	line_ids.push_back("line4");

	for (std::vector<std::string>::const_iterator it = line_ids.begin(); it != line_ids.end(); ++ it) {
		treport* line = find_widget<treport>(&grid, *it, false, true);
		int items = line->items();
		for (int at = 0; at < items; at ++) {
			tcontrol& widget = line->item(at);
			const int code = uint64_2_int(widget.cookie());
			VALIDATE(code >= 'a' && code <= 'z', null_str);
			widget.set_label(UCS2_to_UTF8(capslock_? code - ('a' - 'A'): code));
		}
	}
	tbutton* button = find_widget<tbutton>(&grid, "capslock", false, true);
	button->set_label(capslock_? "misc/ime-capslock-gray.png": "misc/ime-capslock.png");
}

void tkeyboard::rest_chinese_session()
{
	current_tone_ = tone_all;
	chinese_candidate_segments_.clear();
	pinyin_.clear();
}

#define ascii_to_fullwidth(key)	((key) + 0xff00 - ' ')

void tkeyboard::did_alpha_clicked(treport& report, tbutton& widget, tgrid& grid)
{
	VALIDATE(current_page_ == ALPHA_PAGE, null_str);
	int key = uint64_2_int(widget.cookie());
	if (key == nposm) {
		return;
	}
	if (english_ || current_category_ != IME_CATEGORY) {
		int adjusted_key = key;
		if (english_) {
			// key must be digit or letter.
			if (capslock_ && (key >= 'a' && key <= 'z')) {
				adjusted_key = key - ('a' - 'A');
			}
		} else {
			// key must be letter. line1 is used to show help.
			adjusted_key = ascii_to_fullwidth(key);
			if (capslock_ && (key >= 'a' && key <= 'z')) {
				adjusted_key = adjusted_key - ('a' - 'A');
			}
		}
		keyboard::input(UCS2_to_UTF8(adjusted_key));

	} else if (chinese::is_chinese(key)) {
		keyboard::input(UCS2_to_UTF8(key));
		rest_chinese_session();

	} else if (chinese_candidate_segments_.empty() || chinese_candidate_segments_.back().second != 0) {
		VALIDATE(key >= 'a' && key <= 'z', null_str);
		pinyin_ += UCS2_to_UTF8(key);
		int candidate_at = 0; // temperal index in chinese_candiate_
		if (chinese_candidate_segments_.empty()) {
			const int size = frequence_only_? frequence_size_: chinese::get_pinyin_code_bytes();
			int py_at; // at in pingyin_code.
			for (int i = 0; i < size; i ++) {
				if (frequence_only_) {
					py_at = (chinese_frequence_ + i)->at;
				} else {
					py_at = i;
				}
				if (pinyin_table_[py_at * chinese::fix_bytes_in_code] == key) {
					chinese_candidate_[candidate_at ++] = frequence_only_? i: py_at;
				}
			}
			chinese_candidate_segments_.push_back(std::make_pair(0, candidate_at));
		} else {
			const std::pair<int, int>& last_segment = chinese_candidate_segments_.back();
			const int start_candidate_at = last_segment.first + last_segment.second;
			candidate_at = start_candidate_at;
			const int which = chinese_candidate_segments_.size();
			for (int i = last_segment.first; i < start_candidate_at; i ++) {
				int code_from0 = frequence_only_? (chinese_frequence_ + chinese_candidate_[i])->at: chinese_candidate_[i];
				if (pinyin_table_[code_from0 * chinese::fix_bytes_in_code + which] == key) {
					chinese_candidate_[candidate_at ++] = frequence_only_? chinese_candidate_[i]: code_from0;
				}
			}
			chinese_candidate_segments_.push_back(std::make_pair(start_candidate_at, candidate_at - start_candidate_at));
		}
	} else {
		// current has input pinyin, but last is 0, and will discard late alpha.
		return;
	}
	if (!english_ && current_category_ == IME_CATEGORY) {
		pinyin_changed(grid);
	}
}

void tkeyboard::did_symbol_item_clicked(treport& report, tbutton& widget, tgrid& grid)
{
	VALIDATE(current_page_ == SYMBOL_PAGE, null_str);
	uint32_t key = uint64_2_int(widget.cookie());
	VALIDATE(key != nposm, null_str);

	keyboard::input(symbol_key_string(key));
	// The current setting is that when you enter a new character, the cursor automatically moves the next one. 
	// So it's no use moving one character left here.
}

void tkeyboard::did_more_item_clicked(treport& report, tbutton& widget, tgrid& grid)
{
	VALIDATE(current_page_ == MORE_PAGE, null_str);
	int key = size_t_2_int(widget.cookie());
	if (key == nposm) {
		return;
	}
	sound::play_UI_sound("button.wav");
	keyboard::input(UCS2_to_UTF8(key));

	stack_->set_radio_layer(ALPHA_PAGE);
	current_page_ = ALPHA_PAGE;

	rest_chinese_session();
	pinyin_changed(*stack_->layer(ALPHA_PAGE));
}

void tkeyboard::more_tone_changed()
{
	VALIDATE(frequence_only_, null_str);
	VALIDATE(!ime_more_indexs_.empty(), null_str);
	const tchinese_item* frequence_item = nullptr;

	std::vector<int> codes;
	for (std::vector<int>::const_iterator it = ime_more_indexs_.begin(); it != ime_more_indexs_.end(); ++ it) {
		const int value = *it;
		bool tone_satisfied = true;
		frequence_item = chinese_frequence_ + value;
		if (current_more_tone_ != tone_all) {
			if (frequence_item->flag & CHINESE_FLAG_TONE12) {
				if (current_more_tone_ == tone_34) {
					tone_satisfied = false;
				}
			} else if (current_more_tone_ == tone_12) {
				tone_satisfied = false;
			}
		}
		if (tone_satisfied) {
			int code = frequence_item->at + chinese::min_unicode;
			codes.push_back(code);
		}
	}
		
	tgrid& grid2 = *stack_->layer(MORE_PAGE);
	treport* report = find_widget<treport>(&grid2, "report", false, true);
	std::string label;
	for (int at = 0; at < max_more_items_; at ++) {
		tcontrol& widget = report->item(at);
		int code = nposm;
		label.clear();
		if (at < (int)codes.size()) {				
			code = codes[at];
			label = UCS2_to_UTF8(code);
		}
		widget.set_label(label);
		widget.set_cookie(code);
	}

	tbutton* button = find_widget<tbutton>(&grid2, "tone", false, true);
	button->set_label(tone_labels_.find(current_more_tone_)->second);
}

void tkeyboard::click_expand(tgrid& grid, tbutton& twidget)
{
	// more only support when frequence_only_ = true. so disable it temperal.
	VALIDATE(frequence_only_, null_str);

	if (english_ || current_category_ == ALPHA_CATEGORY) {
		keyboard::input(expand_label_);
	} else {
		VALIDATE(current_category_ == IME_CATEGORY, null_str);
		stack_->set_radio_layer(MORE_PAGE);
		current_page_ = MORE_PAGE;

		VALIDATE(!chinese_candidate_segments_.empty(), null_str);
		const std::pair<int, int>& last_segment = chinese_candidate_segments_.back();
		int segment_start_at = last_segment.first;
		int segment_count = last_segment.second;

		ime_more_indexs_.clear();
		current_more_tone_ = tone_all;
		const tchinese_item* frequence_item = nullptr;
		if (segment_start_at != nposm) {				
			for (int at = 0; at < segment_count; at ++) {
				const int value = chinese_candidate_[segment_start_at + at];
				int code = (frequence_only_? (chinese_frequence_ + value)->at: value) + chinese::min_unicode;
				if (equal_pinyin(pinyin_table_, code, pinyin_)) {
					ime_more_indexs_.push_back(frequence_only_? value: code);
				}
			}
		}

		if (frequence_only_) {
			more_tone_changed();
		} else {
			const std::vector<int>& codes = ime_more_indexs_;
		
			tgrid& grid2 = *stack_->layer(MORE_PAGE);
			treport* report = find_widget<treport>(&grid2, "report", false, true);
			std::string label;
			for (int at = 0; at < max_more_items_; at ++) {
				tcontrol& widget = report->item(at);
				int code = nposm;
				label.clear();
				if (at < (int)codes.size()) {				
					code = codes[at];
					label = UCS2_to_UTF8(code);
				}
				widget.set_label(label);
				widget.set_cookie(code);
			}
		}
	}
}

void tkeyboard::click_capslock(tgrid& grid, tbutton& widget)
{
	VALIDATE(current_page_ == ALPHA_PAGE, null_str);
	if (!english_) {
		VALIDATE(current_category_ == ALPHA_CATEGORY, null_str);
	}

	capslock_ = !capslock_;
	capslock_changed();
}

void tkeyboard::click_tone(tbutton& widget)
{
	VALIDATE(current_page_ == ALPHA_PAGE, null_str);
	VALIDATE(current_category_ == IME_CATEGORY, null_str);
	VALIDATE(!english_, null_str);

	current_tone_ ++;
	if (current_tone_ == tone_count) {
		current_tone_ = tone_all;
	}
	pinyin_changed(*stack_->layer(ALPHA_PAGE));
	tone_alpha_->set_label(tone_labels_.find(current_tone_)->second);
}

ttext_box* tkeyboard::get_keyboard_capture_text_box()
{
	VALIDATE(window_ != nullptr, null_str);
	twidget* focus = window_->keyboard_capture_widget();
	if (focus == nullptr || !focus->is_text_box()) {
		return nullptr;
	}
	return static_cast<ttext_box*>(focus);
}

void tkeyboard::click_left_allow(tbutton& widget)
{
	ttext_box* focus = get_keyboard_capture_text_box();
	if (focus == nullptr) {
		return;
	}

	bool handled = focus->handle_key_left_arrow(KMOD_NONE);
	if (handled) {
		sound::play_UI_sound("button.wav");
	}
}

void tkeyboard::click_right_allow(tbutton& widget)
{
	ttext_box* focus = get_keyboard_capture_text_box();
	if (focus == nullptr) {
		return;
	}

	bool handled = focus->handle_key_right_arrow(KMOD_NONE);
	if (handled) {
		sound::play_UI_sound("button.wav");
	}
}

void tkeyboard::click_backspace(tgrid& grid)
{
	if (english_ || current_page_ != ALPHA_PAGE || pinyin_.empty()) {
		keyboard::input(SDLK_BACKSPACE);
	} else {
		VALIDATE(chinese_candidate_segments_.size() == pinyin_.size(), null_str);
		pinyin_ = pinyin_.substr(0, pinyin_.size() - 1);
		std::vector<std::pair<int, int> >::iterator it = chinese_candidate_segments_.begin();
		std::advance(it, chinese_candidate_segments_.size() - 1);
		chinese_candidate_segments_.erase(it);
		pinyin_changed(grid);
	}
}

void tkeyboard::fullwidth_changed(int category, bool to_english)
{
	VALIDATE(category == ALPHA_CATEGORY || category == SYMBOL_CATEGORY, null_str);

	if (category == ALPHA_CATEGORY) {
		// don't udpate
	} else {
		tgrid& grid = *stack_->layer(SYMBOL_PAGE);
		treport* report = find_widget<treport>(&grid, "report", false, true);
		VALIDATE(report->items() == e2c_symbols_, null_str);
		for (int at = 0; at < e2c_symbols_; at ++) {
			tcontrol& widget = report->item(at);
			const uint32_t cookie_code = uint64_2_int(widget.cookie());
			const teng_2_chinese& e2c = *(e2c_symbol_table_ + at);
			uint32_t desire_code = e2c.english;
			if (to_english) {
				// preview is chinese
				VALIDATE(cookie_code == e2c.chinese, null_str);
			} else {
				// preview is english
				VALIDATE(cookie_code == e2c.english, null_str);
				desire_code = e2c.chinese;
			}
			widget.set_label(symbol_key_string(desire_code));
			widget.set_cookie(desire_code);
		}
	}
}

void tkeyboard::click_fullwidth(tbutton& widget)
{
	VALIDATE(current_category_ == uint64_2_int(widget.cookie()), null_str);
	VALIDATE(current_category_ == ALPHA_CATEGORY || current_category_ == SYMBOL_CATEGORY, null_str);
	VALIDATE(chinese_candidate_segments_.empty() && pinyin_.empty(), null_str);

	tgrid& grid = *stack_->layer(ALPHA_PAGE);
	english_ = !english_;
	fullwidth_changed(current_category_, english_);

	category_changed();
}

void tkeyboard::click_goback_ime(tbutton& widget)
{
	VALIDATE(current_page_ == MORE_PAGE, null_str);
	VALIDATE(current_page_ == uint64_2_int(widget.cookie()), null_str);

	stack_->set_radio_layer(ALPHA_PAGE);
	current_page_ = ALPHA_PAGE;
}

void tkeyboard::click_more_tone(tbutton& widget)
{
	VALIDATE(current_page_ == MORE_PAGE, null_str);
	VALIDATE(current_category_ == IME_CATEGORY, null_str);
	VALIDATE(!english_, null_str);

	current_more_tone_ ++;
	if (current_more_tone_ == tone_count) {
		current_more_tone_ = tone_all;
	}

	more_tone_changed();
}

void tkeyboard::click_category(tbutton& widget)
{
	int desire_category = uint64_2_int(widget.cookie());
	VALIDATE(category_labels_.count(desire_category) != 0, null_str);

	if (desire_category == current_category_) {
		return;
	}

	if (current_category_ == IME_CATEGORY) {
		// will change to other category, reset current tone to all.
		rest_chinese_session();
		pinyin_changed(*stack_->layer(ALPHA_PAGE));
	}

	if (!english_ && (current_category_ == ALPHA_CATEGORY || current_category_ == SYMBOL_CATEGORY)) {
		fullwidth_changed(current_category_, true);
	}

	bool require_change_language = false;
	if (desire_category == ALPHA_CATEGORY) {
		if (!english_) {
			require_change_language = true;
		}

	} else if (desire_category == SYMBOL_CATEGORY) {
		if (!english_) {
			require_change_language = true;
		}

	} else {
		if (english_) {
			require_change_language = true;
		}
	}

	if (require_change_language) {
		english_ = !english_;
	}

	current_category_ = desire_category;

	// change capslock to false
	if (capslock_) {
		capslock_ = false;
		capslock_changed();
	}

	category_changed();
}

void tkeyboard::click_space()
{
	int key = SDLK_SPACE;
	if (!english_) {
		key = ascii_to_fullwidth(key);
	}
	keyboard::input(UCS2_to_UTF8(key));
}

void tkeyboard::click_action()
{
}

void tkeyboard::did_shown(twindow& window)
{
	window_ = &window;
}

void tkeyboard::did_hidden()
{
	// same as click_language(widget), and must to english_ = true.
	if (current_category_ == IME_CATEGORY) {
		rest_chinese_session();
		pinyin_changed(*stack_->layer(ALPHA_PAGE));
	}
	if (!english_ && (current_category_ == ALPHA_CATEGORY || current_category_ == SYMBOL_CATEGORY)) {
		fullwidth_changed(current_category_, true);
	}

	bool language_changed = false;
	if (!english_) {
		english_ = !english_;
		language_changed = true;
	}
	
	// change capslock to false
	if (capslock_) {
		capslock_ = false;
		capslock_changed();
	}

	if (language_changed || current_category_ != ALPHA_CATEGORY) {
		current_category_ = ALPHA_CATEGORY;
		category_changed();
	}
	window_ = nullptr;
}

void tkeyboard::clear_texture()
{
	tex_buf = nullptr;
}

void tkeyboard::click_close()
{
	keyboard::set_visible(false, keyboard::REASON_CLOSE);
}

} // namespace gui2

