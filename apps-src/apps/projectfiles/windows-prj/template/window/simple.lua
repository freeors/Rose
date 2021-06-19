-- var.
aplt_leagor_studio__simple = {
	var = {},
};

function aplt_leagor_studio__simple.construct()
	local var = aplt_leagor_studio__simple.var;
	var.startup_ticks_ = rose.get_ticks();
	return 0;
end

function aplt_leagor_studio__simple.pre_show(dlg, window)
	local this = aplt_leagor_studio__simple;
	local var = this.var;
	local aplt = aplt_leagor_studio;
	local _ = aplt_leagor_studio._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/white_background.png");

	gui2.find_widget(window, "ok", false, true):set_visible(gui2.tvisible_INVISIBLE);

end

function aplt_leagor_studio__simple.post_show()
	aplt_leagor_studio__simple.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_studio__simple.timer_handler(now)
	local this = aplt_leagor_studio__simple;
	local var = this.var;
	local _ = aplt_leagor_studio._
end