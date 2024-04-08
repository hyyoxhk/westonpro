// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <weston-pro.h>
#include "shared/util.h"

bool server_init(struct server *server)
{
	struct wlr_compositor *compositor;
	struct wlr_data_device_manager *device_manager;

	server->backend = wlr_backend_autocreate(server->wl_display);
	if (!server->backend) {
		weston_log("failed to create backend\n");
		goto failed;
	}

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		weston_log("failed to create renderer\n");
		goto failed;
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (!server->allocator) {
		weston_log("failed to create allocator\n");
		goto failed;
	}

	wl_list_init(&server->views);

	server->scene = wlr_scene_create();
	if (!server->scene) {
		weston_log("failed to create scene");
		goto failed;
	}

	if (!output_init(server))
		goto failed;

	compositor = wlr_compositor_create(server->wl_display, server->renderer);
	if (!compositor) {
		weston_log("failed to create the wlroots compositor\n");
		goto failed;
	}

	wlr_subcompositor_create(server->wl_display);


	device_manager = wlr_data_device_manager_create(server->wl_display);
	if (!device_manager) {
		weston_log("failed to create data device manager\n");
		goto failed;
	}

	seat_init(server);

	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
	if (!server->xdg_shell) {
		weston_log("failed to create the XDG shell interface\n");
		goto failed;
	}

	server->new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);

	return true;
failed:
	return false;
}

/*
 * 
 */
static int
create_listening_socket(struct wl_display *display, const char *socket_name)
{
	char name_candidate[32];

	if (socket_name) {
		if (wl_display_add_socket(display, socket_name)) {
			// weston_log("fatal: failed to add socket: %s\n",
			// 	   strerror(errno));
			return -1;
		}

		setenv("WAYLAND_DISPLAY", socket_name, 1);
		return 0;
	} else {
		for (int i = 1; i <= 32; i++) {
			sprintf(name_candidate, "wayland-%d", i);
			if (wl_display_add_socket(display, name_candidate) >= 0) {
				setenv("WAYLAND_DISPLAY", name_candidate, 1);
				return 0;
			}
		}
		// weston_log("fatal: failed to add socket: %s\n",
		// 	   strerror(errno));
		return -1;
	}
}

bool server_start(struct server *server)
{
	if (create_listening_socket(server->wl_display, NULL)) {
		wlr_backend_destroy(server->backend);
		return false;
	}


	if (!wlr_backend_start(server->backend)) {
		wlr_backend_destroy(server->backend);
		wl_display_destroy(server->wl_display);
		return false;
	}

	return true;
}

struct server *
server_create(struct wl_display *display, struct log_context *log_ctx)
{
	struct server *server;

	server = zalloc(sizeof *server);
	if (!server)
		return NULL;

	server->log_ctx = log_ctx;
	server->wl_display = display;
	wl_signal_init(&server->destroy_signal);

	server->backend = wlr_backend_autocreate(server->wl_display);
	if (!server->backend) {
		weston_log("failed to create backend\n");
		goto failed;
	}

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		weston_log("failed to create renderer\n");
		goto failed_destroy_backend;
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (!server->allocator) {
		weston_log("failed to create allocator\n");
		goto failed_destroy_renderer;
	}

	server->scene = wlr_scene_create();
	if (!server->scene) {
		weston_log("failed to create scene\n");
		goto failed_destroy_allocator;
	}

	if (wlr_compositor_create(server->wl_display, server->renderer)) {
		weston_log("failed to create the wlroots compositor\n");
		goto failed_destroy_scene;
	}

	if (wlr_subcompositor_create(server->wl_display)) {
		weston_log("failed to create the wlroots subcompositor\n");
		goto failed_destroy_scene;
	}

	if (wlr_viewporter_create(server->wl_display)) {
		weston_log("failed to create the wlroots viewporter\n");
		goto failed_destroy_scene;
	}

	server->output_layout = wlr_output_layout_create();
	if (!server->output_layout) {
		weston_log("failed to create output_layout\n");
		goto failed_destroy_scene;
	}
	
	if (wlr_scene_attach_output_layout(server->scene, server->output_layout)) {
		weston_log("failed to attach output layout\n");
		goto failed_destroy_output_layout;
	}

	if (wlr_xdg_output_manager_v1_create(server->wl_display, server->output_layout)) {
		weston_log("failed to attach output layout\n");
		goto failed_destroy_output_layout;
	}


failed_destroy_output_layout:
	wlr_output_layout_destroy(server->output_layout);
failed_destroy_scene:
	wlr_scene_node_destroy(&server->scene->tree.node);
failed_destroy_allocator:
	wlr_allocator_destroy(server->allocator);
failed_destroy_renderer:
	wlr_renderer_destroy(server->renderer);
failed_destroy_backend:
	wlr_backend_destroy(server->backend);
failed:
	free(server);
	return NULL;
}

void
server_destory(struct server *server)
{
	wl_signal_emit_mutable(&server->destroy_signal, server);

}
