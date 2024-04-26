// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <wlr/types/wlr_keyboard_group.h>

#include <weston-pro.h>

void
keyboard_modifiers_notify(struct wl_listener *listener, void *data)
{
	// struct wet_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

	// wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	// wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
	// 	&keyboard->wlr_keyboard->modifiers);
}


// static bool
// handle_keybinding(struct server *server, xkb_keysym_t sym)
// {
// 	switch (sym) {
// 	case XKB_KEY_Escape:
// 		wl_display_terminate(server->wl_display);
// 		break;
// 	case XKB_KEY_F1:
// 		/* Cycle to the next view */
// 		if (wl_list_length(&server->view_list) < 2) {
// 			break;
// 		}
// 		struct wet_view *next_view = wl_container_of(
// 			server->view_list.prev, next_view, link);
// 		focus_view(next_view, next_view->xdg_toplevel->base->surface);
// 		break;
// 	default:
// 		return false;
// 	}
// 	return true;
// }

void
keyboard_key_notify(struct wl_listener *listener, void *data)
{
	// struct seat *seat = wl_container_of(listener, seat, keyboard_key);
	// struct server *server = seat->server;
	// struct wlr_keyboard_key_event *event = data;
	// struct wlr_seat *wlr_seat = server->myseat.seat;

	// /* Translate libinput keycode -> xkbcommon */
	// uint32_t keycode = event->keycode + 8;
	// /* Get a list of keysyms based on the keymap for this keyboard */
	// const xkb_keysym_t *syms;
	// // int nsyms = xkb_state_key_get_syms(
	// // 		keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	// bool handled = false;
	// uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	// if ((modifiers & WLR_MODIFIER_ALT) &&
	// 		event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
	// 	/* If alt is held down and this button was _pressed_, we attempt to
	// 	 * process it as a compositor keybinding. */
	// 	for (int i = 0; i < nsyms; i++) {
	// 		handled = handle_keybinding(server, syms[i]);
	// 	}
	// }

	// if (!handled) {
	// 	/* Otherwise, we pass it along to the client. */
	// 	wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
	// 	wlr_seat_keyboard_notify_key(seat, event->time_msec,
	// 		event->keycode, event->state);
	// }
}

void
keyboard_init(struct seat *seat)
{
	// seat->keyboard_group = wlr_keyboard_group_create();
	// struct wlr_keyboard *kb = &seat->keyboard_group->keyboard;

	// seat->keyboard_key.notify = keyboard_key_notify;
	
	// wl_signal_add(&kb->events.key, &seat->keyboard_key);

	// seat->keyboard_modifiers.notify = keyboard_modifiers_notify;
	// wl_signal_add(&kb->events.modifiers, &seat->keyboard_modifiers);

}
