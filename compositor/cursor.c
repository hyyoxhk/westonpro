// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <weston-pro.h>

static struct wet_view *desktop_view_at(
		struct server *server, double lx, double ly,
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

static void
request_set_selection_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

static void
process_cursor_move(struct server *server, uint32_t time)
{
	// /* Move the grabbed view to the new position. */
	// struct wet_view *view = server->grabbed_view;
	// view->x = server->cursor->x - server->grab_x;
	// view->y = server->cursor->y - server->grab_y;
	// wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
}

static void
process_cursor_resize(struct server *server, uint32_t time)
{
	// struct wet_view *view = server->grabbed_view;
	// double border_x = server->cursor->x - server->grab_x;
	// double border_y = server->cursor->y - server->grab_y;
	// int new_left = server->grab_geobox.x;
	// int new_right = server->grab_geobox.x + server->grab_geobox.width;
	// int new_top = server->grab_geobox.y;
	// int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	// if (server->resize_edges & WLR_EDGE_TOP) {
	// 	new_top = border_y;
	// 	if (new_top >= new_bottom) {
	// 		new_top = new_bottom - 1;
	// 	}
	// } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
	// 	new_bottom = border_y;
	// 	if (new_bottom <= new_top) {
	// 		new_bottom = new_top + 1;
	// 	}
	// }
	// if (server->resize_edges & WLR_EDGE_LEFT) {
	// 	new_left = border_x;
	// 	if (new_left >= new_right) {
	// 		new_left = new_right - 1;
	// 	}
	// } else if (server->resize_edges & WLR_EDGE_RIGHT) {
	// 	new_right = border_x;
	// 	if (new_right <= new_left) {
	// 		new_right = new_left + 1;
	// 	}
	// }

	// struct wlr_box geo_box;
	// wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
	// view->x = new_left - geo_box.x;
	// view->y = new_top - geo_box.y;
	// wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);

	// int new_width = new_right - new_left;
	// int new_height = new_bottom - new_top;
	// wlr_xdg_toplevel_set_size(view->xdg_toplevel, new_width, new_height);
}

static void process_cursor_motion(struct server *server, uint32_t time)
{
	// if (server->cursor_mode == CURSOR_MOVE) {
	// 	process_cursor_move(server, time);
	// 	return;
	// } else if (server->cursor_mode == CURSOR_RESIZE) {
	// 	process_cursor_resize(server, time);
	// 	return;
	// }

	// double sx, sy;
	// struct wlr_seat *seat = server->myseat.seat;
	// struct wlr_surface *surface = NULL;
	// struct wet_view *view = desktop_view_at(server,
	// 		server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	// if (!view) {
	// 	wlr_xcursor_manager_set_cursor_image(
	// 			server->cursor_mgr, "left_ptr", server->cursor);
	// }
	// if (surface) {
	// 	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	// 	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	// } else {
	// 	wlr_seat_pointer_clear_focus(seat);
	// }
}

static void
cursor_motion_notify(struct wl_listener *listener, void *data)
{
// 	struct seat *seat = wl_container_of(listener, seat, cursor_button);
// 	struct wlr_pointer_motion_event *event = data;

// 	wlr_cursor_move(seat->cursor, &event->pointer->base,
// 			event->delta_x, event->delta_y);
// 	process_cursor_motion(seat, event->time_msec);
}

static void
cursor_motion_absolute_notify(struct wl_listener *listener, void *data)
{
	// struct seat *seat = wl_container_of(listener, seat, cursor_motion_absolute);
	// struct wlr_pointer_motion_absolute_event *event = data;

	// wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
	// process_cursor_motion(server, event->time_msec);
}

static void
cursor_button_notify(struct wl_listener *listener, void *data)
{
	// struct server *server = wl_container_of(listener, server, cursor_button);
	// struct wlr_pointer_button_event *event = data;

	// wlr_seat_pointer_notify_button(server->seat,
	// 		event->time_msec, event->button, event->state);
	// double sx, sy;
	// struct wlr_surface *surface = NULL;
	// struct wet_view *view = desktop_view_at(server,
	// 		server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	// if (event->state == WLR_BUTTON_RELEASED) {
	// 	server->cursor_mode = CURSOR_PASSTHROUGH;
	// } else {
	// 	focus_view(view, surface);
	// }
}

static void
cursor_axis_notify(struct wl_listener *listener, void *data)
{
	// struct server *server = wl_container_of(listener, server, cursor_axis);
	// struct wlr_pointer_axis_event *event = data;

	// wlr_seat_pointer_notify_axis(server->seat,
	// 		event->time_msec, event->orientation, event->delta,
	// 		event->delta_discrete, event->source);
}

static void
cursor_frame_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, cursor_frame);

	wlr_seat_pointer_notify_frame(seat->seat);
}

void cursor_init(struct seat *seat)
{
	seat->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(seat->cursor_mgr, 1);

	seat->cursor_motion.notify = cursor_motion_notify;
	wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion);

	seat->cursor_motion_absolute.notify = cursor_motion_absolute_notify;
	wl_signal_add(&seat->cursor->events.motion_absolute, &seat->cursor_motion_absolute);

	seat->cursor_button.notify = cursor_button_notify;
	wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);

	seat->cursor_axis.notify = cursor_axis_notify;
	wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);

	seat->cursor_frame.notify = cursor_frame_notify;
	wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);

	seat->request_set_selection.notify = request_set_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_selection,
		&seat->request_set_selection);
}
