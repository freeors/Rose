#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/track.hpp"

#include "gui/auxiliary/widget_definition/track.hpp"
#include "gui/auxiliary/window_builder/track.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/timer.hpp"

using namespace std::placeholders;

#include "rose_config.hpp"

namespace gui2 {

REGISTER_WIDGET(track)

bool ttrack::global_did_drawing = false;

ttrack::tdraw_lock::tdraw_lock(SDL_Renderer* renderer, ttrack& widget)
	: widget_(widget)
	, renderer_(renderer)
	, original_(SDL_GetRenderTarget(renderer))
	, render_pause_(false)
{
	if (widget_.float_widget_) {
		int result = -1;
		texture& target = widget_.canvas(0).canvas_tex();
		SDL_Texture* tex = target.get();
		// if app is in background, tex will be nullptr(android).
		if (tex != nullptr) {
			// if want texture to target, this texture must be SDL_TEXTUREACCESS_TARGET.
			int access;
			SDL_QueryTexture(tex, NULL, &access, NULL, NULL);
			VALIDATE(access == SDL_TEXTUREACCESS_TARGET, null_str);

			result = SDL_SetRenderTarget(renderer, tex);
		}
		if (result != 0) {
			render_pause_ = true;
			SDL_SetRenderAppPause(renderer, SDL_TRUE);
		}
	}

	const SDL_Rect rect = widget.get_draw_rect();
	SDL_RenderGetClipRect(renderer, &original_clip_rect_);
	SDL_RenderSetClipRect(renderer, widget_.float_widget_? nullptr: &rect);
}

ttrack::tdraw_lock::~tdraw_lock()
{
	SDL_RenderSetClipRect(renderer_, SDL_RectEmpty(&original_clip_rect_)? nullptr: &original_clip_rect_);
	if (widget_.float_widget_) {
		if (!render_pause_) {
			SDL_SetRenderTarget(renderer_, original_);
		} else {
			SDL_SetRenderAppPause(renderer_, SDL_FALSE);
		}
	}
}

ttrack::ttrack()
	: tcontrol(1)
	, active_(true)
	, require_capture_(true)
	, timer_interval_(0)
	, did_drawing_(false)
	, rose_draw_bg_(true)
	, first_coordinate_(construct_null_coordinate())
	, background_tex_dirty_(false)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(std::bind(
				&ttrack::signal_handler_left_button_down, this, _5));

	connect_signal<event::MOUSE_LEAVE>(std::bind(
				&ttrack::signal_handler_mouse_leave, this, _5));

	connect_signal<event::MOUSE_MOTION>(std::bind(
				&ttrack::signal_handler_mouse_motion, this, _5));
/*
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(std::bind(
				  &ttrack::signal_handler_left_button_double_click
				, this, _2, _3, _4, _5));
*/
/*
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(std::bind(
				  &ttrack::signal_handler_left_button_double_click
				, this, _2, _3, _4)
			, event::tdispatcher::back_post_child);
*/

	connect_signal<event::RIGHT_BUTTON_UP>(std::bind(
				&ttrack::signal_handler_right_button_up
					, this, _3, _4, _5));
/*
	connect_signal<event::RIGHT_BUTTON_UP>(std::bind(
				  &ttrack::signal_handler_right_button_up
				, this, _3, _4, _5), event::tdispatcher::back_post_child);
*/
}

ttrack::~ttrack()
{
}

void ttrack::reset_background_texture(const texture& screen, const SDL_Rect& rect)
{
	if (!screen.get()) {
		VALIDATE(SDL_RectEmpty(&rect), null_str);
	} else {
		VALIDATE(!SDL_RectEmpty(&rect), null_str);
	}

	if (background_tex_) {
		int width, height;
		SDL_QueryTexture(background_tex_.get(), nullptr, nullptr, &width, &height);
		if (width != rect.w || height != rect.h || background_tex_dirty_) {
			// always call when background_tex_dirty_ == true.
			background_tex_ = nullptr;
		}
	}
	if (background_tex_.get() == nullptr && screen.get()) {
		if (did_create_background_tex_ != NULL) {
			background_tex_ = did_create_background_tex_(*this, rect);
			if (background_tex_.get() != nullptr) {
				int width, height;
				SDL_QueryTexture(background_tex_.get(), nullptr, nullptr, &width, &height);
				VALIDATE(width == rect.w && height == rect.h, null_str);
			}
		}
		if (background_tex_.get() == nullptr) {
			texture_from_texture(screen, background_tex_, &rect, 0, 0);
		}
	}

	background_tex_dirty_ = false;
}

void ttrack::handle_background_tex_dirty()
{
	if (!background_tex_dirty_) {
		return;
	}

	VALIDATE(did_create_background_tex_ != NULL, null_str);
	SDL_Rect rect = get_rect();
	background_tex_ = did_create_background_tex_(*this, rect);
	if (background_tex_.get() != nullptr) {
		// if result by background_tex_dirty_ = ture, did_create_background_tex_ must return valid texture.
		// but if app is in background, SDL_CreateTexture will be nullptr(android).
		background_tex_dirty_ = false;

		int width, height;
		SDL_QueryTexture(background_tex_.get(), nullptr, nullptr, &width, &height);
		VALIDATE(width == rect.w && height == rect.h, null_str);
	}
}

void ttrack::impl_draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	if (!timer_.valid()) {
		set_timer_interval(0);
	}

	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);

	const SDL_Rect rect = get_rect();
	reset_background_texture(frame_buffer, rect);

	if (did_draw_ != NULL) {
		const int original_windows = twidget::popup_windows;
		handle_background_tex_dirty();
		// If snap it directly from the background, the background_tex_ is drawn.
		tdid_drawing_lock lock(*this);
		twidget::tdisable_popup_window_lock lock2("Must not popup new window during track.did_draw_");
		SDL_Renderer* renderer = get_renderer();
		ttrack::tdraw_lock lock3(renderer, *this);
		bool bg_drawn = did_create_background_tex_ == NULL;
		if (!bg_drawn && rose_draw_bg_) {
			SDL_RenderCopy(renderer, background_texture().get(), nullptr, &rect);
		}
		did_draw_(*this, rect, bg_drawn);
		if (twidget::popup_windows != original_windows) {
			throw tpopup_window_exception(get_window()->id(), get_control_type(), id_);
		}
	}
}

texture ttrack::get_canvas_tex()
{
	texture result = tcontrol::get_canvas_tex();
	if (result.get() == nullptr) {
		// if app is in background, canvas will be nullptr(android).
		return result;
	}

	const SDL_Rect rect{0, 0, (int)w_, (int)h_};
	reset_background_texture(result, rect);


	if (did_draw_ != NULL) {
		handle_background_tex_dirty();
		// If snap it directly from the background, the background_tex_ is drawn.
		tdid_drawing_lock lock(*this);
		twidget::tdisable_popup_window_lock lock2("Must not popup new window during track.did_draw_");
		SDL_Renderer* renderer = get_renderer();
		ttrack::tdraw_lock lock3(renderer, *this);
		bool bg_drawn = did_create_background_tex_ == NULL;
		if (!bg_drawn && rose_draw_bg_) {
			SDL_RenderCopy(renderer, background_texture().get(), nullptr, &rect);
		}
		did_draw_(*this, rect, bg_drawn);
	}
	return result;
}

SDL_Rect ttrack::get_draw_rect() const
{
	if (float_widget_) {
		return ::create_rect(0, 0, w_, h_);
	}
	return ::create_rect(x_, y_, w_, h_);
}

void ttrack::timer_handler()
{
	if (get_window()->get_need_layout()) {
		// when need_layout is true, rect of this widget is error, so do nothing.
		return;
	}

	// Although background_tex_ is null, it still executes did_draw_. 
	// Why? --In did_draw_, apps may put actions that are not related to gui2 but affect progress, 
	//        like reading serial, determining whether a variable is still valid based on experienced time.
	// rect.w > 0 && rect.h > 0?
	//    The timer may have been executed before the window::layout. 
	//    --why? reference to twindow::show_slice(), timer is exeucted during events::pump(), and twindow::layout is during absolute_draw().
	//      if pre_show spend time more than timer_interval_, first timer_handler() will execute before twindow::layout.
	//    when w_, h_ were 0. gui2::ttrack must make sure this widget has layouted before call did_draw_.
	const SDL_Rect rect = get_draw_rect();
	if (did_draw_ != NULL && rect.w > 0 && rect.h > 0 && visible_ == twidget::VISIBLE) {
		handle_background_tex_dirty();
		tdid_drawing_lock lock(*this);
		twidget::tdisable_popup_window_lock lock2("Must not popup new window during track.did_draw_");
		SDL_Renderer* renderer = get_renderer();
		ttrack::tdraw_lock lock3(renderer, *this);
		bool bg_drawn = false;
		if (!bg_drawn && rose_draw_bg_) {
			SDL_RenderCopy(renderer, background_texture().get(), nullptr, &rect);
		}
		did_draw_(*this, rect, bg_drawn);
	}
}

void ttrack::immediate_draw()
{
	if (did_draw_ != NULL) {
		tdid_drawing_lock lock(*this);
		SDL_Rect rect = get_draw_rect();
		SDL_Renderer* renderer = get_renderer();
		ttrack::tdraw_lock lock3(renderer, *this);
		bool bg_drawn = false;
		if (!bg_drawn && rose_draw_bg_) {
			SDL_RenderCopy(renderer, background_texture().get(), nullptr, &rect);
		}
		did_draw_(*this, rect, bg_drawn);
	}
}

void ttrack::set_timer_interval(int interval)
{ 
	VALIDATE(interval >= 0, null_str);
	twindow* window = get_window();
	VALIDATE(window, "must call set_timer_interval after window valid.");

	if (timer_interval_ != interval) {
		if (timer_.valid()) {
			timer_.reset();
		}
		if (interval != 0) {
			timer_.reset(interval, *window, std::bind(&ttrack::timer_handler, this));
		}
		timer_interval_ = interval;
	}
}

void ttrack::clear_texture()
{
	tcontrol::clear_texture();
	if (background_tex_.get()) {
		background_tex_ = nullptr;
	}
}

const std::string& ttrack::get_control_type() const
{
	static const std::string type = "track";
	return type;
}

void ttrack::signal_handler_mouse_leave(const tpoint& coordinate)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	if (did_mouse_leave_) {
		did_mouse_leave_(*this, first_coordinate_, coordinate);
	}
	set_null_coordinate(first_coordinate_);
}

void ttrack::signal_handler_left_button_down(const tpoint& coordinate)
{
	VALIDATE(is_null_coordinate(first_coordinate_), null_str);

	twindow* window = get_window();
	if (window && require_capture_) {
		window->mouse_capture();
	}
	first_coordinate_ = coordinate;

	if (did_left_button_down_) {
		did_left_button_down_(*this, coordinate);
	}
}

void ttrack::signal_handler_mouse_motion(const tpoint& coordinate)
{
	VALIDATE(!is_mouse_leave_window_event(coordinate), null_str);

	if (did_mouse_motion_) {
		did_mouse_motion_(*this, first_coordinate_, coordinate);
	}
}

void ttrack::signal_handler_left_button_double_click(const event::tevent event, bool& handled, bool& halt, const tpoint& coordinate)
{
	handled = true;

	if (did_double_click_) {
		// twidget_exist_validator validator(*this);
		did_double_click_(*this, coordinate);
	}
}

void ttrack::signal_handler_right_button_up(bool& handled, bool& halt, const tpoint& coordinate)
{
	halt = handled = true;
	if (did_right_button_up_) {
		// twidget_exist_validator validator(*this);
		did_right_button_up_(*this, coordinate);
	}
}

} // namespace gui2
