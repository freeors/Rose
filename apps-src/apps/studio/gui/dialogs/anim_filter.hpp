#ifndef GUI_DIALOGS_ANIM_FILTER_HPP_INCLUDED
#define GUI_DIALOGS_ANIM_FILTER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tanim2;

class tanim_filter: public tdialog
{
public:
	explicit tanim_filter(tanim2& anim);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void do_id(twindow& window);
	void do_range(twindow& window);
	void do_align(twindow& window);
	void do_feature(twindow& window);
	void save(twindow& window);

private:
	tanim2& anim_;
};

} // namespace gui2

#endif

