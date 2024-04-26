// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WESTON_SERVER_H
#define WESTON_SERVER_H

#include "config.h"
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

/* For brevity's sake, struct members are annotated where they are used. */
enum wet_cursor_mode {
	CURSOR_PASSTHROUGH,
	CURSOR_MOVE,
	CURSOR_RESIZE,
};

struct input {
	struct wlr_input_device *wlr_input_device;
	struct seat *seat;
	struct wl_listener destroy;
	struct wl_list link; /* seat::inputs */
};

struct wet_keyboard {
	struct input base;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers;
	struct wl_listener key;
};

struct seat {
	struct server *server;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_keyboard_group *keyboard_group;

	struct wlr_xcursor_manager *cursor_mgr;

	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;

	struct wl_list input_list;

};

struct server {
	struct wl_signal destroy_signal;
	struct wl_display *wl_display;

	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_list view_list;

	struct wl_list keyboards;
	enum wet_cursor_mode cursor_mode;
	struct wet_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list output_list;
	struct wl_listener new_output;

	struct log_context *log_ctx;
	struct wl_signal idle_signal;
	struct wl_signal wake_signal;

	struct seat seat;

};

struct wet_output {
	struct wl_list link;
	struct server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener destroy;
};

struct wet_view {
	struct wl_list link;
	struct server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	int x, y;
};



bool server_init(struct server *server);

bool server_start(struct server *server);

bool output_init(struct server *server);

void cursor_init(struct seat *seat);

void keyboard_init(struct seat *seat);

void focus_view(struct wet_view *view, struct wlr_surface *surface);

void server_new_xdg_surface(struct wl_listener *listener, void *data);

typedef int (*log_func_t)(const char *fmt, va_list ap);

void log_set_handler(log_func_t log, log_func_t cont);

int weston_log(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

int weston_log_continue(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

struct server *server_create(struct wl_display *display, struct log_context *log_ctx);

bool
server_add_destroy_listener_once(struct server *server,
				 struct wl_listener *listener,
				 wl_notify_func_t destroy_handler);

void server_destory(struct server *server);

void new_output_notify(struct wl_listener *listener, void *data);

int weston_pro_shell_init(struct server *server, int *argc, char *argv[]);

void
seat_init(struct server *server);

void
seat_finish(struct server *server);

void keyboard_key_notify(struct wl_listener *listener, void *data);

void
keyboard_modifiers_notify(struct wl_listener *listener, void *data);

#endif
