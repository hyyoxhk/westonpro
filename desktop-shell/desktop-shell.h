// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <wlrston.h>

#include "weston-desktop-shell-protocol.h"

enum fade_type {
	FADE_IN,
	FADE_OUT
};

struct desktop_shell {
        struct server *server;
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;

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

	struct wl_list output_list;

	struct wl_listener seat_create_listener;
	struct wl_listener new_output;
	struct wl_listener output_move_listener;

};

struct shell_output {
	struct desktop_shell  *shell;
	struct wlr_output  *output;
	struct wl_listener    destroy_listener;
	struct wl_list        link;

	struct wlr_surface *panel_surface;
	struct wl_listener panel_surface_listener;

	struct wlr_surface *background_surface;
	struct wl_listener background_surface_listener;

	struct {
		struct weston_curtain *curtain;
		struct weston_view_animation *animation;
		enum fade_type type;
		struct wl_event_source *startup_timer;
	} fade;
};