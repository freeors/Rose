/* $Id: button.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TRACK_HPP_INCLUDED
#define GUI_WIDGETS_TRACK_HPP_INCLUDED

#include "gui/widgets/control.hpp"
#include "gui/widgets/timer.hpp"

namespace gui2 {

/**
 * Simple push button.
 */
class ttrack: public tcontrol
{
public:
	static bool global_did_drawing;

	ttrack();
	~ttrack();

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool active) override { active_ = active; }
		
	/** Inherited from tcontrol. */
	bool get_active() const override { return active_; }

	void set_did_draw(const std::function<void (ttrack&, const SDL_Rect&, const bool)>& did)
	{ did_draw_ = did; }

	void set_did_mouse_motion(const std::function<void (ttrack&, const tpoint&, const tpoint&)>& did)
	{ did_mouse_motion_ = did; }
	void set_did_left_button_down(const std::function<void (ttrack&, const tpoint&)>& did)
	{ did_left_button_down_ = did; }
	void set_did_mouse_leave(const std::function<void (ttrack&, const tpoint&, const tpoint&)>& did)
	{ did_mouse_leave_ = did; }

	void set_did_double_click(const std::function<void (ttrack&, const tpoint& coordinate)>& did)
	{ did_double_click_ = did; }
	void set_did_right_button_up(const std::function<void (ttrack&, const tpoint& coordinate)>& did)
	{ did_right_button_up_ = did; }

	SDL_Rect get_draw_rect() const;
	texture& background_texture() { return background_tex_; }
	void set_did_create_background_tex(const std::function<texture (ttrack&, const SDL_Rect&)>& did)
	{ did_create_background_tex_ = did; }

	void set_require_capture(bool val) { require_capture_ = val; }
	void set_timer_interval(int interval);

	void timer_handler();

	// called by tscroll_container.
	void reset_background_texture(const texture& screen, const SDL_Rect& rect);
	void set_background_tex_dirty() { 
		VALIDATE(did_create_background_tex_, "Call set_background_tex_dirty must be in did_create_background_tex_ != NULL");
		background_tex_dirty_ = true; 
	}

	void clear_texture() override;

	// avoid app call did_draw_ directly
	void immediate_draw();

	// avoid exposing set_dirty() api outwards, for example lua.
	void trigger_redraw() { set_dirty(); }

	void disable_rose_draw_bg() { rose_draw_bg_ = false; }

private:
	/** Inherited from tcontrol. */
	void impl_draw_background(
			  texture& frame_buffer
			, int x_offset
			, int y_offset) override;

	texture get_canvas_tex() override;

	/** Inherited from tcontrol. */
	unsigned get_state() const override { return 0; }

private:
	/** Inherited from tcontrol. */
	const std::string& get_control_type() const override;

	/***** ***** ***** signal handlers ***** ****** *****/
	void signal_handler_mouse_leave(const tpoint& coordinate);

	void signal_handler_left_button_down(const tpoint& coordinate);

	void signal_handler_mouse_motion(const tpoint& coordinate);

	void signal_handler_left_button_double_click(const event::tevent event, bool& handled, bool& halt, const tpoint& coordinate);
	void signal_handler_right_button_up(bool& handled, bool& halt, const tpoint& coordinate);

	void handle_background_tex_dirty();
private:
	bool active_;
	ttimer timer_;
	texture background_tex_;
	bool require_capture_;
	int timer_interval_;

	class tdraw_lock
	{
	public:
		explicit tdraw_lock(SDL_Renderer* renderer, ttrack& widget);
		~tdraw_lock();

	private:
		SDL_Renderer* renderer_;
		SDL_Texture* original_;
		SDL_Rect original_clip_rect_;
		ttrack& widget_;
		bool render_pause_;
	};

	class tdid_drawing_lock
	{
	public:
		tdid_drawing_lock(ttrack& widget)
			: widget_(widget)
			, original_(global_did_drawing)
		{
			if (widget.did_drawing_) {
				std::stringstream ss;
				ss << "track wiget(id: " << widget.id_ << ") must not call did_drawing_ again";
				VALIDATE(false, ss.str());
			}

			global_did_drawing = true;
			widget.did_drawing_ = true;
		}
		~tdid_drawing_lock()
		{
			VALIDATE(widget_.did_drawing_, null_str);
			global_did_drawing = original_;
			widget_.did_drawing_ = false;
		}

	private:
		ttrack& widget_;
		const bool original_;
	};
	bool did_drawing_;
	std::function<void (ttrack&, const SDL_Rect&, const bool)> did_draw_;

	bool rose_draw_bg_;

	tpoint first_coordinate_;
	std::function<void (ttrack&, const tpoint& first, const tpoint& last)> did_mouse_motion_;
	std::function<void (ttrack&, const tpoint& last)> did_left_button_down_;
	std::function<void (ttrack&, const tpoint& first, const tpoint& last)> did_mouse_leave_;

	std::function<texture (ttrack&, const SDL_Rect&)> did_create_background_tex_;
	bool background_tex_dirty_;

	/** Mouse left double click callback */
	std::function<void (ttrack&, const tpoint& coordinate)> did_double_click_;

	std::function<void (ttrack&, const tpoint& coordinate)> did_right_button_up_;
};

} // namespace gui2

#endif

