/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#ifndef GUI_DIALOGS_DIALOG_HPP_INCLUDED
#define GUI_DIALOGS_DIALOG_HPP_INCLUDED

#include "SDL_rect.h"
#include "gui/widgets/widget.hpp"
#include "image.hpp"
#include "thread.hpp"
#include "gui/widgets/timer.hpp"

#include <string>
#include <vector>

class CVideo;
class base_controller;
class display;

#define PROGRESS_MIN_PERCENTAGE		0
#define PROGRESS_MAX_PERCENTAGE		100

namespace gui2 {

void register_window(const std::string& app, const std::string& id);

/**
 * Registers a window.
 *
 * This function registers a window. The registration is used to validate
 * whether the config for the window exists when starting Wesnoth.
 *
 * @note Most of the time you want to call @ref REGISTER_DIALOG instead of this
 * function. It also directly adds the code for the dialog's id function.
 *
 * @param id                      Id of the window, multiple dialogs can use
 *                                the same window so the id doesn't need to be
 *                                unique.
 */
#define REGISTER_WINDOW(                                                   \
		  app                                                              \
		, id)                                                              \
namespace {                                                                \
                                                                           \
	namespace ns_##id {                                                    \
                                                                           \
		struct tregister_helper {                                          \
			tregister_helper()                                             \
			{                                                              \
				register_window(#app, #id);                                \
			}                                                              \
		};                                                                 \
                                                                           \
		tregister_helper register_helper;                                  \
	}                                                                      \
}

/**
 * Registers a window for a dialog.
 *
 * Call this function to register a window. In the header of the class it adds
 * the following code:
 *@code
 *  // Inherited from tdialog, implemented by REGISTER_DIALOG.
 *	virtual const std::string& id() const;
 *@endcode
 * Then use this macro in the implementation, inside the gui2 namespace.
 *
 * @note When the @p id is "foo" and the type tfoo it's easier to use
 * REGISTER_DIALOG(foo).
 *
 * @param type                    Class type of the window to register.
 * @param id                      Id of the window, multiple dialogs can use
 *                                the same window so the id doesn't need to be
 *                                unique.
 */

#define REGISTER_DIALOG2(                                                  \
		  type                                                             \
		, app                                                              \
		, id)                                                              \
                                                                           \
REGISTER_WINDOW(app, id)                                                   \
                                                                           \
const std::string&                                                         \
type::window_id() const                                                    \
{                                                                          \
	static const std::string result(::utils::generate_app_prefix_id(#app, #id));  \
	return result;                                                         \
}


/**
 * Wrapper for REGISTER_DIALOG2.
 *
 * "Calls" REGISTER_DIALOG2(twindow_id, window_id)
 */
#define REGISTER_DIALOG(app, window_id) REGISTER_DIALOG2(t##window_id, app, window_id)

/**
 * Abstract base class for all dialogs.
 *
 * A dialog shows a certain window instance to the user. The subclasses of this
 * class will hold the parameters used for a certain window, eg a server
 * connection dialog will hold the name of the selected server as parameter that
 * way the caller doesn't need to know about the 'contents' of the window.
 *
 * @par Usage
 *
 * Simple dialogs that are shown to query user information it is recommended to
 * add a static member called @p execute. The parameters to the function are:
 * - references to in + out parameters by reference
 * - references to the in parameters
 * - the parameters for @ref tdialog::show.
 *
 * The 'in + out parameters' are used as initial value and final value when the
 * OK button is pressed. The 'in parameters' are just extra parameters for
 * showing.
 *
 * When a function only has 'in parameters' it should return a void value and
 * the function should be called @p display, if it has 'in + out parameters' it
 * must return a bool value. This value indicates whether or not the OK button
 * was pressed to close the dialog. See @ref teditor_new_map::execute for an
 * example.
 */
class tcontrol;
class ttoggle_button;
class treport;
class ttrack;

struct tcontext_menu {
	enum {F_HIDE = 0x1, F_PARAM = 0x2};

	tcontext_menu(const std::string& id)
		: main_id(id)
		, menus()
		, report(NULL)
	{}

	static std::pair<std::string, std::string> extract_item(const std::string& id);
	static size_t decode_flag_str(const std::string& str);

	void load(display& disp, const config& cfg);
	void show(const display& gui, const base_controller& controller, const std::string& id) const;
	void hide() const;

	std::string main_id;
	std::map<std::string, std::vector<std::string> > menus;
	gui2::treport* report;
};

class tdialog: public rtc::MessageHandler
{
	/**
	 * Special helper function to get the id of the window.
	 *
	 * This is used in the unit tests, but these implementation details
	 * shouldn't be used in the normal code.
	 */
	friend class treport;

public:
	class ttexture_holder
	{
	public:
		explicit ttexture_holder(tdialog& dlg)
			: dlg_(dlg)
		{
			register_holder();
		}
		virtual ~ttexture_holder()
		{
			unregister_holder();
		}

		virtual void clear_texture() = 0;

	protected:
		void register_holder();
		void unregister_holder();

	private:
		tdialog& dlg_;
	};

	explicit tdialog(base_controller* controller = nullptr);
	virtual ~tdialog();

	/**
	 * Shows the window.
	 *
	 * @param video               The video which contains the surface to draw
	 *                            upon.
	 * @param auto_close_time     The time in ms after which the dialog will
	 *                            automatically close, if 0 it doesn't close.
	 *                            @note the timeout is a minimum time and
	 *                            there's no quarantee about how fast it closes
	 *                            after the minimum.
	 *
	 * @returns                   Whether the final retval_ == twindow::OK
	 */
	bool show(const unsigned explicit_x = nposm, const unsigned explicit_y = nposm);
	void scene_show();
	void resize_screen();

	void set_volatiles(std::vector<gui2::twidget*>& widgets) { volatiles_ = widgets; }
	const std::vector<gui2::twidget*>& volatiles() const { return volatiles_; }


	/***** ***** ***** setters / getters for members ***** ****** *****/

	int get_retval() const { return retval_; }

	void set_restore(const bool restore) { restore_ = restore; }

	void destruct_widget(const twidget* widget);
	virtual twidget* get_object(const std::string& id) const;
	virtual void set_object_active(const std::string& id, bool active) const;
	virtual void set_object_visible(const std::string& id, const twidget::tvisible visible) const;

	virtual void did_first_layouted(twindow& window) {};
	virtual void app_first_drawn() {};
	virtual void app_did_keyboard_shown(int reason) {};
	virtual void app_did_keyboard_hidden(int reason) {};
	virtual void app_handler_mouse_motion(const tpoint& coordinate) {};

	twidget* get_report(int num) const;
	void set_report_label(int num, const std::string& label) const;
	void set_report_blits(int num, const std::vector<tformula_blit>& blits) const;

	std::vector<tcontext_menu>& context_menus() { return context_menus_; }
	tcontext_menu* context_menu(const std::string& id);

	twindow* get_window() const { return window_; }

	enum {POST_MSG_DRAG_MOUSE_LEAVE, POST_MSG_PULLREFRESH, POST_MSG_POPUP_EDITBOX, POST_MSG_MIN_APP = 100};
	struct tmsg_data_drag_mouse_leave: public rtc::MessageData {
		explicit tmsg_data_drag_mouse_leave(const std::function<void (const int, const int, bool)>& did,
			int x, int y, bool is_up_result)
			: did_mouse_leave(did)
			, x(x)
			, y(y)
			, is_up_result(is_up_result)
		{}

		std::function<void (const int, const int, bool)> did_mouse_leave;
		int x;
		int y;
		bool is_up_result;
	};

	struct tmsg_data_pullrefresh: public rtc::MessageData {
		explicit tmsg_data_pullrefresh(twidget* widget, const SDL_Rect& rect)
			: widget(widget)
			, rect(rect)
		{}

		twidget* widget;
		const SDL_Rect rect;
	};

	struct tmsg_data_popup_editbox: public rtc::MessageData {
		explicit tmsg_data_popup_editbox(twidget& widget)
			: widget(widget)
		{}

		twidget& widget;
	};
	void OnMessage(rtc::Message* msg) override;

protected:
	void set_timer_interval(int interval);
	void click_generic_handler(tcontrol& widget, const std::string& sparam);

private:
	void base_timer_handler();
	virtual void app_timer_handler(uint32_t now) {}
	virtual void app_resize_screen() {}

	void post_layout();

	// must allocate msg->pdata use new. must not use constant value, why references to MessageQueue::Clear(..).
	virtual void app_OnMessage(rtc::Message* msg) {}
	void signal_handler_mouse_leave(const tpoint& coordinate, const tpoint& coordinate2);

protected:
	CVideo& video_;
	twindow* window_;
	std::vector<twidget*> volatiles_;
	base_controller* controller_;
	mutable std::map<std::string, twidget*> cached_widgets_;
	std::map<int, const std::string> reports_;
	std::vector<tcontext_menu> context_menus_;
	int timer_interval_;
	ttimer timer_;

private:
	void scene_pre_show();

	/** The id of the window to build. */
	virtual const std::string& window_id() const = 0;

	/**
	 * Builds the window.
	 *
	 * Every dialog shows it's own kind of window, this function should return
	 * the window to show.
	 *
	 * @param video               The video which contains the surface to draw
	 *                            upon.
	 * @returns                   The window to show.
	 */
	twindow* build_window(CVideo& video, const unsigned explicit_x, const unsigned explicit_y);

	/**
	 * Actions to be taken before showing the window.
	 *
	 * At this point the registered fields are registered and initialized with
	 * their initial values.
	 *
	 * @param video               The video which contains the surface to draw
	 *                            upon.
	 * @param window              The window to be shown.
	 */
	virtual void pre_show() {}

	/**
	 * Actions to be taken after the window has been shown.
	 *
	 * At this point the registered fields already stored their values (if the
	 * OK has been pressed).
	 *
	 * @param window              The window which has been shown.
	 */
	virtual void post_show() {}

private:
	/** Returns the window exit status, 0 means not shown. */
	int retval_;

	/**
	 * Restore the screen after showing?
	 *
	 * Most windows should restore the display after showing so this value
	 * defaults to true. Toplevel windows (like the titlescreen don't want this
	 * behaviour so they can change it in pre_show().
	 */
	bool restore_;

	int longblock_threshold_;
	uint32_t last_send_longblock_ticks_;

	std::set<ttexture_holder*> texture_holders_;
};

class tprogress_
{
public:
	struct tslot {
	public:
		tslot(const std::function<bool (tprogress_&)>& did_first_drawn, const std::string& cancel_img)
			: did_first_drawn(did_first_drawn)
			, cancel_img_(cancel_img)
			, is_dlg_(false)
		{}

		virtual SDL_Rect adjust_rect(const SDL_Rect& rect);
		virtual SDL_Rect get_cancel_rect(const SDL_Rect& widget_rect) const { return empty_rect; }
		virtual void did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, int percentage, const std::string& message) = 0;

		void set_is_dlg() { is_dlg_ = true; }
		void set_title(const std::string& title) { title_ = title; }
		bool has_cancel() const { return !cancel_img_.empty(); }
		const std::string& cancel_img() const { return cancel_img_; }

	public:
		const std::function<bool (tprogress_&)> did_first_drawn;

	protected:
		std::string title_;
		const std::string cancel_img_;
		bool is_dlg_;
	};

	static tprogress_* instance;

	virtual void set_percentage(int percentage) = 0;
	virtual void set_message(const std::string& message) = 0;
	virtual void cancel_task() = 0;
	virtual bool task_cancelled() const = 0;
	virtual void show_slice() = 0;
	virtual bool is_visible() const = 0;
	virtual bool is_new_window() const = 0;
};

class tprogress_default_slot: public tprogress_::tslot
{
public:
	tprogress_default_slot(const std::function<bool (tprogress_&)>& did_first_drawn, const std::string& cancel_img = "", int align = nposm);

	SDL_Rect get_cancel_rect(const SDL_Rect& widget_rect) const override;
	void did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, int percentage, const std::string& message) override;

private:
	SDL_Rect adjust_rect(const SDL_Rect& rect) override;

public:
	int board_size;
	int font_size;
	int title_bottom_gap_height;

private:
	uint32_t blue_;
	bool increase_;
	const int align_;
	texture title_tex_;
	texture cancel_tex_;
	int title_text_height_;
};

bool run_with_progress(tprogress_::tslot& slot, const std::string& title, const std::string& message, int hidden_ms, const SDL_Rect& rect = SDL_Rect{nposm, nposm, nposm, nposm});
bool delete_file(gui2::tprogress_& progress, const std::string& file);
bool delete_files(gui2::tprogress_& progress, const std::vector<std::string>& files);
bool show_confirm_delete(const std::vector<std::string>& files, bool fs);

class button_action
{
public:
	virtual ~button_action() {}

	virtual int button_pressed(int menu_selection) = 0;
};

struct tval_str {
	tval_str(int v, const std::string& s)
		: val(v), str(s)
	{}
	int val;
	std::string str;
};

} // namespace gui2

#endif

