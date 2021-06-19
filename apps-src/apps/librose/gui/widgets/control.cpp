#define GETTEXT_DOMAIN "rose-lib"

#include "control.hpp"

#include "font.hpp"
#include "formula_string_utils.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "marked-up_text.hpp"
#include "area_anim.hpp"

using namespace std::placeholders;
#include <boost/foreach.hpp>

#include <iomanip>

namespace gui2 {

tout_of_chain_widget_lock::tout_of_chain_widget_lock(twidget& widget)
	: window_(nullptr)
{
	window_ = widget.get_window();
	window_->set_out_of_chain_widget(&widget);
}
tout_of_chain_widget_lock::~tout_of_chain_widget_lock()
{
	window_->set_out_of_chain_widget(nullptr);
}

void tfloat_widget::set_visible(bool visible)
{
	widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
}

bool tfloat_widget::is_visible() const
{
	return widget->get_visible() == twidget::VISIBLE;
}

tfloat_widget::~tfloat_widget()
{
	// when destruct, parent of widget must be window.
	// widget->set_parent(&window);
	// VALIDATE(widget->parent() == &window, null_str);
}

void tfloat_widget::set_ref_widget(tcontrol* widget, const tpoint& mouse)
{
	bool dirty = false;
	if (!widget) {
		VALIDATE(is_null_coordinate(mouse), null_str);
	}
	if (widget != ref_widget_) {
		if (ref_widget_) {
			ref_widget_->associate_float_widget(*this, false);
		}
		ref_widget_ = widget;
		dirty = true;
	}
	if (mouse != mouse_) {
		mouse_ = mouse;
		dirty = true;
	}
	if (dirty) {
		need_layout = true;
	}
}

void tfloat_widget::clear_texture()
{
	buf = nullptr;
	canvas = nullptr;
	widget->clear_texture();
}

tcontrol::tcontrol(const unsigned canvas_count)
	: label_()
	, label_size_(std::make_pair(nposm, tpoint(0, 0)))
	, text_editable_(false)
	, post_anims_()
	, integrate_(NULL)
	, tooltip_()
	, canvas_(canvas_count)
	, config_(NULL)
	, text_maximum_width_(0)
	, width_is_max_(false)
	, height_is_max_(false)
	, min_text_width_(0)
	, min_text_height_(0)
	, calculate_text_box_size_(false)
	, text_font_size_(0)
	, text_color_tpl_(0)
	, best_width_1th_(nposm)
	, best_height_1th_(nposm)
	, best_width_2th_("")
	, best_height_2th_("")
	, at_(nposm)
	, gc_distance_(nposm)
	, gc_width_(nposm)
	, gc_height_(nposm)
{
	connect_signal<event::SHOW_TOOLTIP>(std::bind(
			  &tcontrol::signal_handler_show_tooltip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::NOTIFY_REMOVE_TOOLTIP>(std::bind(
			  &tcontrol::signal_handler_notify_remove_tooltip
			, this
			, _2
			, _3));
}

tcontrol::~tcontrol()
{
	while (!post_anims_.empty()) {
		erase_animation(post_anims_.front());
	}
}

bool tcontrol::disable_click_dismiss() const
{
	return get_visible() == twidget::VISIBLE && get_active();
}

tpoint tcontrol::get_config_min_text_size() const
{
	VALIDATE(min_text_width_ >= config_->min_width && min_text_height_ >= config_->min_height, null_str);
	tpoint result(min_text_width_, min_text_height_);
	return result;
}

void tcontrol::layout_init(bool linked_group_only)
{
	// Inherited.
	twidget::layout_init(linked_group_only);
	if (!linked_group_only) {
		text_maximum_width_ = 0;
	}
}

void tcontrol::clear_text_maximum_width()
{
	text_maximum_width_ = 0;
}

int tcontrol::calculate_maximum_text_width(const int maximum_width) const
{
	int ret = maximum_width - config_->text_extra_width;
	if (config_->label_is_text) {
		ret -= config_->text_space_width;
	}
	return ret >=0 ? ret: 0;
}

// (1/2)width ==> text_maximum_width_
void tcontrol::set_text_maximum_width(int maximum)
{
	if (mtwusc_) {
		text_maximum_width_ = calculate_maximum_text_width(maximum);
	}
}

// (2/2)text_maximum_width_ ==> width
int tcontrol::get_multiline_best_width() const
{
	VALIDATE(mtwusc_, null_str);
	int ret = text_maximum_width_ + config_->text_extra_width;
	if (config_->label_is_text) {
		ret += config_->text_space_width;
	}
	return ret;
}

int tcontrol::best_width_from_text_width(const int text_width) const
{
	int ret = text_width + config_->text_extra_width;
	if (text_width && config_->label_is_text) {
		ret += config_->text_space_width;
	}
	return ret;
}

int tcontrol::best_height_from_text_height(const int text_height) const
{
	int ret = text_height + config_->text_extra_height;
	if (text_height && config_->label_is_text) {
		ret += config_->text_space_height;
	}
	return ret;
}

tpoint tcontrol::get_best_text_size(const int maximum_width, const int font_size) const
{
	VALIDATE(!label_.empty() && maximum_width > 0, null_str);

	if (label_size_.first == nposm || maximum_width != label_size_.first) {
		// Try with the minimum wanted size.
		label_size_.first = maximum_width;

		label_size_.second = font::get_rendered_text_size(label_, maximum_width, font_size, font::NORMAL_COLOR, text_editable_);
	}

	const tpoint& size = label_size_.second;
	VALIDATE(size.x <= maximum_width, null_str);

	return size;
}

int tcontrol::calculate_best_height_2th(const twindow& window) const
{
	int height = best_height_2th_(window.variables());
	if (height < 0) {
		height = 0;
	}
	if (best_height_2th_.has_multi_or_noninteger_formula()) {
		height *= twidget::hdpi_scale;
	} else {
		height = cfg_2_os_size(height);
	}
	return height;
}

tpoint tcontrol::best_size_from_builder() const
{
	tpoint result(nposm, nposm);
	const twindow* window = NULL;

	if (best_width_1th_ != nposm) {
		result.x = best_width_1th_;

	} else if (best_width_2th_.has_formula2()) {
		window = get_window();
		result.x = best_width_2th_(window->variables());
		if (result.x < 0) {
			result.x = 0;
		}
		if (best_width_2th_.has_multi_or_noninteger_formula()) {
			result.x *= twidget::hdpi_scale;
		} else {
			result.x = cfg_2_os_size(result.x);
		}
	}

	if (best_height_1th_ != nposm) {
		result.y = best_height_1th_;

	} else if (best_height_2th_.has_formula2()) {
		if (!window) {
			window = get_window();
		}
		result.y = calculate_best_height_2th(*window);
/*
		result.y = best_height_2th_(window->variables());
		if (result.y < 0) {
			result.y = 0;
		}
		if (best_height_2th_.has_multi_or_noninteger_formula()) {
			result.y *= twidget::hdpi_scale;
		} else {
			result.y = cfg_2_os_size(result.y);
		}
*/
	}
	return result;
}

tpoint tcontrol::calculate_best_size() const
{
	VALIDATE(config_, null_str);

	if (mtwusc_ && (clear_restrict_width_cell_size || !text_maximum_width_)) {
		return tpoint(0, 0);
	}

	// if has width/height field, use them. or calculate.
	tpoint cfg_size = best_size_from_builder();
	if (mtwusc_) {
		VALIDATE(config_->label_is_text && text_maximum_width_ > 0 && !width_is_max_, null_str);
		if (cfg_size.x == nposm) {
			// for mtwusc widget, send calculate_best_size only tow value:
			cfg_size.x = label_.empty()? config_->text_extra_width: get_multiline_best_width();
		}
	}
	if (cfg_size.x != nposm && !width_is_max_ && cfg_size.y != nposm && !height_is_max_) {
		return cfg_size;
	}

	// calculate text size.
	tpoint text_size(0, 0);
	if (config_->label_is_text) {
		if ((!text_editable_ || calculate_text_box_size_) && !label_.empty()) {
			int maximum_text_width = calculate_maximum_text_width(cfg_size.x == nposm || cfg_size.x == 0? INT32_MAX: cfg_size.x);
			
			if (text_maximum_width_ && maximum_text_width > text_maximum_width_) {
				maximum_text_width = text_maximum_width_;
			}

			const int font_size = get_text_font_size();
			text_size = get_best_text_size(maximum_text_width, font_size);
			if (text_size.x) {
				if (mtwusc_) {
					// to mtwusc_ widget, it's best text width must be text_maximum_width_.
					// so after update label_, as long as it doesn't change lines, don't trigger invalid layout window.
					// text_size.x = text_maximum_width_;
				}
				text_size.x += config_->text_space_width;
			}
			if (text_size.y) {
				if (text_size.y < font_size) {
					// if exist label_, text_size.y must be >= font_size.
					text_size.y = font_size;
				}
				text_size.y += config_->text_space_height;
			}
		}

	} else {
		VALIDATE(!text_editable_, null_str);
		text_size = mini_get_best_text_size();
	}

	// text size must >= minimum size. 
	const tpoint minimum = get_config_min_text_size();
	if (minimum.x > 0 && text_size.x < minimum.x) {
		text_size.x = minimum.x;
	}
	if (minimum.y > 0 && text_size.y < minimum.y) {
		text_size.y = minimum.y;
	}

	tpoint result(text_size.x + config_->text_extra_width, text_size.y + config_->text_extra_height);
	if (!width_is_max_) {
		if (cfg_size.x != nposm) {
			result.x = cfg_size.x;
		}
	} else {
		if (cfg_size.x != nposm && result.x >= cfg_size.x) {
			result.x = cfg_size.x;
		}
	}
	if (!height_is_max_) {
		if (cfg_size.y != nposm) {
			result.y = cfg_size.y;
		}
	} else {
		if (cfg_size.y != nposm && result.y >= cfg_size.y) {
			result.y = cfg_size.y;
		}
	}

	return result;
}

tpoint tcontrol::calculate_best_size_bh(const int width)
{
	if (!mtwusc_) {
		return tpoint(nposm, nposm);
	}

	// reference to http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=133&extra=page%3D1%26filter%3Dtypeid%26typeid%3D21
	set_text_maximum_width(width);
	tpoint size = get_best_size();
	if (size.x > config_->text_extra_width) {
		size.x = width;
	} else {
		VALIDATE(size.x == config_->text_extra_width, null_str);
		VALIDATE(label_.empty(), null_str);
	}
	return size;
}

void tcontrol::refresh_locator_anim(std::vector<tintegrate::tlocator>& locator)
{
	if (!text_editable_) {
		return;
	}
	locator_.clear();
	if (integrate_) {
		integrate_->fill_locator_rect(locator, true);
	} else {
		locator_ = locator;
	}
}

void tcontrol::set_blits(const std::vector<tformula_blit>& blits)
{ 
	blits_ = blits;
	set_dirty();
}

void tcontrol::set_blits(const tformula_blit& blit)
{
	blits_.clear();
	blits_.push_back(blit);
	set_dirty();
}

void tcontrol::place(const tpoint& origin, const tpoint& size)
{
	if (mtwusc_ && !label_.empty()) {
		// HORIZONTAL_ALIGN_LEFT, HORIZONTAL_ALIGN_CENTER or HORIZONTAL_ALIGN_RIGHT are <=
		VALIDATE(size.x <= text_maximum_width_ + config_->text_extra_width + config_->text_space_width, null_str);		
	}

	SDL_Rect previous_rect = ::create_rect(x_, y_, w_, h_);

	// resize canvasses
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_width(size.x);
		canvas.set_height(size.y);
	}

	// inherited
	twidget::place(origin, size);

	// update the state of the canvas after the sizes have been set.
	update_canvas();

	if (::create_rect(x_, y_, w_, h_) != previous_rect) {
		for (std::set<tfloat_widget*>::const_iterator it = associate_float_widgets_.begin(); it != associate_float_widgets_.end(); ++ it) {
			tfloat_widget& item = **it;
			item.need_layout = true;
		}
	}
}

void tcontrol::set_definition(const std::string& definition, const std::string& min_text_width, const std::string& min_text_height)
{
	VALIDATE(!config_, null_str);

	config_ = get_control(get_control_type(), definition);
	VALIDATE(config_, null_str);

	// min text width
	if (!min_text_width.empty()) {
		// now get_window() is nullptr, so use explicited variables.
		game_logic::map_formula_callable variables;
		get_screen_size_variables(variables);

		tformula<unsigned> formula(min_text_width);
		min_text_width_ = formula(variables);
		VALIDATE(min_text_width_ >= 0, null_str);

		if (formula.has_multi_or_noninteger_formula()) {
			min_text_width_ *= twidget::hdpi_scale;
		} else {
			min_text_width_ = cfg_2_os_size(min_text_width_);
		}
	}
	min_text_width_ = SDL_max(config_->min_width, min_text_width_);

	// min text height
	if (!min_text_height.empty()) {
		// now get_window() is nullptr, so use explicited variables.
		game_logic::map_formula_callable variables;
		get_screen_size_variables(variables);

		tformula<unsigned> formula(min_text_height);
		min_text_height_ = formula(variables);
		VALIDATE(min_text_height_ >= 0, null_str);

		if (formula.has_multi_or_noninteger_formula()) {
			min_text_height_ *= twidget::hdpi_scale;
		} else {
			min_text_height_ = cfg_2_os_size(min_text_height_);
		}
	}
	min_text_height_ = SDL_max(config_->min_height, min_text_height_);

	VALIDATE(canvas().size() == config_->state.size(), null_str);
	for (size_t i = 0; i < canvas_.size(); ++i) {
		canvas(i) = config()->state[i].canvas;
		canvas(i).start_animation();
	}

	const std::string border = mini_default_border();
	set_border(border);
	set_icon(null_str);

	update_canvas();

	load_config_extra();
}

void tcontrol::set_best_size_1th(const int width, bool width_is_max, const int height, bool height_is_max)
{
	if (width_is_max) {
		VALIDATE(width != nposm, null_str);
	}
	if (height_is_max) {
		VALIDATE(height != nposm, null_str);
	}

	bool require_invalidate_layout = false;

	if (best_width_1th_ != width) {
		best_width_1th_ = width;
		require_invalidate_layout = true;
	}
	if (width_is_max != width_is_max_) {
		width_is_max_ = width_is_max;
		require_invalidate_layout = true;
	}

	if (best_height_1th_ != height) {
		best_height_1th_ = height;
		require_invalidate_layout = true;
	}
	if (height_is_max != height_is_max_) {
		height_is_max_ = height_is_max;
		require_invalidate_layout = true;
	}
	
	if (require_invalidate_layout) {
		trigger_invalidate_layout(get_window());
	}
}

void tcontrol::set_best_size_2th(const std::string& width, bool width_is_max, const std::string& height, bool height_is_max)
{
	if (width_is_max) {
		VALIDATE(!width.empty(), null_str);
	}
	if (height_is_max) {
		VALIDATE(!height.empty(), null_str);
	}

	best_width_2th_ = tformula<unsigned>(width);
	width_is_max_ = width_is_max;

	best_height_2th_ = tformula<unsigned>(height);
	height_is_max_ = height_is_max;
}

void tcontrol::set_mtwusc()
{
	VALIDATE(best_width_1th_ == nposm && !best_width_2th_.has_formula(), null_str);
	VALIDATE(best_height_1th_ == nposm && !best_height_2th_.has_formula(), null_str);
	VALIDATE(config_->label_is_text, null_str);
	VALIDATE(min_text_width_ == config_->min_width, null_str);
	VALIDATE(!mtwusc_, null_str);

	twidget::set_mtwusc();
}

void tcontrol::set_icon(const std::string& icon) 
{
	const size_t pos = icon.rfind(".");
	if (config_->icon_is_main && pos != std::string::npos && pos + 4 == icon.size()) {
		set_canvas_variable("icon", variant(icon.substr(0, pos))); 	
	} else {
		set_canvas_variable("icon", variant(icon)); 
	}
}

void tcontrol::set_label(const std::string& label)
{
	VALIDATE(w_ >= 0 && h_ >= 0, null_str);
	VALIDATE(!text_editable_, null_str);

	if (label == label_) {
		return;
	}

	label_ = label;
	update_canvas();
	set_dirty();
	if (!config_->label_is_text) {
		VALIDATE(label_size_.first == nposm, null_str);
		return;
	}

	label_size_.first = nposm;

	if (disalbe_invalidate_layout || float_widget_) {
		return;
	}
	twindow* window = get_window();
	if (!window || !window->layouted()) {
		// when called by tbuilder_control, window will be nullptr.
		return;
	}

	if (me_or_parent_has_invisible(*this)) {
		return;
	}

	bool require_invalidate_layout = false;
	if (w_ == 0) {
		if (label_.empty()) {
			return;
		}
		require_invalidate_layout = true;
	}

	if (!require_invalidate_layout) {
		if (parent_ != nullptr && parent_->is_grid()) {
			require_invalidate_layout = static_cast<tgrid*>(parent_)->require_invalidate_layout(*this);
		}
	}

	if (require_invalidate_layout) {
		trigger_invalidate_layout(window);
	}
}

void tcontrol::set_text_editable(bool editable)
{
	if (editable == text_editable_) {
		return;
	}

	text_editable_ = editable;
	update_canvas();
	set_dirty();
}

void tcontrol::update_canvas()
{
	// set label in canvases
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable("text", variant(label_));
	}
}

void tcontrol::clear_texture()
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.clear_texture();
	}
}

bool tcontrol::exist_anim()
{
	if (!post_anims_.empty()) {
		return true;
	}
	return !canvas_.empty() && canvas_[get_state()].exist_anim();
}

int tcontrol::insert_animation(const ::config& cfg)
{
	int id = start_cycle_float_anim(cfg);
	if (id != nposm) {
		std::vector<int>& anims = post_anims_;
		anims.push_back(id);
	}
	return id;
}

void tcontrol::erase_animation(int id)
{
	bool found = false;
	std::vector<int>::iterator find = std::find(post_anims_.begin(), post_anims_.end(), id);
	if (find != post_anims_.end()) {
		post_anims_.erase(find);
		found = true;
	}
	if (found) {
		anim2::manager::instance->erase_area_anim(id);
		set_dirty();
	}
}

void tcontrol::set_canvas_variable(const std::string& name, const variant& value)
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable(name, value);
	}
	set_dirty();
}

int tcontrol::get_text_font_size() const
{
	int ret = text_font_size_? text_font_size_: font::SIZE_DEFAULT;
	if (ret >= font_min_cfg_size) {
		ret = font_hdpi_size_from_cfg_size(ret);
	}
	return ret;
}

int tcontrol::get_maximum_text_width_4_camvas() const
{
	if (mtwusc_) {
		VALIDATE(text_maximum_width_ > 0, null_str);
		return text_maximum_width_;
	}

	// To be accurate, it should be w_ - config_->text_extra_width + config_->text_space_width, 
	// but sometimes it may be necessary to hope for more.
	VALIDATE(w_ > 0, null_str);

	if (width_is_max_) {
		// temperal method, require discard in future. special for gui2::message,
		return w_;
	}
	return 0; // it is not mtwusc_, use single line.
}

void tcontrol::impl_draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	const SDL_Rect blitting_rectangle = calculate_blitting_rectangle(x_offset, y_offset);
	canvas(get_state()).blit(*this, frame_buffer, blitting_rectangle, get_dirty(), post_anims_);
}

texture tcontrol::get_canvas_tex()
{
	VALIDATE(get_dirty() || get_redraw(), null_str);

	return canvas(get_state()).get_canvas_tex(*this, post_anims_);
}

void tcontrol::associate_float_widget(tfloat_widget& item, bool associate)
{
	if (associate) {
		associate_float_widgets_.insert(&item);
	} else {
		std::set<tfloat_widget*>::iterator it = associate_float_widgets_.find(&item);
		if (it != associate_float_widgets_.end()) {
			associate_float_widgets_.erase(it);
		}
	}
}

void tcontrol::signal_handler_show_tooltip(
		  const event::tevent event
		, bool& handled
		, const tpoint& location)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	return;
#endif

	if (!tooltip_.empty()) {
		std::string tip = tooltip_;
		event::tmessage_show_tooltip message(tip, *this, location);
		handled = fire(event::MESSAGE_SHOW_TOOLTIP, *this, message);
	}
}

void tcontrol::signal_handler_notify_remove_tooltip(
		  const event::tevent event
		, bool& handled)
{
	/*
	 * This makes the class know the tip code rather intimately. An
	 * alternative is to add a message to the window to remove the tip.
	 * Might be done later.
	 */
	get_window()->remove_tooltip();
	// tip::remove();

	handled = true;
}

tbase_tpl_widget::tbase_tpl_widget(twindow& window, twidget& widget)
	: window_(window)
	, widget2_(widget)
{
	window_.register_tpl_widget(widget, this);
}

} // namespace gui2

