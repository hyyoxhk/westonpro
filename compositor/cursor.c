// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023 He Yong <hyyoxhk@163.com>
 */

#include <weston-pro.h>

static struct wet_view *desktop_view_at(
		struct wet_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the wet_view at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree->node.data;
}

static void process_cursor_move(struct wet_server *server, uint32_t time)
{
	/* Move the grabbed view to the new position. */
	struct wet_view *view = server->grabbed_view;
	view->x = server->cursor->x - server->grab_x;
	view->y = server->cursor->y - server->grab_y;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
}

static void process_cursor_resize(struct wet_server *server, uint32_t time)
{
	struct wet_view *view = server->grabbed_view;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
	view->x = new_left - geo_box.x;
	view->y = new_top - geo_box.y;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, new_width, new_height);
}

static void process_cursor_motion(struct wet_server *server, uint32_t time)
{
	if (server->cursor_mode == CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct wet_view *view = desktop_view_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	if (!view) {
		wlr_xcursor_manager_set_cursor_image(
				server->cursor_mgr, "left_ptr", server->cursor);
	}
	if (surface) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void server_cursor_motion(struct wl_listener *listener, void *data)
{
	struct wet_server *server = wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;

	wlr_cursor_move(server->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct wet_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;

	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data)
{
	struct wet_server *server = wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;

	wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wet_view *view = desktop_view_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (event->state == WLR_BUTTON_RELEASED) {
		server->cursor_mode = CURSOR_PASSTHROUGH;
	} else {
		focus_view(view, surface);
	}
}

static void server_cursor_axis(struct wl_listener *listener, void *data)
{
	struct wet_server *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;

	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);
}

static void server_cursor_frame(struct wl_listener *listener, void *data)
{
	struct wet_server *server = wl_container_of(listener, server, cursor_frame);

	wlr_seat_pointer_notify_frame(server->seat);
}

void cursor_init(struct wet_server *server)
{
	server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->cursor_mgr, 1);

	server->cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);

	server->cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server->cursor->events.motion_absolute, &server->cursor_motion_absolute);

	server->cursor_button.notify = server_cursor_button;
	wl_signal_add(&server->cursor->events.button, &server->cursor_button);

	server->cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);

	server->cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);
}
