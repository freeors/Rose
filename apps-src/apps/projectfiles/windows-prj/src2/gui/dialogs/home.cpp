#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/home.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "font.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, home)

thome::thome()
	: current_layer_(nposm)
{
}

void thome::pre_show()
{
	window_->set_label("misc/white_background.png");

	tpanel* aplt = find_widget<tpanel>(window_, "aplt", false, true);
	aplt->set_border("blue_ellipse");

	find_widget<tlabel>(window_, "title", false).set_label(_("Title"));

	tstack* stack = find_widget<tstack>(window_, "body", false, true);
	body_stack_ = stack;

	pre_msg0(*stack->layer(MSG0_LAYER));
	pre_msg1(*stack->layer(MSG1_LAYER));

	treport* report = find_widget<treport>(window_, "navigation", false, true);
	std::vector<std::pair<std::string, std::string> > items;
	items.push_back(std::make_pair(_("First"), "misc/multiselect.png"));
	items.push_back(std::make_pair(_("Second"), "misc/contacts.png"));
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = items.begin(); it != items.end(); ++ it) {
		tcontrol& item = report->insert_item(null_str, it->first);
		item.set_icon(it->second);
	}

	report->set_did_item_changed(std::bind(&thome::did_navigation_changed, this, _1, _2));
	report->select_item(MSG0_LAYER);
}

void thome::post_show()
{
}

void thome::pre_msg0(tgrid& grid)
{
	find_widget<tlabel>(&grid, "msg", false, true)->set_label(_("Hello world"));
}

void thome::pre_msg1(tgrid& grid)
{
	find_widget<tlabel>(&grid, "msg", false, true)->set_label(_("Welcome to Rose"));
}

void thome::did_navigation_changed(treport& report, ttoggle_button& widget)
{
	tgrid& grid = *body_stack_->layer(widget.at());
	body_stack_->set_radio_layer(widget.at());
	current_layer_ = widget.at();
}

} // namespace gui2

