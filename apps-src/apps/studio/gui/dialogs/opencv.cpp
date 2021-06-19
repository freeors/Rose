#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/opencv.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "font.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/video.hpp>

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, opencv)

topencv::topencv()
	: last_coordinate_(construct_null_coordinate())
	, rng_(12345)
	, current_example_(nposm)
{
}

void topencv::pre_show()
{
	window_->set_margin(8 * twidget::hdpi_scale, 8 * twidget::hdpi_scale, 0, 8 * twidget::hdpi_scale);
	window_->set_label("misc/white_background.png");

	find_widget<tlabel>(window_, "title", false).set_label(_("OpenCV tester"));

	paper_ = find_widget<ttrack>(window_, "paper", false, true);
	paper_->set_did_draw(std::bind(&topencv::did_draw_paper, this, _1, _2, _3));
	paper_->set_did_left_button_down(std::bind(&topencv::did_left_button_down_paper, this, _1, _2));
	paper_->set_did_mouse_leave(std::bind(&topencv::did_mouse_leave_paper, this, _1, _2, _3));
	paper_->set_did_mouse_motion(std::bind(&topencv::did_mouse_motion_paper, this, _1, _2, _3));

	treport* report = find_widget<treport>(window_, "examples", false, true);
	report->insert_item(null_str, "mouse");
	report->insert_item(null_str, "blend");
	report->insert_item(null_str, "video"); // markerless_ar
	report->set_did_item_changed(std::bind(&topencv::did_example_item_changed, this, _2));

	tslider* slider = find_widget<tslider>(window_, "slider1", false, true);
	slider->set_did_value_change(std::bind(&topencv::did_slider_value_changed, this, _1, _2, 0));
	slider = find_widget<tslider>(window_, "slider2", false, true);
	slider->set_did_value_change(std::bind(&topencv::did_slider_value_changed, this, _1, _2, 1));
}

void topencv::app_first_drawn()
{
	treport* report = find_widget<treport>(window_, "examples", false, true);
	report->select_item(0);
}

void topencv::example_mouse()
{
	persist_surf_ = create_neutral_surface(paper_->get_width(), paper_->get_height());
	temperate_surf_ = create_neutral_surface(paper_->get_width(), paper_->get_height());

	blits_.clear();
	surface text_surf = font::get_rendered_text(_("Press left mouse and motion to draw rectangle."), 0, font::SIZE_DEFAULT, font::BLACK_COLOR);
	blits_.push_back(image::tblit(text_surf, 0, 0, text_surf->w, text_surf->h));
	blits_.push_back(image::tblit(persist_surf_, 0, 0, persist_surf_->w, persist_surf_->h));
	blits_.push_back(image::tblit(temperate_surf_, 0, 0, temperate_surf_->w, temperate_surf_->h));
	paper_->immediate_draw();
}

void topencv::example_blend(double alpha)
{
	surface image1 = image::get_image("misc/documents.png");
	surface image2 = image::get_image("misc/disk.png");

	tsurface_2_mat_lock lock1(image1);
	tsurface_2_mat_lock lock2(image2);

	double beta = 1 - alpha;
	cv::Mat result;
	cv::addWeighted(lock1.mat, alpha, lock2.mat, beta, 0.0, result);

	blits_.clear();
	blits_.push_back(image::tblit(image1, 0, 0, 0, 0));
	blits_.push_back(image::tblit(image2, image1->w, 0, 0, 0));
	blits_.push_back(image::tblit(surface(result), image1->w / 2, image1->h, 0, 0));
	paper_->immediate_draw();
}

void topencv::example_markerless_ar()
{
	blits_.clear();
	paper_->immediate_draw();
}

void topencv::did_example_item_changed(ttoggle_button& widget)
{
	avcapture_.reset();
	paper_->set_timer_interval(0);

	current_example_ = widget.at();
	if (widget.at() == mouse) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);
		example_mouse();

	} else if (widget.at() == blend) {		
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::VISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::HIDDEN);

		example_blend(0.5);

	} else if (widget.at() == markerless_ar) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);

		avcapture_.reset(new tavcapture(0, *this, *this, tpoint(1280, nposm)));
		paper_->set_timer_interval(30);
		example_markerless_ar();
	}
}

void topencv::did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	trtc_client::VideoRenderer& vsink = *locals[0];
	ttexture_2_mat_lock mat_lock(vsink.tex_);
	ttexture_2_mat_lock cv_mat_lock(vsink.cv_tex_);

	if (vsink.new_frame) {
		cv::Mat edges;
		cv::cvtColor(mat_lock.mat, edges, cv::COLOR_BGRA2GRAY);
		cv::blur(edges, edges, cv::Size(7, 7));
		cv::Canny(edges, edges, 0, 30, 3);
		cv::cvtColor(edges, cv_mat_lock.mat, cv::COLOR_GRAY2BGRA);

		SDL_UnlockTexture(vsink.cv_tex_.get());
	}

	SDL_RenderCopy(renderer, vsink.cv_tex_.get(), NULL, &draw_rect);

	SDL_Rect dst = draw_rect;
	surface text_surf = font::get_rendered_text(_("Local video"), 0, 36, font::GOOD_COLOR);
	dst.w = text_surf->w;
	dst.h = text_surf->h;
	render_surface(renderer, text_surf, NULL, &dst);
}

void topencv::did_left_button_down_paper(ttrack& widget, const tpoint& coordinate)
{
	last_coordinate_ = coordinate;
}

static void cv_draw_rectangle(cv::RNG& rng, const cv::Mat& img, const cv::Rect& rect)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255), 255));
}

void topencv::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	if (current_example_ == mouse) {
		cv::Rect rect(first.x - widget.get_x(), first.y - widget.get_y(), last_coordinate_.x - first.x, last_coordinate_.y - first.y);
	
		tsurface_2_mat_lock lock(persist_surf_);
		cv_draw_rectangle(rng_, lock.mat, rect);
		paper_->immediate_draw();
	}
}

void topencv::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	last_coordinate_ = last;

	if (current_example_ == mouse) {
		cv::Rect rect(first.x - widget.get_x(), first.y - widget.get_y(), last.x - first.x, last.y - first.y);
		fill_surface(temperate_surf_, 0);
		tsurface_2_mat_lock lock(temperate_surf_);
		cv_draw_rectangle(rng_, lock.mat, rect);
		paper_->immediate_draw();
	}
}

void topencv::did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();

	for (std::vector<image::tblit>::const_iterator it = blits_.begin(); it != blits_.end(); ++ it) {
		const image::tblit& blit = *it;
		image::render_blit(renderer, blit, widget.get_x(), widget.get_y());
	}

	if (avcapture_.get()) {
		avcapture_->draw_slice(renderer, draw_rect);
	}
}

void topencv::did_slider_value_changed(tslider& widget, int value, int at)
{
	if (current_example_ == blend) {
		if (at != 0) {
			return;
		}
		double alpha = 1.0 * value / 100;
		example_blend(alpha);
	}
}

void topencv::post_show()
{
	if (avcapture_.get()) {
		avcapture_.reset();
	}
	paper_->set_timer_interval(0);
}

} // namespace gui2

