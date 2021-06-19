-- reserved table: files, preferences_def
aplt_leagor_studio = {
	files = {
		"home.lua",
	};

	preferences_def = {
	};

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
};

function aplt_leagor_studio.startup()
	local this = aplt_leagor_studio;

	local windows = {
		home = this.DLG_HOME,
	};

	local launcher = this.DLG_HOME;
	return windows, launcher;
end

function aplt_leagor_studio._(msgid)
	return rose.gettext(aplt_leagor_studio.GETTEXT_DOMAIN, msgid) or "";
end

function aplt_leagor_studio.vgettext2(msgid, symbols)
	return rose.vgettext2(aplt_leagor_studio.GETTEXT_DOMAIN, msgid, symbols) or "";
end