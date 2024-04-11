// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <weston-pro.h>

#include "weston-desktop-shell-protocol.h"

struct desktop_shell {
        struct server *server;
	struct wlr_xdg_shell *xdg_shell;

	struct wl_listener idle_listener;
	struct wl_listener wake_listener;
	struct wl_listener destroy_listener;

	struct wlr_scene_tree *fullscreen_tree;
	struct wlr_scene_tree *panel_tree;
	struct wlr_scene_tree *background_tree;
	struct wlr_scene_tree *lock_tree;
	struct wlr_scene_tree *view_tree;
        struct wlr_scene_tree *minimized_tree;

	struct {
		struct wl_client *client;
		struct wl_resource *desktop_shell;
		struct wl_listener client_destroy_listener;

		unsigned deathcount;
		struct timespec deathstamp;
	} child;

	bool locked;
	bool showing_input_panels;
	bool prepare_event_sent;
};