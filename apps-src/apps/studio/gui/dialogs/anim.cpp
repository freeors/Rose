#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/anim.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/anim_particular.hpp"
#include "gui/dialogs/anim_filter.hpp"
#include "gui/dialogs/anim_frame.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "hero.hpp"
#include "serialization/parser.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, anim3)

static const std::string default_new_anim_id = "__new_anim";
static const std::string default_particular_frame_tag = "__new_frame";
static std::set<std::string> reserved_particular_frame_tag;

static const char* align_filter_names[] = {
	"none",
	"x",
	"non-x",
	"y",
	"non-y"
};

std::vector<tanim_type2> tanim2::anim_types;
tanim_type2 tanim2::null_anim_type;

const tanim_type2& tanim2::anim_type(const std::string& id)
{
	std::vector<tanim_type2>::const_iterator it = anim_types.begin();
	for (; it != anim_types.end(); ++ it) {
		const tanim_type2& type = *it;
		if (type.id_ == id) {
			return type;
		}
	}
	return null_anim_type;
}

tparticular::tparticular(const config& cfg, const std::string& frame_string)
	: animated<unit_frame>()
	, parameters_(config())
{
	config::const_child_itors range = cfg.child_range(frame_string + "frame");
	starting_frame_time_ = INT_MAX;
	if (cfg[frame_string + "start_time"].empty() &&range.first != range.second) {
		BOOST_FOREACH (const config &frame, range) {
			starting_frame_time_ = std::min(starting_frame_time_, frame["begin"].to_int());
		}
	} else {
		starting_frame_time_ = cfg[frame_string + "start_time"];
	}

	cycles_ = cfg[frame_string + "cycles"].to_bool();

	BOOST_FOREACH (const config &frame, range)
	{
		unit_frame tmp_frame(frame);
		add_frame(tmp_frame.duration(),tmp_frame,!tmp_frame.does_not_change());
	}
	parameters_ = frame_parsed_parameters(cfg, frame_string, get_animation_duration());
	if(!parameters_.does_not_change()  ) {
			force_change();
	}

	layer_ = cfg[frame_string + "layer"].to_int();
	zoom_x_ = cfg[frame_string + "zoom_x"].to_int();
	zoom_y_ = cfg[frame_string + "zoom_y"].to_int();
}

std::string tparticular::description() const
{
	std::stringstream strstr;

	if (!frames_.empty()) {
		strstr << "(" << get_begin_time() << ", " << get_end_time() << ") ";
		strstr << _("frame^Count") << "(" << frames_.size() << ") ";
	} else {
		strstr << _("None");
	}
	return strstr.str();
}

void tparticular::paramsters_update_to_ui_anim_edit(ttree_node& branch, const std::string& name, const int param) const
{
	std::map<std::string, std::string> data;

	std::stringstream ss;
	ss.str("");
	ss << name << ": " << description();
	data["label"] = ss.str();
	ttree_node& htvi_particular = branch.insert_node("default", data);
	htvi_particular.set_child_icon("label", "misc/particual.png");
	htvi_particular.set_cookie(param);

	ss.str("");
	ss << _("Start time") << ": ";
	ss << starting_frame_time_;
	data["label"] = ss.str();
	ttree_node* htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");

	ss.str("");
	ss << "cycles" << ": ";
	ss << (cycles_? "yes": "no");
	data["label"] = ss.str();
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");

	if (!parameters_.x_.get_original().empty()) {
		ss.str("");
		ss << "x" << ": ";
		ss << parameters_.x_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!parameters_.y_.get_original().empty()) {
		ss.str("");
		ss << "y" << ": ";
		ss << parameters_.y_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!parameters_.offset_x_.get_original().empty()) {
		ss.str("");
		ss << "offset_x" << ": ";
		ss << parameters_.offset_x_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!parameters_.offset_y_.get_original().empty()) {
		ss.str("");
		ss << "offset_y" << ": ";
		ss << parameters_.offset_y_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (layer_) {
		ss.str("");
		ss << "layer" << ": ";
		ss << layer_;
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (zoom_x_) {
		ss.str("");
		ss << "zoom_x" << ": ";
		ss << zoom_x_;
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (zoom_y_) {
		ss.str("");
		ss << "zoom_y" << ": ";
		ss << zoom_y_;
		data["label"] = ss.str();
		htvi = &htvi_particular.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	htvi_particular.unfold();
}

void tparticular::frame_update_to_ui_anim_edit(ttree_node& branch, const std::string& name, size_t n, size_t cookie_index) const
{
	std::map<std::string, std::string> data;

	const animated<unit_frame>::frame& f = frames_[n];
	const frame_parsed_parameters& builder = f.value_.get_builder();

	std::stringstream ss;
	ss.str("");
	ss << name;
	data["label"] = ss.str();
	ttree_node& htvi_frame = branch.insert_node("default", data);
	htvi_frame.set_child_icon("label", "misc/frame.png");
	htvi_frame.set_cookie(tanim_::PARAM_FRAME + cookie_index);

	ss.str("");
	ss << "duration" << ": ";
	ss << f.duration_;
	data["label"] = ss.str();
	ttree_node* htvi = &htvi_frame.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");

	if (!builder.image_.empty()) {
		ss.str("");
		ss << "image" << ": ";
		ss << builder.image_;
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.image_mod_.empty()) {
		ss.str("");
		ss << "image_mod" << ": ";
		ss << builder.image_mod_;
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.image_diagonal_.get_filename().empty()) {
		ss.str("");
		ss << "image_diagonal" << ": ";
		ss << builder.image_diagonal_.get_filename();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.image_horizontal_.get_filename().empty()) {
		ss.str("");
		ss << "image_horizontal" << ": ";
		ss << builder.image_horizontal_.get_filename();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.x_.get_original().empty()) {
		ss.str("");
		ss << "x" << ": ";
		ss << builder.x_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.y_.get_original().empty()) {
		ss.str("");
		ss << "y" << ": ";
		ss << builder.y_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.halo_.get_original().empty()) {
		ss.str("");
		ss << "halo" << ": ";
		ss << builder.halo_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.offset_x_.get_original().empty()) {
		ss.str("");
		ss << "offset_x" << ": ";
		ss << builder.offset_x_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.offset_y_.get_original().empty()) {
		ss.str("");
		ss << "offset_y" << ": ";
		ss << builder.offset_y_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.auto_hflip_) {
		ss.str("");
		ss << "auto_hflip" << ": no";
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.auto_vflip_) {
		ss.str("");
		ss << "auto_vflip" << ": no";
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.sound_.empty()) {
		ss.str("");
		ss << "sound" << ": ";
		ss << builder.sound_;
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.stext_.empty()) {
		ss.str("");
		ss << "stext" << ": ";
		ss << builder.stext_;
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (builder.font_size_) {
		ss.str("");
		ss << "font_size" << ": ";
		ss << builder.font_size_;
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (builder.text_color_ != frame_parameters::default_text_color) {
		ss.str("");
		ss << "text_color" << ": ";
		ss << ((builder.text_color_ & 0x00FF0000) >> 16) << "," << ((builder.text_color_ & 0x0000FF00) >> 8) << "," << (builder.text_color_ & 0x000000FF);
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	if (!builder.highlight_ratio_.get_original().empty()) {
		ss.str("");
		ss << "alpha" << ": ";
		ss << builder.highlight_ratio_.get_original();
		data["label"] = ss.str();
		htvi = &htvi_frame.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}

	htvi_frame.unfold();
}

std::string tparticular::prefix_from_tag(const std::string& tag)
{
	if (tag == "frame") {
		return null_str;
	}
	return tag.substr(0, tag.size() - 6);
}

void tparticular::update_to_ui_particular_edit(twindow& window, const std::string& frame_tag) const
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "prefix", false, true);
	std::string prefix = prefix_from_tag(frame_tag);
	if (prefix.empty()) {
		prefix = _("Master particular");
		text_box->set_active(false);
	}
	text_box->set_label(prefix);

	tlabel* label = find_widget<tlabel>(&window, "tag", false, true);
	label->set_label(frame_tag);

	text_box = find_widget<ttext_box>(&window, "start_time", false, true);
	text_box->set_label(str_cast(starting_frame_time_));

	find_widget<tcontrol>(&window, "cycles", false).set_active(cycles_);
	find_widget<ttext_box>(&window, "x", false).set_label(parameters_.x_.get_original());
	find_widget<ttext_box>(&window, "y", false).set_label(parameters_.y_.get_original());
	find_widget<ttext_box>(&window, "offset_x", false).set_label(parameters_.offset_x_.get_original());
	find_widget<ttext_box>(&window, "offset_y", false).set_label(parameters_.offset_y_.get_original());

	find_widget<ttext_box>(&window, "layer", false).set_label(str_cast(layer_));
	// UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_X), zoom_x_);
	// UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_Y), zoom_y_);
}

bool tparticular::use_image(int n) const
{
	const animated<unit_frame>::frame& f = frames_[n];
	const frame_parsed_parameters& builder = f.value_.get_builder();
	return !builder.image_.empty();
}

void tparticular::update_to_ui_frame_edit(twindow& window, int n) const
{
	const animated<unit_frame>::frame& f = frames_[n];

	const frame_parsed_parameters& builder = f.value_.get_builder();

	find_widget<ttext_box>(&window, "duration", false, true)->set_label(str_cast(builder.duration_));

	find_widget<ttext_box>(&window, "image", false, true)->set_label(builder.image_);
	find_widget<ttext_box>(&window, "image_mod", false, true)->set_label(builder.image_mod_);
	find_widget<ttext_box>(&window, "image_diagonal", false, true)->set_label(builder.image_diagonal_.get_filename());
	find_widget<ttext_box>(&window, "image_horizontal", false, true)->set_label(builder.image_horizontal_.get_filename());
	find_widget<ttext_box>(&window, "x", false, true)->set_label(builder.x_.get_original());
	find_widget<ttext_box>(&window, "y", false, true)->set_label(builder.y_.get_original());
	find_widget<ttext_box>(&window, "halo", false, true)->set_label(builder.halo_.get_original());
	find_widget<ttext_box>(&window, "offset_x", false, true)->set_label(builder.offset_x_.get_original());
	find_widget<ttext_box>(&window, "offset_y", false, true)->set_label(builder.offset_y_.get_original());
	find_widget<ttoggle_button>(&window, "auto_hflip", false, true)->set_value(builder.auto_hflip_);
	find_widget<ttoggle_button>(&window, "auto_vflip", false, true)->set_value(builder.auto_vflip_);
	find_widget<ttext_box>(&window, "sound", false, true)->set_label(builder.sound_.c_str());

	find_widget<ttext_box>(&window, "stext", false, true)->set_label(builder.stext_.c_str());
	find_widget<ttext_box>(&window, "font_size", false, true)->set_label(str_cast(builder.font_size_));

	find_widget<ttext_box>(&window, "text_color", false, true)->set_label(encode_color(builder.text_color_));
	find_widget<ttext_box>(&window, "alpha", false, true)->set_label(builder.highlight_ratio_.get_original());
}

static void fill_param_area_anim(tanim_type2& anim)
{
	anim.variables_.insert(std::make_pair("8800", "x"));
	anim.variables_.insert(std::make_pair("8801", "y"));
	anim.variables_.insert(std::make_pair("8802", "offset_x"));
	anim.variables_.insert(std::make_pair("8803", "offset_y"));
	anim.variables_.insert(std::make_pair("8810", "alpha"));
	anim.variables_.insert(std::make_pair("__image", "image"));
	anim.variables_.insert(std::make_pair("__text", "text"));
}

void tanim2::prepare_anim_types()
{
	anim_types.push_back(tanim_type2("defend", _("anim^defend")));
	anim_types.back().variables_.insert(std::make_pair("$base_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$hit_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$miss_png", null_str));

	anim_types.push_back(tanim_type2("resistance", _("anim^resistance")));
	anim_types.back().variables_.insert(std::make_pair("$leading_png", null_str));

	anim_types.push_back(tanim_type2("leading", _("anim^leading")));
	anim_types.back().variables_.insert(std::make_pair("$leading_png", null_str));

	anim_types.push_back(tanim_type2("healing", _("anim^healing")));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("idle", _("anim^idle")));
	anim_types.back().variables_.insert(std::make_pair("$idle_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_4_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

	anim_types.push_back(tanim_type2("multi_idle", _("anim^multi_idle")));
	anim_types.back().variables_.insert(std::make_pair("$idle_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_4_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

	anim_types.push_back(tanim_type2("healed", _("anim^healed")));
	anim_types.back().variables_.insert(std::make_pair("$idle_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$idle_4_png", null_str));

	anim_types.push_back(tanim_type2("movement", _("anim^movement")));
	anim_types.back().variables_.insert(std::make_pair("$move_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$move_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

	anim_types.push_back(tanim_type2("build", _("anim^build")));
	anim_types.back().variables_.insert(std::make_pair("$build_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$build_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$build_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$build_4_png", null_str));

	anim_types.push_back(tanim_type2("repair", _("anim^repair")));
	anim_types.back().variables_.insert(std::make_pair("$repair_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$repair_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$repair_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$repair_4_png", null_str));

	anim_types.push_back(tanim_type2("die", _("anim^die")));
	anim_types.back().variables_.insert(std::make_pair("$die_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$die_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

	anim_types.push_back(tanim_type2("multi_die", _("anim^multi_die")));
	anim_types.back().variables_.insert(std::make_pair("$die_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$die_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$sound_ogg", null_str));

	anim_types.push_back(tanim_type2("melee_attack", _("anim^melee_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$hit_sound", null_str));
	anim_types.back().variables_.insert(std::make_pair("$miss_sound", null_str));
	anim_types.back().variables_.insert(std::make_pair("$melee_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$melee_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$melee_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$melee_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("ranged_attack", _("anim^ranged_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$image_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$image_diagonal_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$image_horizontal_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$hit_sound", null_str));
	anim_types.back().variables_.insert(std::make_pair("$miss_sound", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("magic_missile_attack", _("anim^magic_missile_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("lightbeam_attack", _("anim^lightbeam_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("fireball_attack", _("anim^fireball_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("iceball_attack", _("anim^iceball_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("lightning_attack", _("anim^lightning_attack")));
	anim_types.back().variables_.insert(std::make_pair("$attack_id", null_str));
	anim_types.back().variables_.insert(std::make_pair("$range", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_1_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_2_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_3_png", null_str));
	anim_types.back().variables_.insert(std::make_pair("$ranged_attack_4_png", null_str));

	anim_types.push_back(tanim_type2("card", _("anim^card")));
	anim_types.push_back(tanim_type2("reinforce", _("anim^reinforce")));
	anim_types.push_back(tanim_type2("individuality", _("anim^individuality")));
	anim_types.push_back(tanim_type2("tactic", _("anim^tactic")));
	anim_types.push_back(tanim_type2("blade", _("anim^blade")));
	anim_types.push_back(tanim_type2("fire", _("anim^fire")));
	anim_types.push_back(tanim_type2("magic", _("anim^magic")));
	anim_types.push_back(tanim_type2("heal", _("anim^heal")));
	anim_types.push_back(tanim_type2("destruct", _("anim^destruct")));
	anim_types.push_back(tanim_type2("formation_attack", _("anim^formation attack")));
	anim_types.push_back(tanim_type2("formation_defend", _("anim^formation defend")));
	anim_types.push_back(tanim_type2("pass_scenario", _("anim^pass scenario")));
	anim_types.push_back(tanim_type2("perfect", _("anim^perfect")));
	anim_types.push_back(tanim_type2("income", _("anim^income")));
	anim_types.push_back(tanim_type2("stratagem_up", _("anim^stratagem_up")));
	anim_types.push_back(tanim_type2("stratagem_down", _("anim^stratagem_down")));
	anim_types.push_back(tanim_type2("location", _("anim^location")));
	anim_types.push_back(tanim_type2("hscroll_text", _("anim^hscroll text")));
	anim_types.push_back(tanim_type2("title_screen", _("anim^title_screen")));
	anim_types.push_back(tanim_type2("load_scenario", _("anim^load_scenario")));

	anim_types.push_back(tanim_type2("flags", _("anim^flags")));
	fill_param_area_anim(anim_types.back());

	anim_types.push_back(tanim_type2("text", _("anim^text")));
	fill_param_area_anim(anim_types.back());

	anim_types.push_back(tanim_type2("place", _("anim^place")));
	fill_param_area_anim(anim_types.back());
}

void tanim2::from_config(const config& cfg)
{
	id_ = cfg["id"].str();
	app_ = cfg["app"].str();
	template_ = cfg["template"].to_bool();
	cfg_ = cfg.child("anim");
	anim_from_cfg_ = *this;

	parse(cfg_);
}

void tanim2::parse(const config& cfg)
{
	const std::string& frame_string = "";

	sub_anims_.clear();
	
	directions_.clear();
	hits_.clear();
	primary_attack_filter_.clear();
	secondary_attack_filter_.clear();
	secondary_weapon_type_.clear();

	unit_anim_ = tparticular(cfg, "");
	BOOST_FOREACH (const config::any_child &fr, cfg.all_children_range())
	{
		if (fr.key == frame_string) continue;
		if (fr.key.find("_frame", fr.key.size() - 6) == std::string::npos) continue;
		if (sub_anims_.find(fr.key) != sub_anims_.end()) continue;
		sub_anims_[fr.key] = tparticular(cfg_, fr.key.substr(0, fr.key.size() - 5));
	}

	const std::vector<std::string>& my_directions = utils::split(cfg["direction"]);
	for(std::vector<std::string>::const_iterator i = my_directions.begin(); i != my_directions.end(); ++i) {
		const map_location::DIRECTION d = map_location::parse_direction(*i);
		directions_.insert(d);
	}

	feature_ = cfg["feature"].to_int(HEROS_NO_FEATURE);
	align_ = cfg["align"].to_int(ALIGN_NONE);


	std::vector<std::string> hits_str = utils::split(cfg["hits"]);
	std::vector<std::string>::iterator hit;
	for (hit=hits_str.begin() ; hit != hits_str.end() ; ++hit) {
		if(*hit == "yes" || *hit == "hit") {
			hits_.insert(HIT);
		}
		if(*hit == "no" || *hit == "miss") {
			hits_.insert(MISS);
		}
		if(*hit == "yes" || *hit == "kill" ) {
			hits_.insert(KILL);
		}
	}
	BOOST_FOREACH (const config &filter, cfg.child_range("filter_attack")) {
		primary_attack_filter_.push_back(filter);
		break;
	}
	BOOST_FOREACH (const config &filter, cfg.child_range("filter_second_attack")) {
		secondary_attack_filter_.push_back(filter);
		break;
	}

	std::vector<std::string> vstr = utils::split(cfg["secondary_weapon_type"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		secondary_weapon_type_.insert(*it);
	}
	area_mode_ = cfg["area_mode"].to_bool();
}

void tanim2::change_particular_name(const std::string& from, const std::string& to)
{
	VALIDATE(!to.empty(), null_str);

	const std::string prefix_from = tparticular::prefix_from_tag(from);
	const std::string prefix_to = tparticular::prefix_from_tag(to);

	std::map<std::string, std::string> modifies;
	modifies.insert(std::make_pair(prefix_from + "_start_time", prefix_to + "_start_time"));
	modifies.insert(std::make_pair(prefix_from + "_x", prefix_to + "_x"));
	modifies.insert(std::make_pair(prefix_from + "_y", prefix_to + "_y"));
	modifies.insert(std::make_pair(prefix_from + "_offset", prefix_to + "_offset"));
	
	for (std::map<std::string, std::string>::const_iterator it = modifies.begin(); it != modifies.end(); ++ it) {
		if (cfg_.has_attribute(it->first)) {
			cfg_[it->second] = cfg_[it->first];
			cfg_.remove_attribute(it->first);
		}
	}

	config cfg;
	BOOST_FOREACH (const config& frame, cfg_.child_range(from)) {
		cfg.add_child(to, frame);
	}
	cfg_.clear_children(from);
	cfg_.append_children(cfg);
}

bool tanim2::from_ui_particular_edit(twindow& window, int n)
{
	tparticular* l = &unit_anim_;
	std::string frame_tag_prefix = "";
	if (n > 0) {
		std::map<std::string, tparticular>::iterator it = sub_anims_.begin();
		std::advance(it, n - 1);
		l = &it->second;
		frame_tag_prefix = it->first.substr(0, it->first.size() - 5);
	}

	std::stringstream ss;
	std::set<std::string> attribute;
	config particular_cfg;
	
	ttext_box* text_box = find_widget<ttext_box>(&window, "start_time", false, true);
	int start_time = utils::to_int(text_box->label());
	ss.str("");
	ss << frame_tag_prefix << "start_time";
	if (start_time) {
		particular_cfg[ss.str()] = start_time;
	}
	attribute.insert(ss.str());

	std::string text = find_widget<ttext_box>(&window, "x", false, true)->label();
	ss.str("");
	ss << frame_tag_prefix << "x";
	if (!text.empty()) {
		particular_cfg[ss.str()] = text;
		
	}
	attribute.insert(ss.str());

	text = find_widget<ttext_box>(&window, "y", false, true)->label();
	ss.str("");
	ss << frame_tag_prefix << "y";
	if (!text.empty()) {
		particular_cfg[ss.str()] = text;
		
	}
	attribute.insert(ss.str());

	text = find_widget<ttext_box>(&window, "offset_x", false, true)->label();
	ss.str("");
	ss << frame_tag_prefix << "offset_x";
	if (!text.empty()) {
		particular_cfg[ss.str()] = text;
	}
	attribute.insert(ss.str());

	text = find_widget<ttext_box>(&window, "offset_y", false, true)->label();
	ss.str("");
	ss << frame_tag_prefix << "offset_y";
	if (!text.empty()) {
		particular_cfg[ss.str()] = text;
	}
	attribute.insert(ss.str());

	bool cycles = find_widget<tcontrol>(&window, "cycles", false, true)->get_active();
	ss.str("");
	ss << frame_tag_prefix << "cycles";
	if (cycles) {
		particular_cfg[ss.str()] = "yes";
	}
	attribute.insert(ss.str());

	text_box = find_widget<ttext_box>(&window, "layer", false, true);
	int layer = utils::to_int(text_box->label());
	ss.str("");
	ss << frame_tag_prefix << "layer";
	if (layer) {
		particular_cfg[ss.str()] = layer;
	}
	attribute.insert(ss.str());
/*
	int zoom = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_X));
	strstr.str("");
	strstr << frame_tag_prefix << "zoom_x";
	if (zoom) {
		particular_cfg[strstr.str()] = zoom;
	}
	attribute.insert(strstr.str());

	zoom = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_ANIMPARTICULAR_ZOOM_Y));
	strstr.str("");
	strstr << frame_tag_prefix << "zoom_y";
	if (zoom) {
		particular_cfg[strstr.str()] = zoom;
	}
	attribute.insert(strstr.str());
*/
	bool changed = false;
	for (std::set<std::string>::const_iterator it = attribute.begin(); it != attribute.end(); ++ it) {
		const std::string& attri = *it;
		bool a_has = particular_cfg.has_attribute(attri);
		bool b_has = cfg_.has_attribute(attri);
		if (a_has && !b_has) {
			cfg_[attri] = particular_cfg[attri];
		} else if (!a_has && b_has) {
			cfg_.remove_attribute(attri);
		} else if (a_has && b_has && particular_cfg[attri] != cfg_[attri]) {
			cfg_[attri] = particular_cfg[attri];
		} else {
			continue;
		}
		changed = true;
	}

	if (n > 0) {
		std::map<std::string, tparticular>::iterator it = sub_anims_.begin();
		std::advance(it, n - 1);

		std::string user_frame_tag = find_widget<tcontrol>(&window, "tag", false, true)->label();
		if (user_frame_tag != it->first) {
			change_particular_name(it->first, user_frame_tag);
		}
		changed = true;
	}
	return changed;
}

bool tanim2::from_ui_filter_edit(twindow& window)
{
	std::set<hit_type> hits;
	
	if (find_widget<ttoggle_button>(&window, "hit", false, true)->get_value()) {
		hits.insert(HIT);
	}
	if (find_widget<ttoggle_button>(&window, "miss", false, true)->get_value()) {
		hits.insert(MISS);
	}
	if (find_widget<ttoggle_button>(&window, "kill", false, true)->get_value()) {
		hits.insert(KILL);
	}
	
	std::set<map_location::DIRECTION> directions;
	if (find_widget<ttoggle_button>(&window, "n", false, true)->get_value()) {
		directions.insert(map_location::NORTH);
	}
	if (find_widget<ttoggle_button>(&window, "ne", false, true)->get_value()) {
		directions.insert(map_location::NORTH_EAST);
	}
	if (find_widget<ttoggle_button>(&window, "nw", false, true)->get_value()) {
		directions.insert(map_location::NORTH_WEST);
	}
	if (find_widget<ttoggle_button>(&window, "s", false, true)->get_value()) {
		directions.insert(map_location::SOUTH);
	}
	if (find_widget<ttoggle_button>(&window, "se", false, true)->get_value()) {
		directions.insert(map_location::SOUTH_EAST);
	}
	if (find_widget<ttoggle_button>(&window, "sw", false, true)->get_value()) {
		directions.insert(map_location::SOUTH_WEST);
	}
/*
	std::set<std::string> secondary_weapon_type;
#ifndef _ROSE_EDITOR
	const std::vector<std::string>& atype_ids = unit_types.atype_ids();
	for (size_t n = 0; n < atype_ids.size(); n ++) {
		// IDM of weapon type checkbox must align with atype!
		if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_BLADE + n))) {
			secondary_weapon_type.insert(atype_ids[n]);
		}
	}
#endif	
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_FEATURE);
	int feature = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ALIGN);
	int align = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	// primary_attack_filter
	config primary_attack_filter;
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ID);
	ComboBox_GetText(hctl, text, _MAX_PATH);
	if (text[0] != '\0') {
		primary_attack_filter["name"] = text;
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_RANGE);
	ComboBox_GetText(hctl, text, _MAX_PATH);
	if (text[0] != '\0') {
		primary_attack_filter["range"] = text;
	}
*/
	// update fitler to cfg_
	std::stringstream ss;
	bool changed = false;

	if (hits != hits_) {
		changed = true;
		ss.str("");
		for (std::set<hit_type>::const_iterator it = hits.begin(); it != hits.end(); ++ it) {
			if (it != hits.begin()) {
				ss << ", ";
			}
			if (*it == HIT) {
				ss << "hit";
			} else if (*it == MISS) {
				ss << "miss";
			} else if (*it == KILL) {
				ss << "kill";
			} else {
				ss << "invalid";
			} 
		}
		if (!ss.str().empty()) {
			cfg_["hits"] = ss.str();
		} else {
			cfg_.remove_attribute("hits");
		}
	}
	if (directions != directions_) {
		changed = true;
		ss.str("");
		for (std::set<map_location::DIRECTION>::const_iterator it = directions.begin(); it != directions.end(); ++ it) {
			if (it != directions.begin()) {
				ss << ", ";
			}
			ss << map_location::write_direction(*it);
		}
		if (!ss.str().empty()) {
			cfg_["direction"] = ss.str();
		} else {
			cfg_.remove_attribute("direction");
		}
	}
/*
	if (feature != feature_) {
		changed = true;
		if (feature != HEROS_NO_FEATURE) {
			cfg_["feature"] = feature;
		} else {
			cfg_.remove_attribute("feature");
		}
	}
	if (align != align_) {
		changed = true;
		if (align != ALIGN_NONE) {
			cfg_["align"] = align;
		} else {
			cfg_.remove_attribute("align");
		}
	}
	// simplely, support one only.
	config primary_attack_filter0_;
	if (!primary_attack_filter_.empty()) {
		primary_attack_filter0_ = primary_attack_filter_[0];
	}
	if (primary_attack_filter != primary_attack_filter0_) {
		changed = true;
		cfg_.clear_children("filter_attack");
		if (!primary_attack_filter.empty()) {
			cfg_.add_child("filter_attack", primary_attack_filter);
		}
	}
	if (secondary_weapon_type != secondary_weapon_type_) {
		changed = true;
		strstr.str("");
		for (std::set<std::string>::const_iterator it = secondary_weapon_type.begin(); it != secondary_weapon_type.end(); ++ it) {
			if (it != secondary_weapon_type.begin()) {
				strstr << ", ";
			}
			strstr << *it;
		}
		if (!strstr.str().empty()) {
			cfg_["secondary_weapon_type"] = strstr.str();
		} else {
			cfg_.remove_attribute("secondary_weapon_type");
		}
	}
*/
	return changed;
}

std::string tanim2::particular_frame_tag(const tparticular& l) const
{
	std::string frame_tag = "frame";
	for (std::map<std::string, tparticular>::const_iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
		if (&it->second == &l) {
			frame_tag = it->first;
			break;
		}
	}
	return frame_tag;
}

bool tanim2::has_particular(const std::string& tag) const
{
	if (tag.empty()) {
		return true;
	}
	return sub_anims_.find(tag) != sub_anims_.end();
}

bool tanim2::add_particular()
{
	if (has_particular(default_particular_frame_tag)) {
		return false;
	}
	std::stringstream strstr;
	strstr << default_particular_frame_tag.substr(0, default_particular_frame_tag.size() - 5) << "start_time";
	cfg_[strstr.str()] = 0;

	config add;
	add["duration"] = 1;
	cfg_.add_child(default_particular_frame_tag, add);
	return true;
}

void tanim2::delete_particular(int n)
{
	tparticular* l = &unit_anim_;
	std::string frame_tag_prefix = "";
	if (n > 0) {
		std::map<std::string, tparticular>::iterator it = sub_anims_.begin();
		std::advance(it, n - 1);
		l = &it->second;
		frame_tag_prefix = it->first.substr(0, it->first.size() - 5);
	}

	cfg_.remove_attribute(frame_tag_prefix + "start_time");
	cfg_.remove_attribute(frame_tag_prefix + "x");
	cfg_.remove_attribute(frame_tag_prefix + "y");
	cfg_.remove_attribute(frame_tag_prefix + "offset_x");
	cfg_.remove_attribute(frame_tag_prefix + "offset_y");

	cfg_.clear_children(frame_tag_prefix + "frame");
}

void tanim2::add_frame(tparticular& l, int n, bool front)
{
	config add;
	add["duration"] = 1;

	std::string frame_tag = particular_frame_tag(l);
	cfg_.add_child_at(frame_tag, add, front? n: n + 1);
}

void tanim2::delete_frame(tparticular& l, int n)
{
	std::string frame_tag = particular_frame_tag(l);
	cfg_.remove_child(frame_tag, n);
}

bool tanim2::from_ui_frame_edit(twindow& window, tparticular& l, int n, bool use_image)
{
	config cfg;
	cfg["duration"] = utils::to_int(find_widget<ttext_box>(&window, "duration", false, true)->label());
	
	std::string text;
	if (use_image) {
		text = find_widget<ttext_box>(&window, "image", false, true)->label();
		if (!text.empty()) {
			cfg["image"] = text;
		}
		text = find_widget<ttext_box>(&window, "image_diagonal", false, true)->label();
		if (!text.empty()) {
			cfg["image_diagonal"] = text;
		}
		text = find_widget<ttext_box>(&window, "image_horizontal", false, true)->label();
		if (!text.empty()) {
			cfg["image_horizontal"] = text;
		}
	} else {
		text = find_widget<ttext_box>(&window, "halo", false, true)->label();
		if (!text.empty()) {
			cfg["halo"] = text;
		}
	}
	text = find_widget<ttext_box>(&window, "image_mod", false, true)->label();
	if (!text.empty()) {
		cfg["image_mod"] = text;
	}
	text = find_widget<ttext_box>(&window, "x", false, true)->label();
	if (!text.empty()) {
		cfg["x"] = text;
	}
	text = find_widget<ttext_box>(&window, "y", false, true)->label();
	if (!text.empty()) {
		cfg["y"] = text;
	}
	text = find_widget<ttext_box>(&window, "offset_x", false, true)->label();
	if (!text.empty()) {
		cfg["offset_x"] = text;
	}
	text = find_widget<ttext_box>(&window, "offset_y", false, true)->label();
	if (!text.empty()) {
		cfg["offset_y"] = text;
	}
	if (!find_widget<ttoggle_button>(&window, "auto_hflip", false, true)->get_value()) {
		cfg["auto_hflip"] = "no";
	}
	if (!find_widget<ttoggle_button>(&window, "auto_vflip", false, true)->get_value()) {
		cfg["auto_vflip"] = "no";
	}
	text = find_widget<ttext_box>(&window, "sound", false, true)->label();
	if (!text.empty()) {
		cfg["sound"] = text;
	}
	text = find_widget<ttext_box>(&window, "stext", false, true)->label();
	const std::string stext = text;
	if (!stext.empty()) {
		cfg["stext"] = text;
	}
	int font_size = utils::to_int(find_widget<ttext_box>(&window, "font_size", false, true)->label());
	if (font_size) {
		cfg["font_size"] = font_size;
	}

	if (!stext.empty()) {
		int color = decode_color(find_widget<ttext_box>(&window, "text_color", false, true)->label());
		if (color != frame_parameters::default_text_color) {
			std::stringstream ss;
			ss << ((color & 0xFF000000) >> 24) << "," << ((color & 0x00FF0000) >> 16) << "," << ((color & 0x0000FF00) >> 8) << "," << (color & 0x000000FF);
			cfg["text_color"] = ss.str();
		}
	}

	text = find_widget<ttext_box>(&window, "alpha", false, true)->label();
	if (!text.empty()) {
		cfg["alpha"] = text;
	}

	std::string frame_tag = particular_frame_tag(l);
	config& frame_cfg_ = cfg_.child(frame_tag, n);
	if (frame_cfg_ != cfg) {
		frame_cfg_ = cfg;
		return true;
	}
	return false;
}

std::string tanim2::filter_description() const
{
	std::stringstream strstr;
	if (!hits_.empty()) {
		strstr << "hits[" << cfg_["hits"] << "]";
	}
	if (!directions_.empty()) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "direction[" << cfg_["direction"] << "]";
	}
	if (feature_ != HEROS_NO_FEATURE) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "feature[" << hero::feature_str(feature_) << "]";
	}
	if (align_ != ALIGN_NONE) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "align[" << align_filter_names[align_] << "]";
	}
	if (!primary_attack_filter_.empty()) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		for (std::vector<config>::const_iterator it = primary_attack_filter_.begin(); it != primary_attack_filter_.end(); ++ it) {
			const config& filter = *it;
			strstr << "primary_attack_filter[";
			bool first = true;
			BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
				if (!first) {
					strstr << "  ";
				} else {
					first = false;
				}
				strstr << istrmap.first << "(" << istrmap.second << ")";
			}
			strstr << "]";
		}
	}
	if (!secondary_weapon_type_.empty()) {
		if (!strstr.str().empty()) {
			strstr << "  ";
		}
		strstr << "secondary_seapon_type[" << cfg_["secondary_weapon_type"] << "]";
	}
	return strstr.str();
}

int tanim2::get_end_time() const
{
	int result = unit_anim_.get_end_time();
	std::map<std::string, tparticular>::const_iterator anim_itor =sub_anims_.begin();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		result= std::max<int>(result,anim_itor->second.get_end_time());
	}
	return result;
}

int tanim2::get_begin_time() const
{
	int result = unit_anim_.get_begin_time();
	std::map<std::string, tparticular>::const_iterator anim_itor =sub_anims_.begin();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		result= std::min<int>(result,anim_itor->second.get_begin_time());
	}
	return result;
}

void tanim2::filter_update_to_ui_anim_edit(ttree_node& branch) const
{
	std::stringstream ss;
	ttree_node* htvi;
	bool none_filter = true;

	std::map<std::string, std::string> data;
	ss.str("");
	ss << _("Filter");
	data["label"] = ss.str();
	ttree_node& htvi_filter = branch.insert_node("default", data);
	htvi_filter.set_child_icon("label", "misc/filter.png");
	htvi_filter.set_cookie(PARAM_FILTER);

	if (!hits_.empty()) {
		none_filter = false;

		ss.str("");
		ss << "hits" << ": " << cfg_["hits"];
		data["label"] = ss.str();
		htvi = &htvi_filter.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	if (!directions_.empty()) {
		none_filter = false;

		ss.str("");
		ss << "direction" << ": " << cfg_["direction"];
		data["label"] = ss.str();
		htvi = &htvi_filter.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	if (feature_ != HEROS_NO_FEATURE) {
		none_filter = false;
		
		ss.str("");
		ss << "feature" << ": " << hero::feature_str(feature_);
		data["label"] = ss.str();
		htvi = &htvi_filter.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	if (align_ != ALIGN_NONE) {
		none_filter = false;
		
		ss.str("");
		ss << "align" << ": " << align_filter_names[align_];
		data["label"] = ss.str();
		htvi = &htvi_filter.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	if (!primary_attack_filter_.empty()) {
		none_filter = false;
		for (std::vector<config>::const_iterator it = primary_attack_filter_.begin(); it != primary_attack_filter_.end(); ++ it) {
			const config& filter = *it;
			
			ss.str("");
			ss << "primary_attack_filter" << ": ";
			bool first = true;
			BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
				if (!first) {
					ss << "  ";
				} else {
					first = false;
				}
				ss << istrmap.first << "(" << istrmap.second << ")";
			}
			data["label"] = ss.str();
			htvi = &htvi_filter.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
		}
	}
	if (!secondary_attack_filter_.empty()) {
		none_filter = false;
		for (std::vector<config>::const_iterator it = secondary_attack_filter_.begin(); it != secondary_attack_filter_.end(); ++ it) {
			const config& filter = *it;

			ss.str("");
			ss << "secondary_attack_filter" << ": ";
			bool first = true;
			BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
				if (!first) {
					ss << "  ";
				} else {
					first = false;
				}
				ss << istrmap.first << "(" << istrmap.second << ")";
			}
			data["label"] = ss.str();
			htvi = &htvi_filter.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
		}
	}
	if (none_filter) {
		ss.str("");
		ss << _("None");
		data["label"] = ss.str();
		htvi = &htvi_filter.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	if (!secondary_weapon_type_.empty()) {
		none_filter = false;
		
		ss.str("");
		ss << "secondary_weapon_type" << ": " << cfg_["secondary_weapon_type"];
		data["label"] = ss.str();
		htvi = &htvi_filter.insert_node("default", data);
		htvi->set_child_icon("label", "misc/property.png");
	}
	htvi_filter.unfold();
}
/*
void cb_treeview_update_scroll_anim(HWND htv, HTREEITEM htvi, TVITEMEX& tvi, void* ctx)
{
	tanim2& anim = *reinterpret_cast<tanim2*>(ctx);
	HTREEITEM htvi1;
	TVITEMEX tvi1;
	if (!tvi.cChildren) {
		htvi1 = TreeView_GetParent(htv, htvi);
		TreeView_GetItem1(htv, htvi1, &tvi1, TVIF_PARAM | TVIF_CHILDREN, NULL);
		scroll::first_visible_lparam = tvi1.lParam;
	}
	
	anim.update_to_ui_anim_edit(GetParent(htv));
}
*/

void tanim2::update_to_ui_anim_edit(twindow& window)
{
	ttree* editor = find_widget<ttree>(&window, "editor", false, true);
	editor->clear();
	
	std::map<std::string, std::string> data;
	ttree_node* htvi;

	std::stringstream ss;
	ss.str("");
	ss << id_ << " (" << get_begin_time() << ", " << get_end_time() << ")";
	data["label"] = ss.str();
	ttree_node& htvi_root = editor->insert_node("default", data);
	htvi_root.set_child_icon("label", "misc/animation.png");
	htvi_root.set_cookie(PARAM_ANIM);

	ss.str("");
	ss << _("Area mode") << ": " << (area_mode_? _("Yes"): _("No"));
	data["label"] = ss.str();
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(PARAM_MODE);

	// unit_anim_
	unit_anim_.paramsters_update_to_ui_anim_edit(htvi_root, _("Master particular"), PARAM_PARTICULAR);

	// sub_anim_
	for (std::map<std::string, tparticular>::iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
		const tparticular& l = it->second;
		int index = std::distance(sub_anims_.begin(), it);
		int param = PARAM_PARTICULAR + 1 + index;

		ss.str("");
		ss << _("Second particular") << "#" << index << "(" << it->first << ")";
		l.paramsters_update_to_ui_anim_edit(htvi_root, ss.str(), param);
	}

	// filter
	if (template_) {
		filter_update_to_ui_anim_edit(htvi_root);
	}

	// [frame] section
	int anim_end_time = get_end_time();
	std::map<int, int> analying_indexs, next_frame_start_time;
	analying_indexs.insert(std::make_pair(0, 0));
	if (!unit_anim_.frames_.empty()) {
		next_frame_start_time.insert(std::make_pair(0, unit_anim_.frames_[0].start_time_));
	} else {
		next_frame_start_time.insert(std::make_pair(0, anim_end_time));
	}
	for (std::map<std::string, tparticular>::iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
		const tparticular& l = it->second;
		int index = std::distance(sub_anims_.begin(), it);
		analying_indexs.insert(std::make_pair(1 + index, 0));
		if (!l.frames_.empty()) {
			next_frame_start_time.insert(std::make_pair(1 + index, l.frames_[0].start_time_));
		} else {
			next_frame_start_time.insert(std::make_pair(1 + index, anim_end_time));
		}
	}

	frame_cookie_.clear();

	bool one_particual_unfinised = false;
	int analying_time = get_begin_time();
	do {
		one_particual_unfinised = false;

		ss.str("");
		ss << analying_time << "(ms)";
		data["label"] = ss.str();
		ttree_node& htvi_time = htvi_root.insert_node("default", data);
		htvi_time.set_child_icon("label", "misc/clock.png");
		htvi_time.set_cookie(PARAM_TIME);

		if (analying_indexs[0] < (int)unit_anim_.frames_.size()) {
			const animated<unit_frame>::frame& f = unit_anim_.frames_[analying_indexs[0]];
			if (f.start_time_ == analying_time) {
				frame_cookie_.push_back(std::make_pair(&unit_anim_, analying_indexs[0]));
				unit_anim_.frame_update_to_ui_anim_edit(htvi_time, _("Master particular"), analying_indexs[0], frame_cookie_.size() - 1);

				analying_indexs[0] ++;
				if (analying_indexs[0] < (int)unit_anim_.frames_.size()) {
					next_frame_start_time[0] = f.start_time_ + f.duration_;
				} else {
					next_frame_start_time[0] = anim_end_time;
				}
			}
			if (analying_indexs[0] < (int)unit_anim_.frames_.size()) {
				one_particual_unfinised = true;
			}
		}
		for (std::map<std::string, tparticular>::iterator it = sub_anims_.begin(); it != sub_anims_.end(); ++ it) {
			tparticular& l = it->second;
			int index = std::distance(sub_anims_.begin(), it);
			if (analying_indexs[index + 1] < (int)l.frames_.size()) {
				const animated<unit_frame>::frame& f = l.frames_[analying_indexs[index + 1]];
				if (f.start_time_ == analying_time) {
					ss.str("");
					ss << _("Second particular") << "#" << index << "(" << it->first << ")";
					frame_cookie_.push_back(std::make_pair(&l, analying_indexs[index + 1]));
					l.frame_update_to_ui_anim_edit(htvi_time, ss.str(), analying_indexs[index + 1], frame_cookie_.size() - 1);

					analying_indexs[index + 1] ++;
					if (analying_indexs[index + 1] < (int)l.frames_.size()) {
						next_frame_start_time[index + 1] = f.start_time_ + f.duration_;
					} else {
						next_frame_start_time[index + 1] = anim_end_time;
					}
				}
				if (analying_indexs[index + 1] < (int)l.frames_.size()) {
					one_particual_unfinised = true;
				}
			}
		}

		int min_next = INT_MAX;
		for (std::map<int, int>::iterator it = next_frame_start_time.begin(); it != next_frame_start_time.end(); ++ it) {
			min_next = std::min<int>(min_next, it->second);
		}
		analying_time = min_next;
		htvi_time.unfold();
	} while (one_particual_unfinised);


	htvi_root.unfold();
}

void tanim2::update_to_ui_filter_edit(twindow& window) const
{
	if (std::find(hits_.begin(), hits_.end(), HIT) != hits_.end()) {
		find_widget<ttoggle_button>(&window, "hit", false, true)->set_value(true);
	}
	if (std::find(hits_.begin(), hits_.end(), MISS) != hits_.end()) {
		find_widget<ttoggle_button>(&window, "miss", false, true)->set_value(true);
	}
	if (std::find(hits_.begin(), hits_.end(), KILL) != hits_.end()) {
		find_widget<ttoggle_button>(&window, "kill", false, true)->set_value(true);
	}

	if (std::find(directions_.begin(), directions_.end(), map_location::NORTH) != directions_.end()) {
		find_widget<ttoggle_button>(&window, "n", false, true)->set_value(true);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::NORTH_EAST) != directions_.end()) {
		find_widget<ttoggle_button>(&window, "ne", false, true)->set_value(true);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::NORTH_WEST) != directions_.end()) {
		find_widget<ttoggle_button>(&window, "nw", false, true)->set_value(true);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::SOUTH) != directions_.end()) {
		find_widget<ttoggle_button>(&window, "s", false, true)->set_value(true);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::SOUTH_EAST) != directions_.end()) {
		find_widget<ttoggle_button>(&window, "se", false, true)->set_value(true);
	}
	if (std::find(directions_.begin(), directions_.end(), map_location::SOUTH_WEST) != directions_.end()) {
		find_widget<ttoggle_button>(&window, "sw", false, true)->set_value(true);
	}
/*
#ifndef _ROSE_EDITOR
	const std::vector<std::string>& atype_ids = unit_types.atype_ids();
	for (size_t n = 0; n < atype_ids.size(); n ++) {
		// IDM of weapon type checkbox must align with atype!
		if (std::find(secondary_weapon_type_.begin(), secondary_weapon_type_.end(), atype_ids[n]) != secondary_weapon_type_.end()) {
			Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_ANIMFILTER_BLADE + n), TRUE);
		}
	}
#endif

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_FEATURE);
	for (int n = 0; n < ComboBox_GetCount(hctl); n ++) {
		if (ComboBox_GetItemData(hctl, n) == feature_) {
			ComboBox_SetCurSel(hctl, n);
		}
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ALIGN);
	for (int n = 0; n < ComboBox_GetCount(hctl); n ++) {
		if (ComboBox_GetItemData(hctl, n) == align_) {
			ComboBox_SetCurSel(hctl, n);
		}
	}
	
	if (!primary_attack_filter_.empty()) {
		const config& filter = primary_attack_filter_[0];

		BOOST_FOREACH (const config::attribute &istrmap, filter.attribute_range()) {
			if (istrmap.first == "name") {
				hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_ID);
				ComboBox_SetCurSel(hctl, ComboBox_FindString(hctl, -1, istrmap.second.str().c_str()));
			} else if (istrmap.first == "range") {
				hctl = GetDlgItem(hdlgP, IDC_CMB_ANIMFILTER_RANGE);
				ComboBox_SetCurSel(hctl, ComboBox_FindString(hctl, -1, istrmap.second.str().c_str()));
			}
		}
	}
*/
}

std::string tanim2::generate() const
{
	std::stringstream strstr;

	strstr << "[animation]\n";

	strstr << "\tid=" << id_ << "\n";
	if (!app_.empty()) {
		strstr << "\tapp=" << app_ << "\n";
	}
	if (template_) {
		strstr << "\ttemplate=yes\n";
	}
	strstr << "\t[anim]\n";
	::write(strstr, cfg_, 2);
	strstr << "\t[/anim]\n";

	strstr << "[/animation]";

	strstr << "\n";

	return strstr.str();
}

bool tanim2::dirty() const
{
	if (*this != anim_from_cfg_) {
		return true;
	}
	return false;
}


tanim3::tanim3(const config& app_cfg)
	: app_cfg_(app_cfg)
	, app_anim_start_(nposm)
	, clicked_anim_(nposm)
{
	prepare_anims();
}

void tanim3::prepare_anims()
{
	anim_apps_.clear();
	anims_from_cfg_.clear();
	const config::const_child_itors& utype_anims = app_cfg_.child("units").child_range("animation");
	BOOST_FOREACH (const config &cfg, utype_anims) {
		tanim2 anim;
		anim.from_config(cfg);
		anims_from_cfg_.push_back(anim);
		if (anim_apps_.find(anim.app_) == anim_apps_.end()) {
			anim_apps_.insert(anim.app_);
		}
	}
	anims_updating_ = anims_from_cfg_;

	
	// fill apps that now no animation.
	std::set<std::string> reserved_proj;
	std::set<std::string> apps2 = apps_in_res(); 
	for (std::set<std::string>::const_iterator it = apps2.begin(); it != apps2.end(); ++ it) {
		const std::string& proj = *it;
		if (reserved_proj.find(proj) != reserved_proj.end()) {
			continue;
		}
		if (anim_apps_.find(proj) != anim_apps_.end()) {
			continue;
		}
		anim_apps_.insert(proj);
	}
}

void tanim3::pre_show()
{
	window_->set_margin(8 * twidget::hdpi_scale, 8 * twidget::hdpi_scale, 0, 8 * twidget::hdpi_scale);
	window_->set_label("misc/white_background.png");

	find_widget<tlabel>(window_, "title", false).set_label(_("Animation editor"));

	tlistbox* list = find_widget<tlistbox>(window_, "animations", false, true);
	list->set_did_row_changed(std::bind(&tanim3::did_animation_changed, this, _2));

	treport* report = find_widget<treport>(window_, "apps", false, true);
	for (std::set<std::string>::const_iterator it = anim_apps_.begin(); it != anim_apps_.end(); ++ it) {
		std::string app = *it;
		if (app.empty()) {
			app = "rose";
		}
		report->insert_item(null_str, app);
	}

	report->set_did_item_changed(std::bind(&tanim3::did_app_changed, this, _2));
	if (!anim_apps_.empty()) {
		report->select_item(0);
	}

	ttree* editor = find_widget<ttree>(window_, "editor", false, true);
	editor->set_did_double_click(std::bind(&tanim3::did_double_click_editor, this, _2));
	editor->set_did_right_button_up(std::bind(&tanim3::did_right_button_up_editor, this, _2, _3));

	connect_signal_mouse_left_click(
			  find_widget<tcontrol>(window_, "build", false)
			, std::bind(
				&tanim3::do_build
				, this));

	connect_signal_mouse_left_click(
			  find_widget<tcontrol>(window_, "insert", false)
			, std::bind(
				&tanim3::insert_animation
				, this
				, std::ref(*window_)));

	connect_signal_mouse_left_click(
			  find_widget<tcontrol>(window_, "erase", false)
			, std::bind(
				&tanim3::erase_animation
				, this
				, std::ref(*window_)));

	tbuild::pre_show(find_widget<ttrack>(window_, "task_status", false));
}

std::string tanim3::anims_cfg(const std::string& app) const
{
	std::stringstream ss;
	if (app.empty()) {
		ss << game_config::path << "/data/core/units-internal/animation.cfg";
	} else {
		ss << game_config::path << "/app-" << app << "/units-internal/animation.cfg";
	}

	return ss.str();
}

void tanim3::generate_anims_cfg() const
{
	std::stringstream fp_ss;

	std::map<std::string, std::vector<const tanim2*> > apps;
	for (std::vector<tanim2>::const_iterator it = anims_updating_.begin(); it != anims_updating_.end(); ++ it) {
		const tanim2& anim = *it;
		std::map<std::string, std::vector<const tanim2*> >::iterator it2 = apps.find(anim.app_);
		if (it2 == apps.end()) {
			it2 = apps.insert(std::make_pair(anim.app_, std::vector<const tanim2*>(1, &anim))).first;
		} else {
			it2->second.push_back(&anim);
		}
	}
	for (std::set<std::string>::const_iterator it = anim_apps_.begin(); it != anim_apps_.end(); ++ it) {
		const std::string& app = *it;
		if (apps.find(app) != apps.end()) {
			continue;
		}
		apps.insert(std::make_pair(app, std::vector<const tanim2*>()));
	}

	for (std::map<std::string, std::vector<const tanim2*> >::const_iterator it = apps.begin(); it != apps.end(); ++ it) {
		const std::string& app = it->first;
		const std::vector<const tanim2*>& v = it->second;

		fp_ss.str("");
		fp_ss << "#\n";
		fp_ss << "# NOTE: it is generated by rose studio, don't edit yourself.\n";
		fp_ss << "#\n";
		fp_ss << "\n";
		for (std::vector<const tanim2*>::const_iterator it2 = v.begin(); it2 != v.end(); ++ it2) {
			const tanim2& anim = **it2;
			
			if (!fp_ss.str().empty()) {
				fp_ss << "\n";
			}
			fp_ss << anim.generate();
		}

		const std::string file_name = anims_cfg(app);
		if (file_exists(file_name)) {
			tfile file2(file_name, GENERIC_READ, OPEN_EXISTING);
			int fsize = file2.read_2_data();
			if (fsize == fp_ss.str().size() && !memcmp(fp_ss.str().c_str(), file2.data, fsize)) {
				continue;
			}
		}

		tfile file(file_name, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, fp_ss.str().c_str(), fp_ss.str().length());
	}
}

void tanim3::update_build_button()
{
	tcontrol* build = find_widget<tcontrol>(window_, "build", false, true);
	build->set_active(anims_dirty());
}

bool tanim3::anims_dirty() const
{
	return anims_updating_ != anims_from_cfg_;
}

void tanim3::did_app_changed(ttoggle_button& widget)
{
	std::set<std::string>::const_iterator it = anim_apps_.begin();
	std::advance(it, widget.at());
	const std::string& curr_app = *it;

	tlistbox* list = find_widget<tlistbox>(window_, "animations", false, true);
	list->clear();

	std::stringstream ss;

	std::map<std::string, std::string> data;
	app_anim_start_ = nposm;
	int anim_at = 0;
	std::map<std::string, int> app_anims;
	for (std::set<std::string>::const_iterator it = anim_apps_.begin(); it != anim_apps_.end(); ++ it) {
		app_anims.insert(std::make_pair(*it, 0));
	}
	for (std::vector<tanim2>::const_iterator it = anims_updating_.begin(); it != anims_updating_.end(); ++ it, anim_at ++) {
		const tanim2& anim = *it;

		std::map<std::string, int>::iterator it2 = app_anims.find(anim.app_);
		it2->second = it2->second + 1;

		if (anim.app_ != curr_app) {
			continue;
		}
		if (app_anim_start_ == nposm) {
			app_anim_start_ = anim_at;
		} else {
			VALIDATE(anim_at == app_anim_start_ + list->rows(), null_str);
		}
	
		// number
		data["number"] = str_cast(anim_at);

		// id
		ss.str("");
		ss << anim.id_;
		if (anim.template_) {
			ss << "(" << _("Template") << ")";
		}
		data["id"] = ss.str();

		// particular
		ss.str("");
		ss << (1 + anim.sub_anims_.size());
		ss << ", ";

		// screen mode
		ss << (anim.area_mode_? _("Yes"): _("No"));
		data["area"] = ss.str();

		// animation time
		ss.str("");
		ss << "(" << anim.get_begin_time() << ", " << anim.get_end_time() << ")";
		data["duration"] = ss.str();

		list->insert_row(data);
	}
	if (app_anim_start_ == nposm) {
		// insert_animation require app_anim_start_ valid.
		app_anim_start_ = 0;
		for (std::map<std::string, int>::const_iterator it = app_anims.begin(); it != app_anims.end(); ++ it) {
			if (it->first == curr_app) {
				break;
			}
			app_anim_start_ += it->second;
		}
	}

	if (list->rows()) {
		list->select_row(0);
	} else {
		ttree* editor = find_widget<ttree>(window_, "editor", false, true);
		editor->clear();
	}
}

void tanim3::did_animation_changed(ttoggle_panel& widget)
{
	clicked_anim_ = app_anim_start_ + widget.at();
	tanim2& anim = anims_updating_[clicked_anim_];
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();
}

static bool tvi_is_particular(int param)
{
	return param >= tanim2::PARAM_PARTICULAR && param < tanim2::PARAM_FRAME;
}

static bool tvi_is_frame(int param)
{
	return param >= tanim2::PARAM_FRAME;
}

bool tanim3::did_anim_id_changed(const std::string& label)
{
	const int min_id_len = 1;
	const int max_id_len = 32;

	const std::string id = utils::lowercase(label);
	if (!utils::isvalid_id(id, false, min_id_len, max_id_len)) {
		return false;
	}
	int anim_at = 0;
	for (std::vector<tanim2>::const_iterator it = anims_updating_.begin(); it != anims_updating_.end(); ++ it, anim_at ++) {
		const tanim2& anim = *it;
		if (clicked_anim_ != anim_at && anim.id_ == id) {
			return false;
		}
		if (label == default_new_anim_id) {
			return false;
		}
	}
	return true;
}

void tanim3::did_double_click_editor(ttree_node& widget)
{
	tanim2& anim = anims_updating_[clicked_anim_];
	const int cookie = widget.cookie();
	const int parent_cookie = widget.parent_node().cookie();
	if (cookie == tanim2::PARAM_MODE) {
		if (anim.area_mode_) {
			anim.cfg_.remove_attribute("area_mode");
		} else {
			anim.cfg_["area_mode"] = true;
		}

	} else if (tvi_is_particular(parent_cookie)) {
		gui2::tanim_particular dlg(anim, parent_cookie - tanim2::PARAM_PARTICULAR);
		dlg.show();

	} else if (parent_cookie == tanim2::PARAM_FILTER) {
		gui2::tanim_filter dlg(anim);
		dlg.show();

	} else if (tvi_is_frame(parent_cookie)) {
		gui2::tanim_frame dlg(anim, parent_cookie - tanim2::PARAM_FRAME);
		dlg.show();

	} else {
		VALIDATE(false, null_str);
	}

	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();
}

void tanim3::did_right_button_up_editor(ttree_node& widget, const tpoint& coordinate)
{
	tanim2& anim = anims_updating_[clicked_anim_];
	const int cookie = widget.cookie();

	if (cookie == tanim2::PARAM_ANIM) {
		std::vector<gui2::tmenu::titem> items;
	
		enum {edit, insert_particular};
		items.push_back(gui2::tmenu::titem(std::string(_("Edit")) + "...", edit));
		items.push_back(gui2::tmenu::titem(std::string(_("Add particular")), insert_particular));

		int selected;
		{
			gui2::tmenu dlg(items, nposm);
			dlg.show(coordinate.x, coordinate.y);
			int retval = dlg.get_retval();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			selected = dlg.selected_val();
		}
		if (selected == edit) {
			do_edit_animation();

		} else if (selected == insert_particular) {
			do_insert_particular();

		}

	} else if (tvi_is_particular(cookie)) {
		std::vector<gui2::tmenu::titem> items;
	
		enum {edit_particular, delete_particular};
		items.push_back(gui2::tmenu::titem(std::string(_("Edit")) + "...", edit_particular));
		if (cookie > tanim2::PARAM_PARTICULAR) {
			items.push_back(gui2::tmenu::titem(std::string(_("Delete")) + "...", delete_particular));
		}

		int selected;
		{
			gui2::tmenu dlg(items, nposm);
			dlg.show(coordinate.x, coordinate.y);
			int retval = dlg.get_retval();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			selected = dlg.selected_val();
		}
		if (selected == edit_particular) {
			gui2::tanim_particular dlg(anim, cookie - tanim2::PARAM_PARTICULAR);
			dlg.show();

		} else if (selected == delete_particular) {
			do_erase_particular(cookie - tanim2::PARAM_PARTICULAR);
		}

	} else if (tvi_is_frame(cookie)) {
		std::vector<gui2::tmenu::titem> items, add_submenu;
	
		enum {add_frame_front, add_frame_behind, edit_frame, delete_frame};
		add_submenu.push_back(gui2::tmenu::titem(_("In front of"), add_frame_front));
		add_submenu.push_back(gui2::tmenu::titem(_("Behind"), add_frame_behind));
		items.push_back(gui2::tmenu::titem(_("Add frame"), add_submenu));
		items.push_back(gui2::tmenu::titem(std::string(_("Edit")) + "...", edit_frame));
		items.push_back(gui2::tmenu::titem(std::string(_("Delete")) + "...", delete_frame));

		int selected;
		{
			gui2::tmenu dlg(items, nposm);
			dlg.show(coordinate.x, coordinate.y);
			int retval = dlg.get_retval();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			selected = dlg.selected_val();
		}

		if (selected == add_frame_front) {
			do_insert_frame(cookie - tanim2::PARAM_FRAME, true);

		} else if (selected == add_frame_behind) {
			do_insert_frame(cookie - tanim2::PARAM_FRAME, false);

		} else if (selected == edit_frame) {
			gui2::tanim_frame dlg(anim, cookie - tanim2::PARAM_FRAME);
			dlg.show();

		} else if (selected == delete_frame) {
			do_erase_frame(cookie - tanim2::PARAM_FRAME);
		}
	}
}

void tanim3::insert_animation(twindow& window)
{
	for (std::vector<tanim2>::const_iterator it = anims_updating_.begin(); it != anims_updating_.end(); ++ it) {
		const tanim2& anim = *it;
		if (anim.id_ == default_new_anim_id) {
			return;
		}
	}

	treport* report = find_widget<treport>(&window, "apps", false, true);
	int apps_at = report->cursel()->at();
	std::set<std::string>::const_iterator apps_it = anim_apps_.begin();
	std::advance(apps_it, apps_at);
	const std::string app = *apps_it;

	// id="__new_anim"
	// app=studio
	// [anim]
	//		area_mode=yes
	//		[frame]
	//			duration=1000
	//			image="misc/arrow_right.png"
	//		[/frame]
	// [/anim]
	config animation_cfg;
	animation_cfg["id"] = default_new_anim_id;
	if (!app.empty()) {
		animation_cfg["app"] = app;
	}
	config& anim_cfg = animation_cfg.add_child("anim");
	anim_cfg["area_mode"] = true;
	config& frame_cfg = anim_cfg.add_child("frame");
	frame_cfg["duration"] = 1000;
	frame_cfg["image"] = "misc/arrow_right.png";

	tlistbox* list = find_widget<tlistbox>(&window, "animations", false, true);
	tanim2 anim;
	anim.from_config(animation_cfg);
	if (app_anim_start_ + list->rows() < (int)anims_updating_.size()) {
		std::vector<tanim2>::iterator insert_it = anims_updating_.begin();
		std::advance(insert_it, app_anim_start_ + list->rows());
		anims_updating_.insert(insert_it, anim);
	} else {
		anims_updating_.push_back(anim);
	}
	did_app_changed(*report->cursel());

	update_build_button();
}

void tanim3::erase_animation(twindow& window)
{
	if (clicked_anim_ == nposm) {
		return;
	}
	tanim2& anim = anims_updating_[clicked_anim_];
	int res = gui2::show_message2("", _("Do you want to erase this animation?"), gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	const int origin_clicked_anim = clicked_anim_;
	std::vector<tanim2>::iterator it = anims_updating_.begin();
	std::advance(it, clicked_anim_);

	clicked_anim_ = nposm;
	tlistbox* list = find_widget<tlistbox>(window_, "animations", false, true);
	ttoggle_panel* cursel = list->cursel();
	list->erase_row(cursel->at());
	if (!list->rows()) {
		ttree* editor = find_widget<ttree>(&window, "editor", false, true);
		editor->clear();
	}

	anims_updating_.erase(it);
/*
	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
*/
	update_build_button();
}

void tanim3::do_edit_animation()
{
	tanim2& anim = anims_updating_[clicked_anim_];
	gui2::tedit_box_param param(_("Rename id"), null_str, _("Input new id"), anim.id_, null_str, null_str, _("OK"), nposm);
	{
		param.did_text_changed = std::bind(&tanim3::did_anim_id_changed, this, _1);
		gui2::tedit_box dlg(param);
		try {
			dlg.show();
		} catch(twml_exception& e) {
			e.show();
		}
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		if (anim.id_ == param.result) {
			return;
		}
	}

	anim.id_ = param.result;
	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();

	// update id in listbox
	tlistbox* list = find_widget<tlistbox>(window_, "animations", false, true);
	ttoggle_panel* cursel = list->cursel();
	dynamic_cast<tcontrol*>(cursel->find("id", false))->set_label(anim.id_);
}

void tanim3::do_insert_particular()
{
	tanim2& anim = anims_updating_[clicked_anim_];
	if (!anim.add_particular()) {
		return;
	}
	
	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();
}

void tanim3::do_erase_particular(const int at)
{
	VALIDATE(at > 0, null_str);
	tanim2& anim = anims_updating_[clicked_anim_];

	std::stringstream ss;
	utils::string_map symbols;
	ss << vgettext2("Do you want to delete this particular?", symbols);

	const int res = gui2::show_message2(_("Confirm delete"), ss.str(), gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	anim.delete_particular(at);
	
	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();
}

void tanim3::do_insert_frame(const int at, bool front)
{
	tanim2& anim = anims_updating_[clicked_anim_];
	std::pair<tparticular*, int>& cookie = anim.frame_cookie_[at];

	anim.add_frame(*cookie.first, cookie.second, front);
	
	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();
	return;
}

void tanim3::do_erase_frame(const int at)
{
	tanim2& anim = anims_updating_[clicked_anim_];
	std::pair<tparticular*, int>& cookie = anim.frame_cookie_[at];

	std::stringstream ss;
	utils::string_map symbols;
	symbols["particular"] = anim.particular_frame_tag(*cookie.first);
	ss << vgettext2("Do you want to delete frame on $particular?", symbols);

	const int res = gui2::show_message2(_("Confirm delete"), ss.str(), gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	anim.delete_frame(*cookie.first, cookie.second);
	anim.parse(anim.cfg_);
	anim.update_to_ui_anim_edit(*window_);
	update_build_button();
}

bool tanim3::require_build()
{
	std::vector<teditor_::BIN_TYPE> system_bins;
	system_bins.push_back(teditor_::MAIN_DATA);
	editor_.get_wml2bin_desc_from_wml(system_bins);

	std::vector<std::pair<teditor_::BIN_TYPE, teditor_::wml2bin_desc> >& descs = editor_.wml2bin_descs();
	VALIDATE(descs.size() == 1 && descs[0].first == teditor_::MAIN_DATA, null_str);
	teditor_::wml2bin_desc& desc = descs[0].second;

	return desc.wml_nfiles != desc.bin_nfiles || desc.wml_sum_size != desc.bin_sum_size || desc.wml_modified != desc.bin_modified;
}

void tanim3::do_build()
{
	generate_anims_cfg();
	bool require = require_build();
	VALIDATE(require, null_str);
	VALIDATE(editor_.wml2bin_descs().size() == 1, null_str);

	tbuild::do_build2();
}

void tanim3::app_work_start()
{
	std::vector<std::pair<teditor_::BIN_TYPE, teditor_::wml2bin_desc> >& descs = editor_.wml2bin_descs();
	VALIDATE(descs.size() == 1 && descs[0].first == teditor_::MAIN_DATA, null_str);
	teditor_::wml2bin_desc& desc = descs[0].second;
	desc.require_build = true;

	// context_menu_panel_->set_radio_layer(LAYER_TASK_STATUS);
}

void tanim3::app_work_done()
{
	anims_from_cfg_.clear();
	const config::const_child_itors& utype_anims = app_cfg_.child("units").child_range("animation");
	BOOST_FOREACH (const config &cfg, utype_anims) {
		tanim2 anim;
		anim.from_config(cfg);
		anims_from_cfg_.push_back(anim);
/*
		if (anim_apps_.find(anim.app_) == anim_apps_.end()) {
			anim_apps_.insert(anim.app_);
		}
*/
	}

	update_build_button();
}

} // namespace gui2

