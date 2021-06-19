#ifndef GUI_DIALOGS_ANIM_PARTICULAR_HPP_INCLUDED
#define GUI_DIALOGS_ANIM_PARTICULAR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tanim2;
class ttext_box;

class tanim_particular: public tdialog
{
public:
	explicit tanim_particular(tanim2& anim, const int at);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_prefix_changed(ttext_box& widget);
	void save(twindow& window);

private:
	tanim2& anim_;
	const int at_;
};

} // namespace gui2

#endif

