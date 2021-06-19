-- var.
aplt_leagor_studio__home = {
	MSG0_LAYER = 0,
	MSG1_LAYER = 1,

	var = {},
};

function aplt_leagor_studio__home.construct()
	local var = aplt_leagor_studio__home.var;
	var.startup_ticks_ = rose.get_ticks();
	return 0;
end

function aplt_leagor_studio__home.pre_show(dlg, window)
	local this = aplt_leagor_studio__home;
	local var = this.var;
	local aplt = aplt_leagor_studio;
	local _ = aplt_leagor_studio._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/white_background.png");

	local widget = gui2.find_widget(window, "title", false, true):set_label(_("Title"));

	widget = gui2.find_widget(window, "body", false, true);
	var.body_stack_ = widget;

	this.pre_msg0(widget:layer(this.MSG0_LAYER));
	this.pre_msg1(widget:layer(this.MSG1_LAYER));

	widget = gui2.find_widget(window, "navigation", false, true);
	local items = {
		{label = _("First"), icon = "misc/multiselect.png"},
		{label = _("Second"), icon = "misc/contacts.png"},
	};
	for k, v in ipairs(items) do
		local item = widget:insert_item("", v.label);
		item:set_icon(v.icon);
	end

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.MSG0_LAYER);

end

function aplt_leagor_studio__home.post_show()
	aplt_leagor_studio__home.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_studio__home.pre_msg0(grid)
	local _ = aplt_leagor_studio._
	gui2.find_widget(grid, "msg", false, true):set_label(_("Hello world"));
end

function aplt_leagor_studio__home.pre_msg1(grid)
	local _ = aplt_leagor_studio._
	gui2.find_widget(grid, "msg", false, true):set_label(_("Welcome to Rose"));
end

function aplt_leagor_studio__home.did_navigation_changed(report, widget)
	local this = aplt_leagor_studio__home;
	local var = this.var;
	
	local current_layer = widget:at();
	var.body_stack_:set_radio_layer(current_layer);
end

function aplt_leagor_studio__home.timer_handler(now)
	local this = aplt_leagor_studio__home;
	local var = this.var;
	local _ = aplt_leagor_studio._
end