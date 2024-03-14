// SPDX-License-Identifier: MIT
/*
 * Copyright © 2019 Collabora Ltd
 *
 * Ported from Weston-11.0 - libweston/weston-log-internal.h
 */

#ifndef WESTON_PRO_LOG_INTERNAL_H
#define WESTON_PRO_LOG_INTERNAL_H

#include <wayland-server-core.h>

struct log_scope;
struct log_context;
struct log_subscription;

struct log_subscriber {
	/** write the data pointed by @param data */
	void (*write)(struct log_subscriber *sub, const char *data, size_t len);
	/** For destroying the subscriber */
	void (*destroy)(struct log_subscriber *sub);
	/** For the type of streams that required additional destroy operation
	 * for destroying the stream */
	void (*destroy_subscription)(struct log_subscriber *sub);
	/** For the type of streams that can inform the 'consumer' part that
	 * write operation has been terminated/finished and should close the
	 * stream.
	 */
	void (*complete)(struct log_subscriber *sub);
};

void
log_subscription_create(struct log_subscriber *owner, struct log_scope *scope);

void
log_subscription_destroy(struct log_subscription *sub);

void
weston_debug_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

struct log_scope *
log_get_scope(struct log_context *log_ctx, const char *name);

void
weston_debug_protocol_advertise_scopes(struct log_context *log_ctx,
				       struct wl_resource *res);

int
vlog(const char *fmt, va_list ap);

int
vlog_continue(const char *fmt, va_list ap);

#endif
