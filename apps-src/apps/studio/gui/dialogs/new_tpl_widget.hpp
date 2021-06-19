#ifndef GUI_DIALOGS_NEW_TPL_WIDGET_HPP_INCLUDED
#define GUI_DIALOGS_NEW_TPL_WIDGET_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "editor.hpp"
#include "unit.hpp"

namespace gui2 {

class ttext_box;
class ttoggle_button;

class tnew_tpl_widget: public tdialog
{
public:
	static std::string generate_tpl_widget_cfg(const std::string& app, const std::string& id, bool nested);

	enum {option_landscape, option_portrait};
	struct tresult {
		int option;
		std::string app;
		std::string id;
		std::string file;
	};
	explicit tnew_tpl_widget(const std::string& app, const unit& u, const std::set<const gui2::ttpl_widget*>& used_tpl_widget, const std::string& default_id);

	const tresult& result() const { return result_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_option_changed(ttoggle_button& row);
	void did_app_changed(ttoggle_button& row);
	void refresh_file_tip(twindow& window, const std::string& id) const;
	void did_id_changed(twindow& window, ttext_box& widget);

private:
	const unit& u_;
	const std::set<const gui2::ttpl_widget*>& used_tpl_widget_;
	const bool nested_;
	const std::string default_id_;
	std::set<std::string> apps_;
	tresult result_;
	int current_option_;
};

} // namespace gui2

#endif

