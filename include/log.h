// SPDX-License-Identifier: MIT
/*
 * Copyright © 2017 Pekka Paalanen <pq@iki.fi>
 *
 * Ported from Weston-11.0 - include/libweston/weston-log.h
 */

#ifndef WESTON_PRO_LOG_H
#define WESTON_PRO_LOG_H

#include <stdbool.h>

struct log_context;
struct log_subscriber;
struct log_subscription;
struct log_scope;
struct server;

void
server_enable_debug_protocol(struct server *server);

bool
server_is_debug_protocol_enabled(struct server *server);

typedef void (*log_scope_cb)(struct log_subscription *sub, void *user_data);

struct log_scope *
log_ctx_add_log_scope(struct log_context *log_ctx,
		      const char *name,
		      const char *description,
		      log_scope_cb new_subscription,
		      log_scope_cb destroy_subscription,
		      void *user_data);

// struct log_scope *
// server_add_log_scope(struct server *server,
// 		     const char *name,
// 		     const char *description,
// 		     log_scope_cb new_subscription,
// 		     log_scope_cb destroy_subscription,
// 		     void *user_data);

void
log_scope_destroy(struct log_scope *scope);

bool
log_scope_is_enabled(struct log_scope *scope);

void
log_scope_write(struct log_scope *scope, const char *data, size_t len);

int
log_scope_vprintf(struct log_scope *scope, const char *fmt, va_list ap);

int
log_scope_printf(struct log_scope *scope,
		 const char *fmt, ...)
		 __attribute__ ((format (printf, 2, 3)));

void
log_subscription_printf(struct log_subscription *sub,
			const char *fmt, ...)
			__attribute__ ((format (printf, 2, 3)));

void
log_scope_complete(struct log_scope *scope);

void
log_subscription_complete(struct log_subscription *sub);

char *
log_scope_timestamp(struct log_scope *scope,
		    char *buf, size_t len);

char *
log_timestamp(char *buf, size_t len, int *cached_tm_mday);

void
log_subscriber_destroy(struct log_subscriber *subscriber);

void
log_subscribe(struct log_context *log_ctx,
	      struct log_subscriber *subscriber,
	      const char *scope_name);

struct log_subscriber *
log_subscriber_create_log(FILE *dump_to);



struct log_context *log_ctx_create(void);

void log_ctx_destroy(struct log_context *log_ctx);

#endif
