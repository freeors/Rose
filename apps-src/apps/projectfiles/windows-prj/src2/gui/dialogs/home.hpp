#ifndef GUI_DIALOGS_HOME_HPP_INCLUDED
#define GUI_DIALOGS_HOME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tgrid;
class ttoggle_button;
class tstack;

class thome: public tdialog
{
public:
	enum {MSG0_LAYER, MSG1_LAYER};

	explicit thome();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void pre_msg0(tgrid& grid);
	void pre_msg1(tgrid& grid);
	void did_navigation_changed(treport& report, ttoggle_button& widget);

private:
	tstack* body_stack_;
	int current_layer_;
};

} // namespace gui2

#endif

