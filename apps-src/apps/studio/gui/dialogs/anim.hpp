#ifndef GUI_DIALOGS_ANIM_HPP_INCLUDED
#define GUI_DIALOGS_ANIM_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "animation.hpp"
#include "gui/dialogs/build.hpp"

namespace gui2 {

class ttoggle_button;
class ttoggle_panel;
class ttree;
class ttree_node;

class tanim_type2
{
public:
	tanim_type2(const std::string& id = null_str, const std::string& description = null_str)
		: id_(id)
		, description_(description)
		, variables_()
	{}

public:
	std::string id_;
	std::string description_;
	std::map<std::string, std::string> variables_;
};

//
// animation section

// SLG Maker          kingdom
// ---------------------------
// tparticular <==>   particular
// tanim2      <==>   unit_animation

class tparticular: public animated<unit_frame>
{
public:
	static std::string prefix_from_tag(const std::string& tag);

	explicit tparticular(int start_time=0, const frame_parsed_parameters &builder = frame_parsed_parameters(config()))
		: animated<unit_frame>(start_time)
		, parameters_(builder)
		, layer_(0)
		, zoom_x_(0)
		, zoom_y_(0)
		{};
	explicit tparticular(const config& cfg, const std::string& frame_string = "frame");

	std::string description() const;
	void update_to_ui_particular_edit(twindow& window, const std::string& frame_tag) const;
	void update_to_ui_frame_edit(twindow& window, int n) const;

	void paramsters_update_to_ui_anim_edit(ttree_node& branch, const std::string& name, const int param) const;
	void frame_update_to_ui_anim_edit(ttree_node& branch, const std::string& name, size_t n, size_t cookie_index) const;

	bool use_image(int n) const;

public:
	// animation params that can be locally overridden by frames
	frame_parsed_parameters parameters_;
	int layer_;
	int zoom_x_;
	int zoom_y_;
};

class tanim_
{
public:
	enum {PARAM_NONE = 0, PARAM_ANIM, PARAM_MODE, PARAM_FILTER, PARAM_TIME, PARAM_PARTICULAR, PARAM_FRAME = 100};

	tanim_() 
		: id_()
		, app_()
		, cfg_()
		, area_mode_(false)
		, template_(false)
	{}

	bool operator==(const tanim_& that) const
	{
		if (id_ != that.id_) return false;
		if (app_ != that.app_) return false;
		if (cfg_ != that.cfg_) return false;
		if (area_mode_ != that.area_mode_) return false;
		if (template_ != that.template_) return false;
		return true;
	}
	bool operator!=(const tanim_& that) const { return !operator==(that); }

public:
	std::string id_;
	std::string app_;
	bool area_mode_;
	bool template_;
	config cfg_;
};

class tanim2: public tanim_
{
public:
	static std::vector<tanim_type2> anim_types;
	static const tanim_type2& anim_type(const std::string& id);
	static tanim_type2 null_anim_type;

	typedef enum { HIT, MISS, KILL, INVALID} hit_type;

	tanim2()
		: tanim_()
		, anim_from_cfg_()
		, directions_()
		, primary_attack_filter_()
		, secondary_attack_filter_()
		, secondary_weapon_type_()
		, hits_()
		, unit_anim_()
		, sub_anims_()
	{
		if (anim_types.empty()) {
			prepare_anim_types();
		}
	}

	void from_config(const config& cfg);
	// void from_ui(HWND hdlgP);
	bool from_ui_particular_edit(twindow& window, int n);
	bool from_ui_filter_edit(twindow& window);
	bool from_ui_frame_edit(twindow& window, tparticular& l, int n, bool use_image);
	void update_to_ui_anim_edit(twindow& window);
	void update_to_ui_filter_edit(twindow& window) const;
	std::string generate() const;

	void change_particular_name(const std::string& from, const std::string& to);

	bool add_particular();
	void delete_particular(int n);

	void add_frame(tparticular& l, int n, bool front);
	void delete_frame(tparticular& l, int n);

	bool dirty() const;

	void parse(const config& cfg);
	int get_end_time() const;
	int get_begin_time() const;

	std::string filter_description() const;
	bool has_particular(const std::string& tag) const;
	std::string particular_frame_tag(const tparticular& l) const;

private:
	void prepare_anim_types();
	void filter_update_to_ui_anim_edit(ttree_node& branch) const;

public:
	tparticular unit_anim_;
	std::map<std::string, tparticular> sub_anims_;

	std::vector<std::pair<tparticular*, int> > frame_cookie_;

	// filter
	std::set<map_location::DIRECTION> directions_;
	std::set<hit_type> hits_;
	int feature_;
	int align_;
	std::vector<config> primary_attack_filter_;
	std::vector<config> secondary_attack_filter_;
	std::set<std::string> secondary_weapon_type_;

	tanim_ anim_from_cfg_;
};

class tanim3: public tdialog, public tbuild
{
public:
	explicit tanim3(const config& app_cfg);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	std::string anims_cfg(const std::string& app) const;
	void generate_anims_cfg() const;

	void prepare_anims();
	void insert_animation(twindow& window);
	void erase_animation(twindow& window);
	void do_edit_animation();
	void do_insert_particular();
	void do_erase_particular(const int at);
	void do_insert_frame(const int at, bool front);
	void do_erase_frame(const int at);

	bool anims_dirty() const;
	void did_app_changed(ttoggle_button& widget);
	void did_animation_changed(ttoggle_panel& widget);
	bool did_anim_id_changed(const std::string& label);
	void did_double_click_editor(ttree_node& widget);
	void did_right_button_up_editor(ttree_node& widget, const tpoint& coordinate);

	void update_build_button();

	bool require_build();
	void do_build();

	void app_work_start() override;
	void app_work_done() override;
	void app_handle_desc(const bool started, const int at, const bool ret)  override {}

private:
	const config& app_cfg_;

	int app_anim_start_;
	int clicked_anim_;

	std::vector<tanim2> anims_from_cfg_;
	std::vector<tanim2> anims_updating_;
	std::set<std::string> anim_apps_;
};

} // namespace gui2

#endif

