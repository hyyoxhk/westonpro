if get_option('shell-desktop')
	config_h.set_quoted('WESTON_SHELL_CLIENT', get_option('desktop-shell-client-default'))

	srcs_shell_desktop = [
		'desktop-shell.c',
		'xdg.c',
		weston_desktop_shell_protocol_h,
		weston_desktop_shell_protocol_c,
	]
	deps_shell_desktop = [
		dep_wayland_server,
		dep_wlroots,
		dep_libshared,
	]
	plugin_shell_desktop = shared_library(
		'desktop-shell',
		srcs_shell_desktop,
		include_directories: inc_weston_pro,
		dependencies: deps_shell_desktop,
		name_prefix: '',
	)
endif
