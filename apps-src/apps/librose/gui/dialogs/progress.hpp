/* $Id$ */
/*
   Copyright (C) 2011 by Sergey Popov <loonycyborg@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_PROGRESS_HPP_INCLUDED
#define GUI_DIALOGS_PROGRESS_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"
// #include "gui/widgets/progress_bar.hpp"

namespace gui2 {

class tbutton;

class tprogress_util
{
public:
	explicit tprogress_util(twindow& window, twidget& widget, tprogress_::tslot& slot, const std::string& message, int initial_percentage);
	virtual ~tprogress_util();
	
	ttrack& track() { return widget_; }
	bool set_percentage(int percentage);
	int get_percentage() const { return percentage_; }

	bool set_message(const std::string& message);

private:
	void did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);

private:
	twindow& window_;
	ttrack& widget_;
	tprogress_::tslot& slot_;
	std::string message_;
	int percentage_;
};

class tprogress : public tdialog, public tprogress_
{
public:
	tprogress(tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, int best_width, int best_height);
	~tprogress();

	void set_percentage(int percentage) override;
	void set_message(const std::string& message) override;
	void cancel_task() override;
	bool task_cancelled() const override { return require_cancel_; }
	void show_slice() override;
	bool is_visible() const override;
	bool is_new_window() const override { return true; }

protected:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	void click_cancel();

private:
	void app_first_drawn() override;
	void timer_handler(bool render_track);

private:
	tprogress_::tslot& slot_;
	const std::string title_;
	const std::string message_;
	Uint32 hidden_ticks_;

	std::unique_ptr<tprogress_util> progress_;
	
	Uint32 start_ticks_;
	bool require_cancel_;
	const int track_best_width_;
	const int track_best_height_;
};


class tprogress_widget: public tprogress_
{
public:
	tprogress_widget(twindow& window, tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, const SDL_Rect& rect);
	~tprogress_widget();

	void set_percentage(int percentage) override;
	void set_message(const std::string& message) override;
	void cancel_task() override;
	bool task_cancelled() const override { return require_cancel_; }
	void show_slice() override;
	bool is_visible() const override;
	bool is_new_window() const override { return false; }

private:
	void pre_show();
	void set_visible();
	void set_track_rect();

	void did_left_button_down(ttrack& widget, const tpoint& coordinate);
	void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void did_mouse_leave_paper(ttrack& widget, const tpoint&, const tpoint& /*last_coordinate*/);

private:
	void timer_handler(bool render_track);

private:
	twindow& window_;
	tfloat_widget& float_track_;
	tprogress_::tslot& slot_;
	const std::string message_;
	uint32_t hidden_ticks_;

	std::unique_ptr<tprogress_util> progress_;
	
	Uint32 start_ticks_;
	bool require_cancel_;
	SDL_Rect track_direct_rect_;

	bool maybe_cancel_;
	SDL_Point last_frameTexture_size_;
};

} // namespace gui2

#endif

