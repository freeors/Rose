#ifndef GUI_DIALOGS_NEW_THEME_HPP_INCLUDED
#define GUI_DIALOGS_NEW_THEME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "editor.hpp"
#include "theme.hpp"

namespace gui2 {

class ttext_box;
class treport;

class tnew_theme: public tdialog
{
public:
	explicit tnew_theme(const tapp_copier& app, const config& core_cfg, tsave_theme& save_theme, tremove_theme& remove_theme);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	void did_id_changed(twindow& window, ttext_box& widget);
	void did_color_changed(twindow& window, ttext_box& widget, const int tpl, const int at);

	bool did_item_pre_change_report(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_item_changed_report(treport& report, ttoggle_button& widget);
	void new_theme(twindow& window);
	void erase_theme(twindow& window);
	void save(twindow& window);
	void close(twindow& window, bool& handled, bool& halt);

	void reload_theme(twindow& window, const int desire_at, const bool initial);
	std::string form_tab_label(const int at) const;
	bool check_id(const std::string& id, const int current_at) const;
	uint32_t check_color(const std::string& color_str) const;
	bool check_total(const theme::tfields& fields, const int current_at) const;

private:
	const tapp_copier& app_;
	const config& core_cfg_;
	tsave_theme& save_theme_;
	tremove_theme& remove_theme_;

	std::vector<theme::tfields> themes_;
	struct tfields2 
	{
		tfields2(const theme::tfields& fields, bool exist_cfg)
			: fields(fields)
			, exist_cfg(exist_cfg)
		{}

		theme::tfields fields;
		bool exist_cfg;
	};
	std::vector<tfields2> cfg_themes_;

	treport* navigate_;
	int current_at_;

	class tswitch_lock 
	{
	public:
		tswitch_lock(tnew_theme& new_theme)
			: new_theme_(new_theme)
		{
			new_theme_.switching_ = true;
		}
		~tswitch_lock()
		{
			new_theme_.switching_ = false;
		}

	private:
		tnew_theme& new_theme_;
	};
	bool switching_;
};

} // namespace gui2

#endif

