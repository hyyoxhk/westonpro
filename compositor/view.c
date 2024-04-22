// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <assert.h>
#include <weston-pro.h>


void focus_view(struct wet_view *view, struct wlr_surface *surface)
{
	// if (view == NULL) {
	// 	return;
	// }
	// struct server *server = view->server;
	// struct wlr_seat *seat = server->seat;
	// struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	// if (prev_surface == surface) {
	// 	/* Don't re-focus an already focused surface. */
	// 	return;
	// }
	// if (prev_surface) {
	// 	struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
	// 				seat->keyboard_state.focused_surface);
	// 	assert(previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
	// 	wlr_xdg_toplevel_set_activated(previous->toplevel, false);
	// }
	// struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	// /* Move the view to the front */
	// wlr_scene_node_raise_to_top(&view->scene_tree->node);
	// wl_list_remove(&view->link);
	// wl_list_insert(&server->view_list, &view->link);

	// wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);

	// if (keyboard != NULL) {
	// 	wlr_seat_keyboard_notify_enter(seat, view->xdg_toplevel->base->surface,
	// 		keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	// }
}
