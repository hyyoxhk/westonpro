// SPDX-License-Identifier: MIT
/*
 * Copyright © 2019 Collabora Ltd.
 * 
 * Ported from Weston-11.0 - libweston/weston-log-wayland.c
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <log.h>
#include "weston-debug-protocol.h"
#include "log-internal.h"
#include "util.h"

struct log_debug_wayland {
	struct log_subscriber base;
	int fd;				/**< client provided fd */
	struct wl_resource *resource;	/**< weston_debug_stream_v1 object */
};

static struct log_debug_wayland *
to_log_debug_wayland(struct log_subscriber *sub)
{
	return wl_container_of(sub, (struct log_debug_wayland *)NULL, base);
}

static void
stream_close_unlink(struct log_debug_wayland *stream)
{
	if (stream->fd != -1)
		close(stream->fd);
	stream->fd = -1;
}

static void WL_PRINTF(2, 3)
stream_close_on_failure(struct log_debug_wayland *stream, const char *fmt, ...)
{
	char *msg;
	va_list ap;
	int ret;

	stream_close_unlink(stream);

	va_start(ap, fmt);
	ret = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (ret > 0) {
		weston_debug_stream_v1_send_failure(stream->resource, msg);
		free(msg);
	} else {
		weston_debug_stream_v1_send_failure(stream->resource,
						    "MEMFAIL");
	}
}

static void
log_debug_wayland_write(struct log_subscriber *sub, const char *data, size_t len)
{
	ssize_t len_ = len;
	ssize_t ret;
	int e;
	struct log_debug_wayland *stream = to_log_debug_wayland(sub);

	if (stream->fd == -1)
		return;

	while (len_ > 0) {
		ret = write(stream->fd, data, len_);
		e = errno;
		if (ret < 0) {
			if (e == EINTR)
				continue;

			stream_close_on_failure(stream,
					"Error writing %zd bytes: %s (%d)",
					len_, strerror(e), e);
			break;
		}

		len_ -= ret;
		data += ret;
	}
}


static void
log_debug_wayland_complete(struct log_subscriber *sub)
{
	struct log_debug_wayland *stream = to_log_debug_wayland(sub);

	stream_close_unlink(stream);
	weston_debug_stream_v1_send_complete(stream->resource);
}

static void
log_debug_wayland_to_destroy(struct log_subscriber *sub)
{
	struct log_debug_wayland *stream = to_log_debug_wayland(sub);

	if (stream->fd != -1)
		stream_close_on_failure(stream, "debug name removed");
}

static struct log_debug_wayland *
stream_create(struct log_context *log_ctx, const char *name,
	      int32_t streamfd, struct wl_resource *stream_resource)
{
	struct log_debug_wayland *stream;
	struct log_scope *scope;

	stream = zalloc(sizeof *stream);
	if (!stream)
		return NULL;

	stream->fd = streamfd;
	stream->resource = stream_resource;

	stream->base.write = log_debug_wayland_write;
	stream->base.destroy = NULL;
	stream->base.destroy_subscription = log_debug_wayland_to_destroy;
	stream->base.complete = log_debug_wayland_complete;
	// wl_list_init(&stream->base.subscription_list);

	scope = log_get_scope(log_ctx, name);
	if (scope) {
		log_subscription_create(&stream->base, scope);
	} else {
		stream_close_on_failure(stream,
					"Debug stream name '%s' is unknown.",
					name);
	}

	return stream;
}

static void
stream_destroy(struct wl_resource *stream_resource)
{
	struct log_debug_wayland *stream;
	stream = wl_resource_get_user_data(stream_resource);

	stream_close_unlink(stream);
	// log_subscriber_release(&stream->base);
	free(stream);
}

static void
weston_debug_stream_destroy(struct wl_client *client, struct wl_resource *stream_resource)
{
	wl_resource_destroy(stream_resource);
}

static const struct weston_debug_stream_v1_interface weston_debug_stream_impl = {
	weston_debug_stream_destroy
};

static void
weston_debug_destroy(struct wl_client *client,
		     struct wl_resource *global_resource)
{
	wl_resource_destroy(global_resource);
}

static void
weston_debug_subscribe(struct wl_client *client,
		       struct wl_resource *global_resource,
		       const char *name,
		       int32_t streamfd,
		       uint32_t new_stream_id)
{
	struct log_context *log_ctx;
	struct wl_resource *stream_resource;
	uint32_t version;
	struct log_debug_wayland *stream;

	log_ctx = wl_resource_get_user_data(global_resource);
	version = wl_resource_get_version(global_resource);

	stream_resource = wl_resource_create(client,
					&weston_debug_stream_v1_interface,
					version, new_stream_id);
	if (!stream_resource)
		goto fail;

	stream = stream_create(log_ctx, name, streamfd, stream_resource);
	if (!stream)
		goto fail;

	wl_resource_set_implementation(stream_resource,
				       &weston_debug_stream_impl,
				       stream, stream_destroy);
	return;

fail:
	close(streamfd);
	wl_client_post_no_memory(client);
}

static const struct weston_debug_v1_interface weston_debug_impl = {
	weston_debug_destroy,
	weston_debug_subscribe
};

void
weston_debug_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct log_context *log_ctx = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &weston_debug_v1_interface,
				      version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &weston_debug_impl,
				       log_ctx, NULL);

	// weston_debug_protocol_advertise_scopes(log_ctx, resource);
}
