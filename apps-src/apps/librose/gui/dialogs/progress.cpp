/* $Id$ */
/*
   Copyright (C) 2011 Sergey Popov <loonycyborg@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/progress.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "base_instance.hpp"

using namespace std::placeholders;

#include "font.hpp"

namespace gui2 {

REGISTER_DIALOG(rose, progress)

tprogress_* tprogress_::instance = nullptr;

tprogress_util::tprogress_util(twindow& window, twidget& widget, tprogress_::tslot& slot, const std::string& message, int initial_percentage)
	: window_(window)
	, widget_(*dynamic_cast<ttrack*>(&widget))
	, slot_(slot)
	, message_(message)
	, percentage_(initial_percentage)

{
	VALIDATE(percentage_ >= PROGRESS_MIN_PERCENTAGE && percentage_ <= PROGRESS_MAX_PERCENTAGE, null_str);
	widget_.set_did_draw(std::bind(&tprogress_util::did_draw_bar, this, _1, _2, _3));
}

tprogress_util::~tprogress_util()
{
}

bool tprogress_util::set_percentage(int percentage)
{
	if (percentage < PROGRESS_MIN_PERCENTAGE) {
		percentage = PROGRESS_MIN_PERCENTAGE;
	} else if (percentage > PROGRESS_MAX_PERCENTAGE) {
		percentage = PROGRESS_MAX_PERCENTAGE;
	}

	if (percentage == percentage_) {
		return false;
	}

	percentage_ = percentage;

	if (widget_.get_visible() == twidget::VISIBLE) {
		widget_.immediate_draw();
	}
	return true;
}

bool tprogress_util::set_message(const std::string& message)
{
	if (message == message_) {
		return false;
	}

	message_ = message;

	if (widget_.get_visible() == twidget::VISIBLE) {
		widget_.immediate_draw();
	}
	return true;
}

void tprogress_util::did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	if (window_.get_visible() != twidget::VISIBLE) {
		return;
	}

	SDL_Renderer* renderer = get_renderer();
	slot_.did_draw_bar(widget, widget_rect, percentage_, message_);
}

tprogress::tprogress(tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, int best_width, int best_height)
	: slot_(slot)
	, title_(title)
	, message_(message)
	, start_ticks_(SDL_GetTicks())
	, hidden_ticks_(hidden_ms)
	, require_cancel_(false)
	, track_best_width_(best_width)
	, track_best_height_(best_height)
{
	VALIDATE(slot.did_first_drawn, null_str);

	VALIDATE(tprogress_::instance == nullptr, "only allows a maximum of one tprogress");
	tprogress_::instance = this;
}

tprogress::~tprogress()
{
	VALIDATE(!timer_.valid(), null_str);
	tprogress_::instance = nullptr;
}

void tprogress::pre_show()
{
	if (!title_.empty()) {
		tlabel* label = find_widget<tlabel>(window_, "title", false, false);
		label->set_label(title_);
	} else {
		window_->set_margin(0, 0, 0, 0);
		window_->set_border(null_str);
	}

	const int initial_percentage = hidden_ticks_ == 0? PROGRESS_MIN_PERCENTAGE: PROGRESS_MAX_PERCENTAGE;
	progress_.reset(new tprogress_util(*window_, find_widget<ttrack>(window_, "_progress", false), slot_, message_, initial_percentage));
	progress_->track().set_best_size_1th(track_best_width_, progress_->track().get_width_is_max(), track_best_height_, progress_->track().get_height_is_max());

	tbutton* button = find_widget<tbutton>(window_, "_cancel", false, false);
	if (!slot_.has_cancel()) {
		button->set_visible(twidget::INVISIBLE);
	} else {
		button->set_label(slot_.cancel_img());
	}

	connect_signal_mouse_left_click(
		*button
		, std::bind(
		&tprogress::click_cancel
		, this));

	if (hidden_ticks_ != 0) {
		window_->set_visible(twidget::INVISIBLE);
	}
}

void tprogress::post_show()
{
}

void tprogress::app_first_drawn()
{
	bool ret = slot_.did_first_drawn(*this);
	VALIDATE(!window_->is_closing(), null_str);

	if (ret) {
		VALIDATE(!require_cancel_, null_str);
		window_->set_retval(twindow::OK);
	} else {
		window_->set_retval(twindow::CANCEL);
	}
}

void tprogress::timer_handler(bool render_track)
{
	if (window_->get_visible() != twidget::VISIBLE && SDL_GetTicks() - start_ticks_ >= hidden_ticks_) {
		window_->set_visible(twidget::VISIBLE);
		hidden_ticks_ = UINT32_MAX;
	}

	if (render_track && window_->get_visible() == twidget::VISIBLE) {
		progress_->track().timer_handler();
	}
}

void tprogress::set_percentage(int percentage)
{
	if (percentage < PROGRESS_MIN_PERCENTAGE) {
		percentage = PROGRESS_MIN_PERCENTAGE;
	} else if (percentage > PROGRESS_MAX_PERCENTAGE) {
		percentage = PROGRESS_MAX_PERCENTAGE;
	}

	bool changed = progress_->set_percentage(percentage);
	if (changed) {
		timer_handler(false);
		window_->show_slice();
	}
}

void tprogress::set_message(const std::string& message)
{
	bool changed = progress_->set_message(message);
	if (changed) {
		timer_handler(false);
		window_->show_slice();
	}
}

void tprogress::cancel_task()
{
	VALIDATE(!require_cancel_, null_str);
	require_cancel_ = true;
}

void tprogress::show_slice()
{ 
	timer_handler(true);
	window_->show_slice();
	if (!require_cancel_ && ::instance->terminating()) {
		cancel_task();
	}
}

bool tprogress::is_visible() const
{
	return window_->get_visible() == twidget::VISIBLE;
}

void tprogress::click_cancel()
{
	if (!require_cancel_) {
		cancel_task();
	}
}

bool run_with_progress_dlg(tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, const SDL_Rect& rect)
{
	gui2::tprogress dlg(slot, title, message, hidden_ms, rect.w, rect.h);
	dlg.show(rect.x, rect.y);

	return dlg.get_retval() == twindow::OK;
}

//
// tprogress_widget
//
tprogress_widget::tprogress_widget(twindow& window, tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, const SDL_Rect& rect)
	: window_(window)
	, float_track_(window.float_track())
	, slot_(slot)
	, message_(message)
	, hidden_ticks_(hidden_ms)
	, start_ticks_(SDL_GetTicks())
	, require_cancel_(false)
	, track_direct_rect_(rect)
	, maybe_cancel_(false)
	, last_frameTexture_size_(SDL_Point{nposm, nposm})
{
	VALIDATE(slot_.did_first_drawn, null_str);

	VALIDATE(!float_track_.is_visible(), null_str);
	VALIDATE(tprogress_::instance == nullptr, "only allows a maximum of one tprogress");
	tprogress_::instance = this;

	pre_show();
}

tprogress_widget::~tprogress_widget()
{
	float_track_.set_visible(false);
	tprogress_::instance = nullptr;
}

void tprogress_widget::pre_show()
{
	const int initial_percentage = hidden_ticks_ == 0? PROGRESS_MIN_PERCENTAGE: PROGRESS_MAX_PERCENTAGE;
	progress_.reset(new tprogress_util(window_, *float_track_.widget.get(), slot_, message_, initial_percentage));

	ttrack& track = progress_->track();
	set_track_rect();

	// if set need_layut to true, old dirty background may appear after be invisible.
	// reference to "canvas = widget.get_canvas_tex();" in twindow::draw_float_widgets().
	float_track_.need_layout = true;

	track.set_did_left_button_down(std::bind(&tprogress_widget::did_left_button_down, this, _1, _2));
	track.set_did_mouse_motion(std::bind(&tprogress_widget::did_mouse_motion_paper, this, _1, _2, _3));
	track.set_did_mouse_leave(std::bind(&tprogress_widget::did_mouse_leave_paper, this, _1, _2, _3));

	if (hidden_ticks_ != 0) {
		float_track_.set_visible(false);
	} else {
		set_visible();
	}
}

void tprogress_widget::did_left_button_down(ttrack& widget, const tpoint& coordinate)
{
	VALIDATE(!maybe_cancel_, null_str);
	if (!slot_.has_cancel()) {
		return;
	}
	
	const SDL_Rect rect = slot_.get_cancel_rect(widget.get_rect());
	if (point_in_rect(coordinate.x, coordinate.y, rect)) {
		maybe_cancel_ = true;
	}
}

void tprogress_widget::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}
	if (maybe_cancel_) {
		const SDL_Rect rect = slot_.get_cancel_rect(widget.get_rect());
		maybe_cancel_ = point_in_rect(last.x, last.y, rect);

		if (maybe_cancel_) {
			SDL_Point delta{last.x - first.x, last.y - first.y};
			const int threshold = 3 * twidget::hdpi_scale; // moving_bar_->size / 4
			maybe_cancel_ = posix_abs(delta.x) <= threshold && posix_abs(delta.y) <= threshold;
		}
	}
}

void tprogress_widget::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	const bool cancel = maybe_cancel_;
	maybe_cancel_ = false;
	if (is_null_coordinate(last)) {
		return;
	}

	if (!require_cancel_) {
		if (cancel) {
			cancel_task();
		}
	}
}

void tprogress_widget::timer_handler(bool render_track)
{
	if (!float_track_.is_visible() && SDL_GetTicks() - start_ticks_ >= hidden_ticks_) {
		set_visible();
		hidden_ticks_ = UINT32_MAX;
	}

	if (render_track && float_track_.is_visible()) {
		progress_->track().timer_handler();
	}
}

void tprogress_widget::set_percentage(int percentage)
{
	if (percentage < PROGRESS_MIN_PERCENTAGE) {
		percentage = PROGRESS_MIN_PERCENTAGE;
	} else if (percentage > PROGRESS_MAX_PERCENTAGE) {
		percentage = PROGRESS_MAX_PERCENTAGE;
	}

	bool changed = progress_->set_percentage(percentage);
	if (changed) {
		timer_handler(false);
		absolute_draw_float_widgets();
	}
}

void tprogress_widget::set_message(const std::string& message)
{
	bool changed = progress_->set_message(message);
	if (changed) {
		timer_handler(false);
		absolute_draw_float_widgets();
	}
}

void tprogress_widget::cancel_task()
{
	VALIDATE(!require_cancel_, null_str);
	require_cancel_ = true;
}

void tprogress_widget::show_slice()
{
	timer_handler(true);
	if (false) {
		// Must not call window_.show_slice()! it will process event, for example mouse.
		// why? 1)click result run progress_widget. 
		// 2)click it again, will result run progress_widget agin, but first is running. throw exception.
		// ==>Base rule: during run_with_progress_xxx, must not process any event.
		{
			SDL_Event temp_event;
			while (SDL_PollEvent(&temp_event)) {}
		}

		// why not useab solute_draw()?
		// --twindow::draw will call ttrack::did_draw_, did_draw_ may run_with_progress_widget or user's app logic,
		//   and result dead-loop or exception.
		absolute_draw_float_widgets();

		// Add a delay so we don't keep spinning if there's no event.
		SDL_Delay(10);

	} else {
		twindow::tunique_event_widget_lock lock(window_, *float_track_.widget.get());
		twidget::tdisable_popup_window_lock lock2("Must not popup new window during progress.show_slice().");
		// window_.show_slice();
		events::pump();
		absolute_draw_float_widgets();

		// Add a delay so we don't keep spinning if there's no event.
		SDL_Delay(10);
	}
	if (!require_cancel_ && ::instance->terminating()) {
		cancel_task();

	} else if (last_frameTexture_size_.x != gui2::settings::screen_width || last_frameTexture_size_.y != gui2::settings::screen_height) {
		set_track_rect();
	}
}

bool tprogress_widget::is_visible() const
{
	return float_track_.is_visible();
}

void tprogress_widget::set_visible()
{
	VALIDATE(!float_track_.is_visible(), null_str);
	float_track_.set_visible(true);
	absolute_draw_float_widgets();
}

void tprogress_widget::set_track_rect()
{
	const SDL_Rect track_rect = slot_.adjust_rect(track_direct_rect_);

	ttrack& track = progress_->track();
	float_track_.set_ref_widget(&window_, tpoint(track_rect.x, track_rect.y));
	track.set_layout_size(tpoint(track_rect.w, track_rect.h));
	last_frameTexture_size_.x = frameTexture_width();
	last_frameTexture_size_.y = frameTexture_height();
}

SDL_Rect tprogress_::tslot::tslot::adjust_rect(const SDL_Rect& rect)
{
	const int default_width = gui2::settings::screen_width * 80 / 100; // 80%
	const int default_height = 48 * twidget::hdpi_scale;
	SDL_Rect ret = rect;
	if (ret.w == nposm) {
		ret.w = default_width;
	}
	if (ret.h == nposm) {
		ret.h = default_height;
	}
	if (ret.x == nposm) {
		ret.x = (gui2::settings::screen_width - ret.w) / 2;
	}
	if (ret.y == nposm) {
		ret.y = (gui2::settings::screen_height - ret.h) / 2;
	}
	return ret;
}


tprogress_default_slot::tprogress_default_slot(const std::function<bool (tprogress_&)>& did_first_drawn, const std::string& cancel_img, int align)
	: tslot(did_first_drawn, cancel_img)
	, board_size(cfg_2_os_size(8))
	, font_size(font::SIZE_DEFAULT)
	, title_bottom_gap_height(cfg_2_os_size(8))
	, blue_(0)
	, increase_(false)
	, align_(align != nposm? align: tgrid::HORIZONTAL_ALIGN_CENTER)
	, title_text_height_(nposm)
{
	VALIDATE(align_ == tgrid::HORIZONTAL_ALIGN_CENTER || align_ == tgrid::HORIZONTAL_ALIGN_LEFT || align_ == tgrid::HORIZONTAL_ALIGN_RIGHT, null_str);
}

SDL_Rect tprogress_default_slot::adjust_rect(const SDL_Rect& rect)
{	
	VALIDATE(!is_dlg_, null_str);
	int default_width = gui2::settings::screen_width * 80 / 100; // 80%
	int default_height = 48 * twidget::hdpi_scale;
	if (!title_.empty()) {
		default_width += 2 * board_size;
		title_text_height_ = font::get_rendered_text_size(title_, INT32_MAX, font_size, uint32_to_color(0xff000000)).y;
		VALIDATE(title_text_height_ > 0, null_str);
		default_height += 2 * board_size + title_text_height_ + title_bottom_gap_height;
	}
	SDL_Rect ret = rect;
	if (ret.w == nposm) {
		ret.w = default_width;
	}
	if (ret.h == nposm) {
		ret.h = default_height;
	}
	if (ret.x == nposm) {
		ret.x = (gui2::settings::screen_width - ret.w) / 2;
	}
	if (ret.y == nposm) {
		ret.y = (gui2::settings::screen_height - ret.h) / 2;
	}
	return ret;
}

SDL_Rect tprogress_default_slot::get_cancel_rect(const SDL_Rect& widget_rect) const
{
	VALIDATE(!is_dlg_, null_str);
	VALIDATE(has_cancel(), null_str);

	SDL_Rect progress_rect = widget_rect;

	if (!title_.empty()) {
		progress_rect.x += board_size;
		progress_rect.w -= 2 * board_size;

		const int title_full_height = board_size + title_text_height_ + title_bottom_gap_height;
		progress_rect.y += title_full_height;
		progress_rect.h -= title_full_height + board_size;
	}
	progress_rect.w = progress_rect.w - progress_rect.h;

	SDL_Rect cancel_rect = progress_rect;
	cancel_rect.x += cancel_rect.w;
	cancel_rect.w = cancel_rect.h;

	return cancel_rect;
}

void tprogress_default_slot::did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, int percentage, const std::string& message)
{
	SDL_Renderer* renderer = get_renderer();

	// render_rect(renderer, widget_rect, 0x80ffffff);
	render_rect(renderer, widget_rect, 0xffffffff);

	if (!is_dlg_ && !title_.empty()) {
		render_rect_frame(renderer, widget_rect, 0x801883d7, 1);
		if (title_tex_.get() == nullptr) {
			surface surf = font::get_rendered_text(title_, 0, font_size, uint32_to_color(0xff000000));
			title_tex_ = SDL_CreateTextureFromSurface(renderer, surf);
		}
	}
	if (title_tex_.get() != nullptr) {
		int width2, height2;
		SDL_QueryTexture(title_tex_.get(), NULL, NULL, &width2, &height2);
		VALIDATE(title_text_height_ == height2, null_str);
		SDL_Rect rect = ::create_rect(widget_rect.x + board_size, widget_rect.y + board_size, width2, height2);
		SDL_RenderCopy(renderer, title_tex_.get(), nullptr, &rect);
	}

	SDL_Rect progress_rect = widget_rect;
	if (!is_dlg_ && !title_.empty()) {
		progress_rect.x += board_size;
		progress_rect.w -= 2 * board_size;

		const int title_full_height = board_size + title_text_height_ + title_bottom_gap_height;
		progress_rect.y += title_full_height;
		progress_rect.h -= title_full_height + board_size;
	}

	if (!is_dlg_ && has_cancel()) {
		progress_rect.w = progress_rect.w - progress_rect.h;
	}

	render_rect_frame(renderer, progress_rect, 0x8000ff00, 2);

	if (percentage != PROGRESS_MAX_PERCENTAGE) {
		render_rect(renderer, SDL_Rect{progress_rect.x, progress_rect.y, progress_rect.w * percentage / 100, progress_rect.h}, 0xff00ff00);
	} else {
		if (increase_) {
			blue_ += 2;
		} else {
			blue_ -= 2;
		}
		if (blue_ < 0xb0) {
			increase_ = true;
			blue_ = 0xc0;

		} else if (blue_ > 0xff) {
			increase_ = false;
			blue_ = 0xff;
		}

		const uint32_t color = 0xff000000 | ((blue_ & 0xff) << 8);
		render_rect(renderer, SDL_Rect{progress_rect.x, progress_rect.y, progress_rect.w, progress_rect.h}, color);
	}

	if (!message.empty()) {
		surface surf = font::get_rendered_text(message, 0, font::SIZE_SMALL, uint32_to_color(0xff000000));
		SDL_Rect dest_rect {progress_rect.x + (progress_rect.w - surf->w) / 2, progress_rect.y + (progress_rect.h - surf->h) / 2, surf->w, surf->h};
		const int gap = 2 * twidget::hdpi_scale;

		if (align_ == tgrid::HORIZONTAL_ALIGN_LEFT) {
			dest_rect.x = progress_rect.x + gap;

		} else if (align_ == tgrid::HORIZONTAL_ALIGN_RIGHT) {
			dest_rect.x = progress_rect.x + progress_rect.w - gap - surf->w;
		}

		render_surface(renderer, surf, nullptr, &dest_rect);
	}

	//
	// cancel 
	//
	if (!is_dlg_ && has_cancel()) {
		SDL_Rect cancel_rect = get_cancel_rect(widget_rect);

		if (cancel_tex_.get() == nullptr) {
			VALIDATE(!cancel_img_.empty(), null_str);
			surface surf = image::get_image(cancel_img_);
			VALIDATE(surf.get() != nullptr, null_str);
			cancel_tex_ = SDL_CreateTextureFromSurface(renderer, surf);
		}
		if (cancel_tex_.get() != nullptr) {
			SDL_RenderCopy(renderer, cancel_tex_.get(), nullptr, &cancel_rect);
		}
	}
}

bool run_with_progress(tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, const SDL_Rect& _rect)
{
	std::vector<twindow*> connected = gui2::connectd_window();
	if (connected.empty()) {
		slot.set_is_dlg();
		return run_with_progress_dlg(slot, title, message, hidden_ms, _rect);
	}

	slot.set_title(title);

	twindow& window = *connected.back();

	gui2::tprogress_widget dlg(window, slot, title, message, hidden_ms, _rect);
	return slot.did_first_drawn(dlg);
}

//
// gui2::delete_files
//
static bool delete_file_delete(gui2::tprogress_& progress, const std::string& file, SDL_bool is_file)
{
	progress.show_slice();
	if (progress.task_cancelled()) {
		return false;
	}
	SDL_bool ret = SDL_DeleteFilesFast(file.c_str(), is_file);
	if (!ret) {
		utils::string_map symbols;
		// symbols["file"] = os_normalize_path(file);
		symbols["file"] = file;
		gui2::show_message(null_str, vgettext2("Delete $file fail", symbols));
	}
	return ret;
}

#define delete_file_set_message(progress, file, is_file) \
	symbols["file"] = file.substr(prefix_size); \
	progress.set_message(vgettext2("Deleting $file", symbols)); \
	progress.set_percentage((deleted.dirs + deleted.files) * 100 / (summary.dirs + summary.files)); \
	ret = delete_file_delete(progress, file, is_file)

static bool delete_file_internal(gui2::tprogress_& progress, int prefix_size, utils::string_map& symbols, 
	const tpath_summary& summary, tpath_summary& deleted, const std::string& path)
{
	SDL_DIR* dir;
	SDL_dirent2* dirent;
	bool ret = true;
	std::string full_name;
	const std::string pathplus1 = path + '/';

	VALIDATE(!SDL_IsFile(path.c_str()), null_str);

	dir = SDL_OpenDir(path.c_str());
	if (!dir) {
		return false;
	}
	
	while ((dirent = SDL_ReadDir(dir))) {
		full_name = pathplus1 + dirent->name;
		if (SDL_DIRENT_DIR(dirent->mode)) {
			if (SDL_strcmp(dirent->name, ".") && SDL_strcmp(dirent->name, "..")) {
				if (!delete_file_internal(progress, prefix_size, symbols, summary, deleted, full_name)) {
					ret = false;
					break;
				}
			}
		} else {
			// file
			delete_file_set_message(progress, full_name, SDL_TRUE);
			deleted.files ++;
			if (!ret) {
				break;
			}
		}
	}
	SDL_CloseDir(dir);

	if (!ret) {
		return false;
	}
	delete_file_set_message(progress, path, SDL_FALSE);
	deleted.dirs ++;
	return ret;
}

bool delete_file(gui2::tprogress_& progress, const std::string& file)
{
	bool ret = true;
	utils::string_map symbols;

	tpath_summary summary, deleted;
	path_summary(summary, file.c_str());
	if (summary.dirs == 0 && summary.files == 0) {
		// file is empty directory. 
		// avoid devided 0 exception.
		summary.dirs = 1;
	}

	memset(&deleted, 0, sizeof(deleted));

	const int prefix_size = extract_directory(file).size() + 1;
	if (SDL_IsFile(file.c_str())) {
		delete_file_set_message(progress, file, SDL_TRUE);
		return ret;
	}

	return delete_file_internal(progress, prefix_size, symbols, summary, deleted, file);
}

bool delete_files(gui2::tprogress_& progress, const std::vector<std::string>& files)
{
	VALIDATE(!files.empty(), null_str);

	bool ret = false;
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& file = *it;
		if (!delete_file(progress, file)) {
			return false;
		}
	}

	return true;
}

} // namespace gui2

