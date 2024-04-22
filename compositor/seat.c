// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include <weston-pro.h>

static void
request_cursor_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = seat->seat->pointer_state.focused_client;

	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(seat->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

static void
request_set_selection_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}



static bool
handle_keybinding(struct server *server, xkb_keysym_t sym)
{
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
		/* Cycle to the next view */
		if (wl_list_length(&server->view_list) < 2) {
			break;
		}
		struct wet_view *next_view = wl_container_of(
			server->view_list.prev, next_view, link);
		focus_view(next_view, next_view->xdg_toplevel->base->surface);
		break;
	default:
		return false;
	}
	return true;
}



static void keyboard_handle_destroy(struct wl_listener *listener, void *data)
{
	struct wet_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);

	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

static void
new_keyboard(struct seat *seat, struct wlr_input_device *device)
{
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
	struct wet_keyboard *keyboard = calloc(1, sizeof(struct wet_keyboard));
	// keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	// keyboard->modifiers.notify = keyboard_modifiers_notify;
	// wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);

	// keyboard->key.notify = keyboard_key_notify;
	// wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(seat->seat, keyboard->wlr_keyboard);

	/* And add the keyboard to our list of keyboards */
	// wl_list_insert(&seat->keyboards, &keyboard->link);
}

static void 
new_pointer(struct seat *seat, struct wlr_input_device *device)
{
	wlr_cursor_attach_input_device(seat->cursor, device);
}

static struct input *
new_touch(struct seat *seat, struct wlr_input_device *dev)
{
}

static void new_input_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		new_keyboard(seat, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		new_pointer(seat, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		new_touch(seat, device);
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	// if (!wl_list_empty(&seat->inputs)) {
	// 	caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	// }
	wlr_seat_set_capabilities(seat->seat, caps);
}



// void seat_init(struct server *server)
// {
// 	server->seat = wlr_seat_create(server->wl_display, "seat0");

// 	server->request_cursor.notify = request_cursor_notify;
// 	wl_signal_add(&server->seat->events.request_set_cursor, &server->request_cursor);

// 	server->request_set_selection.notify = request_set_selection_notify;
// 	wl_signal_add(&server->seat->events.request_set_selection, &server->request_set_selection);

// 	wl_list_init(&server->keyboards);
// 	server->new_input.notify = new_input_notify;
// 	wl_signal_add(&server->backend->events.new_input, &server->new_input);

// 	server->cursor = wlr_cursor_create();

// 	wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
// 	cursor_init(server);
// }

void
seat_init(struct server *server)
{
	struct seat *seat = &server->seat;
	seat->server = server;

	seat->seat = wlr_seat_create(server->wl_display, "seat0");



	seat->new_input.notify = new_input_notify;
	wl_signal_add(&server->backend->events.new_input, &seat->new_input);

	seat->request_set_selection.notify = request_set_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_selection,
		&seat->request_set_selection);

	seat->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);
	seat->request_cursor.notify = request_cursor_notify;
	wl_signal_add(&seat->seat->events.request_set_cursor, &seat->request_cursor);
	keyboard_init(seat);

}
