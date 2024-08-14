// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include "config.h"

#include "desktop-shell.h"
#include "shared/util.h"
#include "wlrston.h"

static struct shell_output *
find_shell_output_from_weston_output(struct desktop_shell *shell,
				     struct wlr_output *output)
{
	struct shell_output *shell_output;

	wl_list_for_each(shell_output, &shell->output_list, link) {
		if (shell_output->output == output)
			return shell_output;
	}

	return NULL;
}

static void
handle_background_surface_destroy(struct wl_listener *listener, void *data)
{
	struct shell_output *output = wl_container_of(listener, output, background_surface_listener);

	// weston_log("background surface gone\n");
	wl_list_remove(&output->background_surface_listener.link);
	output->background_surface = NULL;
}

static void
desktop_shell_set_background(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *output_resource,
			     struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct wlr_output *output = wl_resource_get_user_data(output_resource);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	struct shell_output *sh_output;

	sh_output = find_shell_output_from_weston_output(shell, output);
	if (sh_output->background_surface) {
		/* The output already has a background, tell our helper
		 * there is no need for another one. */
		weston_desktop_shell_send_configure(resource, 0,
						    surface_resource,
						    0, 0);
	} else {
		weston_desktop_shell_send_configure(resource, 0,
						    surface_resource,
						    output->width,
						    output->height);

		sh_output->background_surface = surface;

		sh_output->background_surface_listener.notify =
					handle_background_surface_destroy;
		wl_signal_add(&surface->events.destroy,
			      &sh_output->background_surface_listener);
	}

}

static void
desktop_shell_set_panel(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *output_resource,
			struct wl_resource *surface_resource)
{
}

static void
desktop_shell_set_lock_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
}

static void
desktop_shell_unlock(struct wl_client *client,
		     struct wl_resource *resource)
{
}

static void
desktop_shell_set_grab_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
}

static void
desktop_shell_desktop_ready(struct wl_client *client,
			    struct wl_resource *resource)
{
}

static void
desktop_shell_set_panel_position(struct wl_client *client,
				 struct wl_resource *resource,
				 uint32_t position)
{
}

static const struct weston_desktop_shell_interface desktop_shell_implementation = {
	desktop_shell_set_background,
	desktop_shell_set_panel,
	desktop_shell_set_lock_surface,
	desktop_shell_unlock,
	desktop_shell_set_grab_surface,
	desktop_shell_desktop_ready,
	desktop_shell_set_panel_position
};

static void
launch_desktop_shell_process(void *data)
{
	// struct desktop_shell *shell = data;

	// shell->child.client = weston_client_start(shell->compositor,
	// 					  shell->client);

	// if (!shell->child.client) {
	// 	weston_log("not able to start %s\n", shell->client);
	// 	return;
	// }

	// shell->child.client_destroy_listener.notify =
	// 	desktop_shell_client_destroy;
	// wl_client_add_destroy_listener(shell->child.client,
	// 			       &shell->child.client_destroy_listener);
}

static void
unbind_desktop_shell(struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	// if (shell->locked)
		// resume_desktop(shell);

	shell->child.desktop_shell = NULL;
	shell->prepare_event_sent = false;
}

static void
bind_desktop_shell(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &weston_desktop_shell_interface,
				      1, id);

	if (client == shell->child.client) {
		wl_resource_set_implementation(resource,
					       &desktop_shell_implementation,
					       shell, unbind_desktop_shell);
		shell->child.desktop_shell = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "permission to bind desktop_shell denied");
}

static void
create_shell_output(struct desktop_shell *shell, struct wet_output *output)
{
// output_list
}

static void
new_output_notify1(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell = wl_container_of(listener, shell, new_output);
	struct wlr_output *wlr_output = data;

	// create_shell_output(shell, wlr_output);
}

static void
setup_output_handler(struct server *server, struct desktop_shell *shell)
{
	struct wet_output *output;

	wl_list_init(&shell->output_list);
	wl_list_for_each(output, &server->output_list, link)
		create_shell_output(shell, output);

	shell->new_output.notify = new_output_notify1;
	wl_signal_add(&server->backend->events.new_output, &shell->new_output);
}

static void
shell_destroy(struct wl_listener *listener, void *data)
{

}

bool
server_add_destroy_listener_once(struct server *server,
				 struct wl_listener *listener,
				 wl_notify_func_t destroy_handler)
{
	if (wl_signal_get(&server->destroy_signal, destroy_handler))
		return false;

	listener->notify = destroy_handler;
	wl_signal_add(&server->destroy_signal, listener);
	return true;
}

static void
idle_handler(struct wl_listener *listener, void *data)
{
	//struct desktop_shell *shell = wl_container_of(listener, shell, idle_listener);


	// wl_list_for_each(seat, &shell->server->seat_list, link)
	// 	weston_seat_break_desktop_grabs(seat);

	// shell_fade(shell, FADE_OUT);
	/* lock() is called from shell_fade_done_for_output() */
}

int
wlrston_shell_init(struct server *server, int *argc, char *argv[])
{
	struct desktop_shell *shell;
	struct wl_event_loop *loop;

	shell = zalloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	shell->server = server;

	if (!server_add_destroy_listener_once(server,
					      &shell->destroy_listener, 
					      shell_destroy)) {
		free(shell);
		return 0;
	}

	shell->idle_listener.notify = idle_handler;
	wl_signal_add(&server->idle_signal, &shell->idle_listener);

	shell->background_tree = wlr_scene_tree_create(&server->scene->tree);
	shell->view_tree = wlr_scene_tree_create(&server->scene->tree);
	shell->panel_tree = wlr_scene_tree_create(&server->scene->tree);
	shell->fullscreen_tree = wlr_scene_tree_create(&server->scene->tree);
// 	input_panel_layer
	shell->lock_tree = wlr_scene_tree_create(&server->scene->tree);
	// shell->minimized_tree = wlr_scene_tree_create(&server->scene->tree);

	shell->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
	shell->new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&shell->xdg_shell->events.new_surface, &shell->new_xdg_surface);

	if (wl_global_create(server->wl_display,
			     &weston_desktop_shell_interface, 1,
			     shell, bind_desktop_shell) == NULL)
		return -1;

	setup_output_handler(server, shell);

	loop = wl_display_get_event_loop(server->wl_display);
	wl_event_loop_add_idle(loop, launch_desktop_shell_process, shell);

}