#ifndef GUI_DIALOGS_OPENCV_HPP_INCLUDED
#define GUI_DIALOGS_OPENCV_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "rtc_client.hpp"
#include <opencv2/core.hpp>

namespace gui2 {

class ttoggle_button;
class ttrack;
class tslider;

class topencv: public tdialog, public trtc_client::tadapter
{
public:
	enum {mouse, blend, markerless_ar};

	explicit topencv();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void example_mouse();
	void example_blend(double alpha);
	void example_markerless_ar();
	void did_example_item_changed(ttoggle_button& widget);
	void did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn);
	void did_left_button_down_paper(ttrack& widget, const tpoint& coordinate);
	void did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void did_slider_value_changed(tslider& widget, int value, int at);

	void app_first_drawn() override;

	void did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;

private:
	ttrack* paper_;
	std::vector<image::tblit> blits_;

	std::unique_ptr<tavcapture> avcapture_;
	surface persist_surf_;
	surface temperate_surf_;

	int current_example_;
	tpoint last_coordinate_;
	cv::RNG rng_;
};

} // namespace gui2

#endif

