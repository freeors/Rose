/* $Id: mp_login.cpp 50955 2011-08-30 19:41:15Z mordante $ */
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

#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/tools.hpp"

#include "SDL_image.h"
#include "rose_config.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/browse.hpp"
#include "integrate.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "config_cache.hpp"
#include "tflite.hpp"
#include "font.hpp"

using namespace std::placeholders;
#include <iomanip>

#include <opencv2/imgproc.hpp>
#include "filesystem.hpp"

struct tcrop
{
	tcrop(const SDL_Rect& r, const std::string& name)
		: r(r)
		, name(name)
		, alphamask()
	{}

	SDL_Rect r;
	std::string name;
	std::string alphamask;
};

namespace gui2 {

REGISTER_DIALOG(studio, tools)

std::map<ttools::tstyle, std::string> ttools::styles;

ttools::ttools()
	: style_(style_nine)
	, cnftl_path_("C:/tensorflow/cnftl")
	, cnftl_generating_(false)
{
	cnftl_path_ = utils::normalize_path(cnftl_path_);
	utils::lowercase2(cnftl_path_);

	if (design_dir.empty()) {
		design_dir = game_config::preferences_dir + "/editor";
		input_png = "input.png";
	}

	if (styles.empty()) {
		styles.insert(std::make_pair(style_transition, "design/style-transition.png"));
		styles.insert(std::make_pair(style_stuff, "design/style-stuff.png"));
		styles.insert(std::make_pair(style_nine, "design/style-nine.png"));
	}
}

const size_t max_text_size = 30;

void ttools::pre_show()
{
	window_->set_label("misc/white_background.png");

	treport* navigation = find_widget<treport>(window_, "navigation", false, true);
	navigation->insert_item(null_str, _("Image"));
	navigation->insert_item(null_str, _("cnftl"));
	navigation->set_did_item_pre_change(std::bind(&ttools::did_navigation_pre_change, this, _2));
	navigation->set_did_item_changed(std::bind(&ttools::did_navigation_changed, this, _2));
	navigation->select_item(0);

	pre_image(*window_);
	pre_cnftl(*window_);
}

void ttools::pre_image(twindow& window)
{
	std::stringstream strstr;

	tgrid* layer = find_widget<tstack>(&window, "body_panel", false).layer(IMAGE_LAYER);

	ttrack* portrait = find_widget<ttrack>(layer, "portrait", false, true);
	portrait->set_label(null_str);
	portrait->set_did_draw(std::bind(&ttools::did_draw_showcase, this, _1, _2, _3));

	ttext_box* user_widget = find_widget<ttext_box>(layer, "first_input", false, true);
	user_widget->set_maximum_chars(max_text_size);

	user_widget = find_widget<ttext_box>(layer, "second_input", false, true);
	user_widget->set_maximum_chars(max_text_size);

	strstr.str("");
	tlabel* label = find_widget<tlabel>(layer, "work_directory", false, true);
	strstr << _("Work directory") << ": " << design_dir;
	label->set_label(strstr.str());
	// label->set_label("Work directory: /private/var/mobile/Containers/Data/Application/8F168B30-668A-487E-A384-D56872941D22/Documents/editor");

	strstr.str("");
	label = find_widget<tlabel>(layer, "remark", false, true);
	strstr << _("Input filename") << ": " << input_png;
	label->set_label(strstr.str());

	refresh_according_to_style(*layer);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(layer, "style", false)
		, std::bind(
		&ttools::style
		, this
		, std::ref(window)));
	find_widget<tbutton>(layer, "style", false).set_label(ht::generate_img(styles.find(style_)->second));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(layer, "execute", false)
		, std::bind(
		&ttools::execute
		, this
		, std::ref(window)));
}

void ttools::pre_cnftl(twindow& window)
{
	tgrid* layer = find_widget<tstack>(&window, "body_panel", false).layer(CNFTL_LAYER);
	
	find_widget<ttext_box>(layer, "cnftl_path", false, true)->set_active(false);

	tlistbox* list = find_widget<tlistbox>(layer, "fonts", false, true);
	list->enable_select(false);

	connect_signal_mouse_left_click(
			find_widget<tbutton>(layer, "browse", false)
			, std::bind(
				&ttools::browse_cnftl
				, this
				, std::ref(window)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(layer, "generate", false)
		, std::bind(
		&ttools::generate_cnftl
		, this
		, std::ref(window)));

	did_cnftl_changed();
}

void ttools::post_show()
{
}

bool ttools::did_navigation_pre_change(ttoggle_button& widget)
{
	if (cnftl_generating_) {
		return false;
	}
	return true;
}

void ttools::did_navigation_changed(ttoggle_button& widget)
{
	tstack& stack = find_widget<tstack>(window_, "body_panel", false);

	if (widget.at() == 0) {
		stack.set_radio_layer(0);

	} else {
		VALIDATE(widget.at() == 1, null_str);
		stack.set_radio_layer(1);
	}
}

void ttools::refresh_according_to_style(tgrid& image_layer)
{
	std::stringstream strstr;

	// first tag/input
	strstr.str("");
	if (style_ == style_transition) {
		strstr << _("Transition filename prefix(Include '-' if necessary)");
	} else if (style_ == style_stuff) {
		strstr << _("Stuff filename prefix(Include '-' if necessary)");
	} else if (style_ == style_nine) {
		strstr << _("Cropped filename prefix. full filename: <prefix>-topleft.png");
	}
	tlabel* label = find_widget<tlabel>(&image_layer, "first_tag", false, true);
	label->set_label(strstr.str());

	ttext_box* user_widget = find_widget<ttext_box>(&image_layer, "first_input", false, true);
	strstr.str("");
	if (style_ == style_transition) {
		strstr << "green-abrupt-";
	} else if (style_ == style_stuff) {
		strstr << "green-stuff-";
	} else if (style_ == style_nine) {
		strstr << "thermometer2";
	}
	user_widget->set_label(strstr.str());

	// second tag/input
	strstr.str("");
	label = find_widget<tlabel>(&image_layer, "second_tag", false, true);
	if (style_ == style_transition) {
		strstr << _("Terrain filename(Exclude '.png')");
		label->set_label(strstr.str());
		label->set_visible(twidget::VISIBLE);
	} else if (style_ == style_stuff) {
		label->set_visible(twidget::INVISIBLE);
	} else if (style_ == style_nine) {
		strstr << _("OFFSET_SIDE_1, OFFSET_SIDE_2, MID_W, MID_H");
		label->set_label(strstr.str());
		label->set_visible(twidget::VISIBLE);
	}

	user_widget = find_widget<ttext_box>(&image_layer, "second_input", false, true);
	strstr.str("");
	if (style_ == style_transition) {
		strstr << "green";
		user_widget->set_label(strstr.str());
		user_widget->set_visible(twidget::VISIBLE);
	} else if (style_ == style_stuff) {
		user_widget->set_visible(twidget::INVISIBLE);
	} else if (style_ == style_nine) {
		strstr << "16,16,64,64";
		user_widget->set_label(strstr.str());
		user_widget->set_visible(twidget::VISIBLE);
	}
}

std::string ttools::text_box_str(twindow& window, const std::string& id, const std::string& name, int min, int max, bool allow_empty)
{
	std::stringstream err;
	utils::string_map symbols;

	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->label();

	if (!allow_empty && str.empty()) {
		symbols["key"] = ht::generate_format(name, color_to_uint32(font::BAD_COLOR));
		
		err << vgettext2("Invalid '$key' value, not accept empty", symbols);
		gui2::show_message("", err.str());
		return str;
	} else if ((int)str.size() < min || (int)str.size() > max) {
		symbols["min"] = ht::generate_format(min, color_to_uint32(font::YELLOW_COLOR));
		symbols["max"] = ht::generate_format(max, color_to_uint32(font::YELLOW_COLOR));
		symbols["key"] = ht::generate_format(name, color_to_uint32(font::BAD_COLOR));
		
		if (min != max) {
			err << vgettext2("'$key' value must combine $min to $max characters", symbols);
		} else {
			err << vgettext2("'$key' value must be $min characters", symbols);
		}
		gui2::show_message("", err.str());
		return null_str;

	}
	return str;
}

bool is_absent_character(const surface& surf, const SDL_Rect& clip, wchar_t wch)
{
	if (clip.w <= 10 || clip.h <= 10) {
		return false;
	}
	if (wch == '1' || 1.0 * clip.w / clip.h >= 1) {
		// I think only 1 is bar.
		return false;
	}

	VALIDATE(surf.get(), null_str);
	const_surface_lock lock(surf);

	const int w = surf->w;
	const int h = surf->h;

	VALIDATE(clip.w >= 0 && clip.h >= 0 && clip.x + clip.w <= surf->w && clip.y + clip.h <= surf->h, null_str);

	const uint32_t absent_color = 0xff000000;
	for (int row = clip.y + 1; row < clip.y + clip.h - 1; row ++) {
		const uint32_t* data = lock.pixels() + row * w;
		for (int col = clip.x + 1; col < clip.x + clip.w - 1; col ++) {
			uint32_t value = data[col];
			if (value != absent_color) {
				return false;
			}
		}
	}
	return true;
}

// generate <font_path>/labels.txt base on <font_path>/chinese-3500.txt
void generate_labels_txt(const std::string& font_path)
{
	// Chinese punctuation. reference to  http://blog.chinaunix.net/uid-12348673-id-3335307.html
	std::set<wchar_t> label_chinese_punctuation;
	label_chinese_punctuation.insert(0x2014);
	label_chinese_punctuation.insert(0x2018);
	label_chinese_punctuation.insert(0x2019);
	label_chinese_punctuation.insert(0x201c);
	label_chinese_punctuation.insert(0x201d);
	label_chinese_punctuation.insert(0x2026);
	label_chinese_punctuation.insert(0x3001);
	label_chinese_punctuation.insert(0x3002);
	label_chinese_punctuation.insert(0x300a);
	label_chinese_punctuation.insert(0x300b);
	label_chinese_punctuation.insert(0x300e);
	label_chinese_punctuation.insert(0x300f);
	label_chinese_punctuation.insert(0x3010);
	label_chinese_punctuation.insert(0x3011);

	std::set<wchar_t> label_3500;
	{
		const std::string imagenet_comp_path = font_path + "/chinese-3500.txt";
		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch & 0x80) {
				int wch = posix_mku16(file.data[at], file.data[at + 1]);
				std::string str(file.data + at, 2);
				str = ansi_2_utf8(str);

				utils::utf8_iterator it(str);
				bool first = false;
				for (; it != utils::utf8_iterator::end(str); ++ it) {
					VALIDATE(!first, null_str);
					if (*it != 0x3000) {
						// may be exist chinese-space.
						label_3500.insert(*it);
					}
					first = true;
				}
				at ++;
			}
		}
		VALIDATE(label_3500.size() == 3500, null_str);
	}

	std::set<wchar_t> label_gb2312_extra;
	{
		// reference to http://ash.jp/code/cn/gb2312tbl.htm
		for (wchar_t wch = 0xb0a0; wch <= 0xd7f9; ) {
			if ((wch & 0xff) == 0xa0) {
				wch ++;
				continue;
			} else if ((wch & 0xff) == 0xff) {
				wch ++;
				wch += 0xa0;
				continue;
			}
			// uint8_t data[3] = {wch & 0xff, (wch & 0xff00) >> 8, '\0'};
			uint8_t data[3] = {(uint8_t)((wch & 0xff00) >> 8), (uint8_t)(wch & 0xff), '\0'};
			std::string str((char*)data, 2);
			str = ansi_2_utf8(str);

			utils::utf8_iterator it(str);
			bool first = false;
			for (; it != utils::utf8_iterator::end(str); ++ it) {
				VALIDATE(!first, null_str);
				if (!label_3500.count(*it)) {
					label_gb2312_extra.insert(*it);
				}
				first = true;
			}
			wch ++;
		}
		// master gb2312 has 3755 chinese. but maybe gb2312 exited but 3500 not, 
		// so label_gb2312_extra.size() maybe larger 3755 - 3500.
	}

	// wrtie to file
	tfile file(font_path + "/labels.txt", GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);
	std::stringstream fp_ss;

	const int chars_per_line = 8;

	fp_ss << "#\n";
	fp_ss << "# ascii section.\n";
	fp_ss << "#\n";
	int at = 0;
	for (wchar_t wch = 0x21; wch <= 0x7e; wch ++, at ++) {
		if (at && at % chars_per_line == 0) {
			fp_ss << "\n";
		} else if (at) {
			fp_ss << ",";
		}
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << wch;
	}
	fp_ss << "\n\n";


	fp_ss << "#\n";
	fp_ss << "# Chinese punctuation section.\n";
	fp_ss << "#\n";
	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_chinese_punctuation.begin(); it != label_chinese_punctuation.end(); ++ it, at ++) {
		if (at && at % chars_per_line == 0) {
			fp_ss << "\n";
		} else if (at) {
			fp_ss << ",";
		}
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << *it;
	}
	fp_ss << "\n\n";

	fp_ss << "#\n";
	fp_ss << "# 3500 chinese section. reference <Chinese curriculum standards (2011 Edition)>\n";
	fp_ss << "#\n";

	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_3500.begin(); it != label_3500.end(); ++ it, at ++) {
		if (at && at % chars_per_line == 0) {
			fp_ss << "\n";
		} else if (at) {
			fp_ss << ",";
		}
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << *it;
	}
	fp_ss << "\n\n";

	fp_ss << "#\n";
	fp_ss << "# master gb2312 extra chinese section. maybe large 255 = 3755 - 3500\n";
	fp_ss << "#\n";

	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_gb2312_extra.begin(); it != label_gb2312_extra.end(); ++ it, at ++) {
		if (at && at % chars_per_line == 0) {
			fp_ss << "\n";
		} else if (at) {
			fp_ss << ",";
		}
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << *it;
	}

	posix_fwrite(file.fp, fp_ss.str().c_str(), fp_ss.str().size());
}

void ttools::did_cnftl_changed()
{
	tgrid* layer = find_widget<tstack>(window_, "body_panel", false).layer(CNFTL_LAYER);

	find_widget<ttext_box>(layer, "cnftl_path", false, true)->set_label(cnftl_path_);

	config fonts_cfg;
	{
		config_cache_transaction transaction;
		config_cache& cache = config_cache::instance();
		cache.clear_defines();

		cache.get_config(cnftl_path_ + "/fonts/fonts.cfg", fonts_cfg);
	}

	fonts_.clear();
	BOOST_FOREACH (const config& c, fonts_cfg.child_range("font")) {
		const std::string& name = c["name"].str();
		VALIDATE(!name.empty(), "[font] must exist name attribute.");

		fonts_.push_back(std::unique_ptr<tfont>(new tfont(name, c["ascii"].to_bool(), c["chinese"].to_bool())));
	}

	std::stringstream remark_ss;
	tlabel* remark = find_widget<tlabel>(layer, "summary", false, true);
	if (!fonts_.empty()) {
		remark_ss << "There is " << fonts_.size() << " fonts.";
	}
	remark->set_label(remark_ss.str());

	std::map<std::string, std::string> data;
	tlistbox* list = find_widget<tlistbox>(layer, "fonts", false, true);
	list->clear();
	for (std::vector<std::unique_ptr<tfont> >::const_iterator it = fonts_.begin(); it != fonts_.end(); ++ it) {
		const tfont& font = *it->get();
		data["name"] = font.name;
		ttoggle_panel& row = list->insert_row(data);

		const int size = cfg_2_os_size(24);
		std::stringstream x_ss, y_ss;
		x_ss << "(width - " << size << ")/2";
		y_ss << "(height - " << size << ")/2";
		if (font.ascii) {
			tcontrol* ascii = dynamic_cast<tcontrol*>(row.find("ascii", false));
			ascii->set_blits(tformula_blit("misc/ok.png", x_ss.str(), y_ss.str(), str_cast(size), str_cast(size)));
		}
		if (font.chinese) {
			tcontrol* chinese = dynamic_cast<tcontrol*>(row.find("chinese", false));
			chinese->set_blits(tformula_blit("misc/ok.png", x_ss.str(), y_ss.str(), str_cast(size), str_cast(size)));
		}
	}

	bool valid = !fonts_.empty();
	find_widget<tbutton>(layer, "generate", false).set_active(valid);
}

void ttools::browse_cnftl(twindow& window)
{
	std::string desire_dir;
	{
		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_DIR, true, null_str, _("Choose a cnftl Directory"));
		gui2::tbrowse dlg(param, false);
		dlg.show();
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		desire_dir = utils::normalize_path(param.result);
		utils::lowercase2(desire_dir);
	}
	if (desire_dir == cnftl_path_) {
		return;
	}
	
	cnftl_path_ = desire_dir;
	did_cnftl_changed();
}

void ttools::generate_cnftl(twindow& window)
{
	thread_->Start();
}

void ttools::handle_font(const tfont& font, const bool started, const int at, const bool ret)
{
	tgrid* layer = find_widget<tstack>(window_, "body_panel", false).layer(CNFTL_LAYER);
	tlistbox& list = find_widget<tlistbox>(layer, "fonts", false);
	std::stringstream ss;
	std::string str;
	
	ttoggle_panel& row = list.row_panel(at);

	if (!started) {
		ss.str("");
		ss << font.absent_chars.size();
		const int max_show_absent_chars = 20;
		int at = 0;
		for (std::map<int, wchar_t>::const_iterator it = font.absent_chars.begin(); it != font.absent_chars.end() && at < max_show_absent_chars; ++ it, at ++) {
			std::string text = UCS2_to_UTF8(it->second);
			ss << " [" << std::setbase(10) << it->first << "(" << text << ")";
			ss << "]";
		}
		if (font.absent_chars.size() > max_show_absent_chars) {
			ss << "...";
		}
		dynamic_cast<tcontrol*>(row.find("absent", false))->set_label(ss.str());
	}

	tcontrol* status = find_widget<tcontrol>(&row, "status", false, true);
	if (started) {
		str = ret? "misc/process.png": "misc/alert.png";
	} else {
		str = ret? "misc/ok.png": "misc/alert.png";
	}
	status->set_label(str);
}

void ttools::DoWork()
{
	const std::string font_path = cnftl_path_ + "/fonts";
	const std::string classify = "train";
	const std::string dest_path = cnftl_path_ + "/" + classify;
	std::stringstream dest_ss, unicode_ss;
	const int font_size = 64;
	const int remark_font_size = 14;

	// generate_labels_txt(font_path);
	std::set<wchar_t> label_strings = tflite::load_labels_txt_unicode(cnftl_path_ + "/labels.txt");

	SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGB888);
	int font_at = 0;
	for (std::vector<std::unique_ptr<tfont> >::const_iterator it = fonts_.begin(); it != fonts_.end(); ++ it, font_at ++) {
		tfont& font_desc = *it->get();
		const std::string file_name = font_path + "/" + font_desc.name;
		TTF_Font* font = TTF_OpenFont(file_name.c_str(), font_size);
		if (font == nullptr) {
			main_->Invoke<void>(RTC_FROM_HERE, rtc::Bind(&ttools::handle_font, this, font_desc, true, font_at, false));
			continue;
		}
		
		const int elem_size = 48;
		const int grid_height = 72;
		const int chars_per_line = 32;
		surface paper = create_neutral_surface(chars_per_line * elem_size, 125 * grid_height);
		fill_surface(paper, 0xffffffff);

		SDL_Rect paper_clip_rect {0, 0, elem_size, elem_size};

		main_->Invoke<void>(RTC_FROM_HERE, rtc::Bind(&ttools::handle_font, this, font_desc, true, font_at, true));

		int at = 0;
		for (std::set<wchar_t>::const_iterator it2 = label_strings.begin(); it2 != label_strings.end(); ++ it2, at ++) {
			const wchar_t wch = *it2;
			std::string text = UCS2_to_UTF8(wch);

			surface surf = surface(TTF_RenderUTF8_Blended(font, text.c_str(), uint32_to_color(0xff000000)));
			// clip surf.

			const SDL_Rect fg_region = calculate_max_foreground_region(surf, 0x0);
			// som fonts, if this char not exist, will be all transparent.
			bool absent = fg_region.w == -1;
			if (!absent) {
				VALIDATE(fg_region.h != -1, null_str);

				surface result = create_neutral_surface(fg_region.w, fg_region.h);
				fill_surface(result, 0xffffffff);
				sdl_blit(surf, &fg_region, result, nullptr);

				// som fonts, if this char not exist, will be black bar.
				if (!is_absent_character(surf, fg_region, wch)) {
					//
					// convert result to rbg888 and save to file.
					//
					result = SDL_ConvertSurface(result, format, 0);

					dest_ss.str("");
					dest_ss << dest_path << "/" << std::setfill('0') << std::setw(5) << at;
					SDL_MakeDirectory(dest_ss.str().c_str());

					dest_ss << "/" << font_desc.name << ".png";
					IMG_SavePNG(result, dest_ss.str().c_str());

				} else {
					font_desc.absent_chars.insert(std::make_pair(at, wch));
				}

			} else {
				font_desc.absent_chars.insert(std::make_pair(at, wch));
			}

			//
			// insert this character to wall.
			// 
			tpoint size = calculate_adaption_ratio_size(paper_clip_rect.w, paper_clip_rect.h, surf->w, surf->h);
			SDL_Rect clip_rect = paper_clip_rect;
			clip_rect.x += (paper_clip_rect.w - size.x) / 2;
			clip_rect.y += (paper_clip_rect.h - size.y) / 2;
			clip_rect.w = size.x;
			clip_rect.h = size.y;
			surf = scale_surface(surf, clip_rect.w, clip_rect.h);

			// draw character
			sdl_blit(surf, nullptr, paper, &clip_rect);

			// draw index
			surf = font::get_rendered_text(str_cast(at), 0, remark_font_size, uint32_to_color(0xff000000));
			clip_rect.x = paper_clip_rect.x + (paper_clip_rect.w - surf->w) / 2;
			clip_rect.y = paper_clip_rect.y + paper_clip_rect.h - font_size / 8;
			clip_rect.w = surf->w;
			clip_rect.h = surf->h;
			sdl_blit(surf, nullptr, paper, &clip_rect);

			// draw unicode
			unicode_ss.str("");
			unicode_ss << std::setbase(16) << std::setw(4) << std::setfill('0') << std::setiosflags(std::ios::uppercase) << wch;
			surf = font::get_rendered_text(unicode_ss.str(), 0, remark_font_size, uint32_to_color(0xff000000));
			clip_rect.x = paper_clip_rect.x + (paper_clip_rect.w - surf->w) / 2;
			clip_rect.y += (grid_height - paper_clip_rect.h) / 2;
			clip_rect.w = surf->w;
			clip_rect.h = surf->h;
			sdl_blit(surf, nullptr, paper, &clip_rect);

			paper_clip_rect.x += paper_clip_rect.w;
			if (paper_clip_rect.x == chars_per_line * paper_clip_rect.w) {
				paper_clip_rect.x = 0;
				paper_clip_rect.y += grid_height;
				// draw horizontal line.
				draw_line(paper, 0xff000000, 0, paper_clip_rect.y, paper->w, paper_clip_rect.y);
			}
		}
		TTF_CloseFont(font);

		// save character wall.
		dest_ss.str("");
		dest_ss << cnftl_path_ << "/" << classify << "-" << font_desc.name << "-wall.png";
		paper = SDL_ConvertSurface(paper, format, 0);
		IMG_SavePNG(paper, dest_ss.str().c_str());

		main_->Invoke<void>(RTC_FROM_HERE, rtc::Bind(&ttools::handle_font, this, font_desc, false, font_at, true));
	}
	SDL_FreeFormat(format);
}

void ttools::OnWorkStart()
{
	find_widget<tbutton>(window_, "ok", false).set_active(false);

	tgrid* layer = find_widget<tstack>(window_, "body_panel", false).layer(CNFTL_LAYER);
	find_widget<tbutton>(layer, "generate", false).set_active(false);

	tlistbox& list = find_widget<tlistbox>(layer, "fonts", false);
	int rows = list.rows();
	for (int at = 0; at < rows; at ++) {
		ttoggle_panel& row = list.row_panel(at);

		tcontrol* status = dynamic_cast<tcontrol*>(row.find("status", false));
		status->set_label(null_str);

		tcontrol* absent = dynamic_cast<tcontrol*>(row.find("absent", false));
		absent->set_label(null_str);
	}

	for (std::vector<std::unique_ptr<tfont> >::const_iterator it = fonts_.begin(); it != fonts_.end(); ++ it) {
		tfont& font_desc = *it->get();
		font_desc.absent_chars.clear();
	}

	cnftl_generating_ = true;
}

void ttools::OnWorkDone()
{
	find_widget<tbutton>(window_, "ok", false).set_active(true);

	tgrid* layer = find_widget<tstack>(window_, "body_panel", false).layer(CNFTL_LAYER);
	find_widget<tbutton>(layer, "generate", false).set_active(true);

	cnftl_generating_ = false;
}

void ttools::style(twindow& window)
{
	std::vector<std::string> items;
	std::vector<int> values;

	int initial_at = nposm;
	for (std::map<tstyle, std::string>::const_iterator it = styles.begin(); it != styles.end(); ++ it) {
		items.push_back(ht::generate_img(it->second));
		values.push_back(it->first);
		if (values.back() == style_) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	const tstyle style = (tstyle)values[dlg.cursel()];
	if (style == style_) {
		return;
	}
	style_ = style;

	tcontrol* label = find_widget<tcontrol>(&window, "style", false, true);
	label->set_label(ht::generate_img(styles.find(style_)->second));

	tgrid* layer = find_widget<tstack>(&window, "body_panel", false).layer(IMAGE_LAYER);

	refresh_according_to_style(*layer);
}

void ttools::did_draw_showcase(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	VALIDATE(bg_drawn, null_str);

	const int xsrc = widget_rect.x;
	const int ysrc = widget_rect.y;
	SDL_Rect dst = empty_rect;

	int offset_x, offset_y = 0;

	const std::string input_png2 = design_dir + "/" + input_png;
	surface surf = image::get_image(input_png2);
	if (!surf) {
		surf = font::get_rendered_text(_("Can not find input.png"), 0, 12 * twidget::hdpi_scale, font::BLACK_COLOR);
	}
	offset_x = (widget_rect.w - surf->w) / 2;
	offset_y = (widget_rect.h - surf->h) / 2;
	dst = ::create_rect(xsrc + offset_x, ysrc + offset_y, surf->w, surf->h);
	SDL_Renderer* renderer = get_renderer();
	render_surface(renderer, surf, NULL, &dst);
}

void ttools::execute(twindow& window)
{
	std::string first_str = text_box_str(window, "first_input", _("First edit box input"), 2, 28, false);
	if (first_str.empty()) {
		return;
	}

	std::string second_str = text_box_str(window, "second_input", _("Second edit box input"), 2, 19, false);
	
	std::vector<tcrop> items;
	const std::string input_png2 = design_dir + "/" + input_png;
	surface surf = image::get_image(input_png2);
	if (!surf) {
		return;
	}

	if (style_ == style_transition) {
		if (surf->w != 180 && surf->h != 216) {
			gui2::show_message("", "input png must be 180x216");
			return;
		}
		if (second_str.empty()) {
			return;
		}
		const std::string transiton_prefix = design_dir + "/" + first_str;
		const std::string base = design_dir + "/" + second_str + ".png";

		items.push_back(tcrop(::create_rect(54, 0, 72, 72), transiton_prefix + "s.png"));
		items.push_back(tcrop(::create_rect(108, 36, 72, 72), transiton_prefix + "sw.png"));
		items.push_back(tcrop(::create_rect(108, 108, 72, 72), transiton_prefix + "nw.png"));
		items.push_back(tcrop(::create_rect(54, 144, 72, 72), transiton_prefix + "n.png"));
		items.push_back(tcrop(::create_rect(0, 108, 72, 72), transiton_prefix + "ne.png"));
		items.push_back(tcrop(::create_rect(0, 36, 72, 72), transiton_prefix + "se.png"));
		items.push_back(tcrop(::create_rect(54, 72, 72, 72), base));

	} else if (style_ == style_stuff) {
		if (surf->w != 300 && surf->h != 300) {
			gui2::show_message("", "input png must be 300x300");
			return;
		}
		const std::string stuff_prefix = design_dir + "/" + first_str;

		items.push_back(tcrop(::create_rect(0, 0, -1, -1), stuff_prefix + "n.png"));
		items.back().alphamask = "design/alphamask-n.png";
		items.push_back(tcrop(::create_rect(0, 0, -1, -1), stuff_prefix + "ne.png"));
		items.back().alphamask = "design/alphamask-ne.png";
		items.push_back(tcrop(::create_rect(0, 0, -1, -1), stuff_prefix + "se.png"));
		items.back().alphamask = "design/alphamask-se.png";
		items.push_back(tcrop(::create_rect(0, 0, -1, -1), stuff_prefix + "s.png"));
		items.back().alphamask = "design/alphamask-s.png";
		items.push_back(tcrop(::create_rect(0, 0, -1, -1), stuff_prefix + "sw.png"));
		items.back().alphamask = "design/alphamask-sw.png";
		items.push_back(tcrop(::create_rect(0, 0, -1, -1), stuff_prefix + "nw.png"));
		items.back().alphamask = "design/alphamask-nw.png";

	} else if (style_ == style_nine) {
		std::vector<std::string> vstr = utils::split(second_str);
		if (vstr.size() != 4) {
			return;
		}
		int corner_width = lexical_cast_default<int>(vstr[0]);
		int corner_height = lexical_cast_default<int>(vstr[1]);
		int mid_width = lexical_cast_default<int>(vstr[2]);
		int mid_height = lexical_cast_default<int>(vstr[3]);

		if (corner_width <= 0 || corner_height <= 0 || mid_width <= 0 || mid_height < 0) {
			return;
		}
		if (2 * corner_width + mid_width > surf->w || 2 * corner_height + mid_height > surf->h) {
			return;
		}
		const std::string transiton_prefix = design_dir + "/" + first_str;
		items.push_back(tcrop(::create_rect(0, 0, corner_width, corner_height), transiton_prefix + "_topleft.png"));
		items.push_back(tcrop(::create_rect(surf->w - corner_width, 0, corner_width, corner_height), transiton_prefix + "_topright.png"));
		items.push_back(tcrop(::create_rect(0, surf->h - corner_height, corner_width, corner_height), transiton_prefix + "_botleft.png"));
		items.push_back(tcrop(::create_rect(surf->w - corner_width, surf->h - corner_height, corner_width, corner_height), transiton_prefix + "_botright.png"));
		items.push_back(tcrop(::create_rect(corner_width, 0, mid_width, corner_height), transiton_prefix + "_top.png"));
		items.push_back(tcrop(::create_rect(corner_width, surf->h - corner_height, mid_width, corner_height), transiton_prefix + "_bottom.png"));
		if (mid_height) {
			items.push_back(tcrop(::create_rect(0, corner_height, corner_width, mid_height), transiton_prefix + "_left.png"));
			items.push_back(tcrop(::create_rect(surf->w - corner_width, corner_height, corner_width, mid_height), transiton_prefix + "_right.png"));
			items.push_back(tcrop(::create_rect(corner_width, corner_height, mid_width, mid_height), transiton_prefix + "_middle.png"));
		}
	}

	for (std::vector<tcrop>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const tcrop& crop = *it;
		surface middle;
		if (crop.r.w > 0) {
			middle = cut_surface(surf, crop.r);
		} else {
			middle = surf;
		}
		if (style_ != style_nine) {
			surface masksurf = crop.alphamask.empty()? image::get_hexmask(): image::get_image(crop.alphamask);
			middle = mask_surface(middle, masksurf);
		}
		IMG_SavePNG(middle, crop.name.c_str());
	}

	gui2::show_message("", "Execute finished!");
}

} // namespace gui2

