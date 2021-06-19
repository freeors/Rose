#ifndef GUI_DIALOGS_SIMPLE_HPP_INCLUDED
#define GUI_DIALOGS_SIMPLE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tsimple: public tdialog
{
public:
	explicit tsimple();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;
};

} // namespace gui2

#endif

