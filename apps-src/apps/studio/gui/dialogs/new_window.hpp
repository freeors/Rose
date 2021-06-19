#ifndef GUI_DIALOGS_NEW_WINDOW_HPP_INCLUDED
#define GUI_DIALOGS_NEW_WINDOW_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "editor.hpp"

namespace gui2 {

class ttext_box;

class tnew_window: public tdialog
{
public:
	explicit tnew_window(bool scene, const std::string& app);

	std::string get_id() const { return ret_id_; }
	bool get_unit_files() const { return unit_files_; }
protected:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	void did_id_changed(twindow& window, ttext_box& widget);
	void refresh_file_tip(twindow& window, const std::string& id) const;
	std::string calculate_res_base_path() const;
	std::string calculate_src_base_path() const;
	std::string calculate_src_gui_base_path() const;

	std::vector<std::string> calculate_res_files(const std::string& id) const;

	std::vector<std::string> calculate_src_files(const std::string& id) const;
	std::vector<std::string> calculate_src_unit_files() const;
	bool is_generate_unit_files() const;

private:
	const bool is_aplt_;
	const bool scene_;
	ttext_box* id_;
	std::string ret_id_;

	bool unit_files_;
	const std::string& app_;
};

} // namespace gui2

#endif

