// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include "shared/util.h"
#include <stdbool.h>
#include <stdlib.h>

#include <weston-pro.h>


static void
input_device_destroy(struct wl_listener *listener, void *data)
{
	// struct input *input = wl_container_of(listener, input, destroy);
	// wl_list_remove(&input->link);
	// wl_list_remove(&input->destroy.link);

	// /* `struct keyboard` is derived and has some extra clean up to do */
	// if (input->wlr_input_device->type == WLR_INPUT_DEVICE_KEYBOARD) {
	// 	struct keyboard *keyboard = (struct keyboard *)input;
	// 	wl_list_remove(&keyboard->key.link);
	// 	wl_list_remove(&keyboard->modifier.link);
	// 	keyboard_cancel_keybind_repeat(keyboard);
	// }
	// free(input);
}

static bool
is_touch_device(struct wlr_input_device *wlr_input_device)
{
	switch (wlr_input_device->type) {
	case WLR_INPUT_DEVICE_TOUCH:
	case WLR_INPUT_DEVICE_TABLET_TOOL:
	case WLR_INPUT_DEVICE_TABLET_PAD :
		return true;
	default:
		break;
	}
	return false;
}

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
keyboard_handle_destroy(struct wl_listener *listener, void *data)
{
	// struct wet_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);

	// wl_list_remove(&keyboard->modifiers.link);
	// wl_list_remove(&keyboard->key.link);
	// wl_list_remove(&keyboard->destroy.link);
	// wl_list_remove(&keyboard->link);
	// free(keyboard);
}

static struct input *
new_pointer(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = zalloc(sizeof(*input));
	input->wlr_input_device = dev;

	wlr_cursor_attach_input_device(seat->cursor, dev);

	return input;
}

static struct input *
new_keyboard(struct seat *seat, struct wlr_input_device *device, bool virtual)
{
	struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);

	struct wet_keyboard *keyboard = zalloc(sizeof(*keyboard));
	keyboard->base.wlr_input_device = device;
	keyboard->wlr_keyboard = kb;

	wlr_keyboard_set_keymap(kb, seat->keyboard_group->keyboard.keymap);

	if (!virtual) {
		wlr_keyboard_group_add_keyboard(seat->keyboard_group, kb);
	}

	keyboard->key.notify = keyboard_key_notify;
	wl_signal_add(&kb->events.key, &keyboard->key);
	keyboard->modifiers.notify = keyboard_modifiers_notify;
	wl_signal_add(&kb->events.modifiers, &keyboard->modifiers);

	wlr_seat_set_keyboard(seat->seat, kb);

	return (struct input *)keyboard;
}



static struct input *
new_touch(struct seat *seat, struct wlr_input_device *dev)
{
}

static void
seat_update_capabilities(struct seat *seat)
{
	struct input *input = NULL;
	uint32_t caps = 0;

	wl_list_for_each(input, &seat->input_list, link) {
		switch (input->wlr_input_device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			caps |= WL_SEAT_CAPABILITY_KEYBOARD;
			break;
		case WLR_INPUT_DEVICE_POINTER:
			caps |= WL_SEAT_CAPABILITY_POINTER;
			break;
		case WLR_INPUT_DEVICE_TOUCH:
			caps |= WL_SEAT_CAPABILITY_TOUCH;
			break;
		default:
			break;
		}
	}
	wlr_seat_set_capabilities(seat->seat, caps);
}

static void
seat_add_device(struct seat *seat, struct input *input)
{
	input->seat = seat;
	input->destroy.notify = input_device_destroy;
	wl_signal_add(&input->wlr_input_device->events.destroy, &input->destroy);
	wl_list_insert(&seat->input_list, &input->link);

	seat_update_capabilities(seat);
}

static void new_input_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;
	struct input *input = NULL;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		input = new_keyboard(seat, device, false);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		input = new_pointer(seat, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		// input = new_touch(seat, device);
		break;
	default:
		break;
	}

	seat_add_device(seat, input);
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

	seat->request_cursor.notify = request_cursor_notify;
	wl_signal_add(&seat->seat->events.request_set_cursor, &seat->request_cursor);

	/* TODO: */
	seat->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	keyboard_init(seat);
	cursor_init(seat);
	// touch_init(seat);
}

void
seat_finish(struct server *server)
{
	// struct seat *seat = &server->seat;
	// wl_list_remove(&seat->new_input.link);

	// struct input *input, *next;
	// wl_list_for_each_safe(input, next, &seat->inputs, link) {
	// 	input_device_destroy(&input->destroy, NULL);
	// }

	// keyboard_finish(seat);
	/*
	 * Caution - touch_finish() unregisters event listeners from
	 * seat->cursor and must come before cursor_finish(), otherwise
	 * a use-after-free occurs.
	 */
	// touch_finish(seat);
	// cursor_finish(seat);
}