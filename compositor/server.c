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

static void
handle_primary_client_destroyed(struct wl_listener *listener, void *data)
{
	struct wl_client *client = data;

	weston_log("Primary client died.  Closing...\n");

	wl_display_terminate(wl_client_get_display(client));
}

bool server_start(struct server *server)
{
	struct wl_listener primary_client_destroyed;
	struct wl_client *primary_client;
	char *server_socket = NULL;
	int fd;

	server_socket = getenv("WAYLAND_SERVER_SOCKET");
	if (server_socket) {
		weston_log("Running with single client\n");
		if (!safe_strtoint(server_socket, &fd))
			fd = -1;
	} else {
		fd = -1;
	}

	if (fd != -1) {
		primary_client = wl_client_create(server->wl_display, fd);
		if (!primary_client) {
			weston_log("fatal: failed to add client: %s\n",
				   strerror(errno));
			// goto out;
		}
		primary_client_destroyed.notify =
			handle_primary_client_destroyed;
		wl_client_add_destroy_listener(primary_client,
					       &primary_client_destroyed);
	} else if (create_listening_socket(server->wl_display, socket_name)) {
		// goto out;
	}

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
server_create(struct wl_display *display)
{
	struct wlr_compositor *compositor;
	struct server *server;

	server = zalloc(sizeof *server);
	if (!server)
		return NULL;

	server->wl_display = display;

	server->backend = wlr_backend_autocreate(server->wl_display);
	if (!server->backend) {
		printf("failed to create backend\n");
		goto failed;
	}

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		printf("failed to create renderer\n");
		goto failed;
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (!server->allocator) {
		printf("failed to create allocator\n");
		goto failed;
	}

	if (wlr_compositor_create(server->wl_display, server->renderer)) {
		printf("failed to create the wlroots compositor\n");
		goto failed;
	}

	if (wlr_subcompositor_create(server->wl_display)) {
		printf("failed to create the wlroots subcompositor\n");
		goto failed;
	}

	if (wlr_viewporter_create(server->wl_display)) {
		printf("failed to create the wlroots viewporter\n");
		goto failed;
	}

	if (!output_init(server))
		goto failed;


failed:
	free(server);
	return NULL;
}

// server_destory
