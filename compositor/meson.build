srcs_weston_pro = [
	'main.c',
	'server.c',
	'output.c',
	'seat.c',
	'keyboard.c',
	'cursor.c',
	'view.c',
	'log.c',
	'log-file.c',
	'log-wayland.c',
	'option-parser.c',
	'config-parser.c',
	xdg_shell_protocol_h,
	xdg_shell_protocol_c,
	weston_debug_protocol_h,
	weston_debug_protocol_c,

]

deps_weston_pro = [
	dep_wayland_server,
	dep_wlroots,
	dep_xkbcommon,
	dep_pixman,
	dep_libshared,
]

executable(
	'wlrston',
	sources: srcs_weston_pro,
	include_directories: inc_weston_pro,
	dependencies: deps_weston_pro,
)
