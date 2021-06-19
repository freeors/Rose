#ifndef GUI_DIALOGS_SIMPLE_THEME_HPP_INCLUDED
#define GUI_DIALOGS_SIMPLE_THEME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

class simple_controller;

namespace gui2 {

class tsimple_scene: public tdialog
{
public:
	enum { ZOOM, POSITION, NUM_REPORTS};

	enum {
		HOTKEY_RETURN = HOTKEY_MIN,
		HOTKEY_SHARE,
	};

	tsimple_scene(simple_controller& controller);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	void pre_show() override;
};

} //end namespace gui2

#endif
