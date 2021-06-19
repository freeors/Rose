#ifndef GUI_DIALOGS_ANIM_FRAME_HPP_INCLUDED
#define GUI_DIALOGS_ANIM_FRAME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tanim2;
class ttoggle_button;

class tanim_frame: public tdialog
{
public:
	explicit tanim_frame(tanim2& anim, const int at);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void variables_to_lv(twindow& window, const std::string& id);
	void save(twindow& window);
	void did_use_image_changed(ttoggle_button& widget);

	enum {image_layer, halo_layer};

private:
	tanim2& anim_;
	const int at_;
};

} // namespace gui2

#endif

