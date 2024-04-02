// SPDX-License-Identifier: MIT
/*
 * Copyright © 2017 Pekka Paalanen <pq@iki.fi>
 * Copyright © 2018 Zodiac Inflight Innovations
 * 
 * Ported from Weston-11.0 - libweston/weston-log.c
 */


#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "weston-debug-protocol.h"

#include <weston-pro.h>
#include <log.h>
#include "log-internal.h"
#include "shared/util.h"

struct log_context {
	struct wl_global *global;
	struct wl_listener server_destroy_listener;
	struct wl_list scope_list; /**< log_scope::link */
	struct wl_list pending_subscription_list; /**< log_subscription::source_link */
};

struct log_scope {
	char *name;
	char *desc;
	log_scope_cb new_subscription;
	log_scope_cb destroy_subscription;
	void *user_data;
	struct wl_list server_link;
	struct wl_list subscription_list;  /**< log_subscription::source_link */
};

struct log_subscription {
	struct log_subscriber *owner;

	char *scope_name;
	struct log_scope *source;
	struct wl_list source_link;     /**< log_scope::subscription_list  or
					  log_context::pending_subscription_list */
};

static struct log_subscription *
find_pending_subscription(struct log_context *log_ctx, const char *scope_name)
{
	struct log_subscription *sub;

	wl_list_for_each(sub, &log_ctx->pending_subscription_list, source_link)
		if (!strcmp(sub->scope_name, scope_name))
			return sub;

	return NULL;
}

static void
log_subscription_create_pending(struct log_subscriber *owner,
				const char *scope_name,
				struct log_context *log_ctx)
{
	assert(owner);
	assert(scope_name);
	struct log_subscription *sub = malloc(sizeof(*sub));

	if (!sub)
		return;

	sub->scope_name = strdup(scope_name);
	sub->owner = owner;

	wl_list_insert(&log_ctx->pending_subscription_list, &sub->source_link);
}

static void
log_subscription_destroy_pending(struct log_subscription *sub)
{
	assert(sub);
	/* pending subsriptions do not have a source */
	wl_list_remove(&sub->source_link);
	free(sub->scope_name);
	free(sub);
}

static void
log_subscription_write(struct log_subscription *sub, const char *data, size_t len)
{
	if (sub->owner && sub->owner->write)
		sub->owner->write(sub->owner, data, len);
}

static void
log_subscription_vprintf(struct log_subscription *sub, const char *fmt, va_list ap)
{
	static const char oom[] = "Out of memory";
	char *str;
	int len;

	if (!log_scope_is_enabled(sub->source))
		return;

	len = vasprintf(&str, fmt, ap);
	if (len >= 0) {
		log_subscription_write(sub, str, len);
		free(str);
	} else {
		log_subscription_write(sub, oom, sizeof oom - 1);
	}
}

static void
log_subscription_add(struct log_scope *scope, struct log_subscription *sub)
{
	assert(scope);
	assert(sub);
	/* don't allow subscriptions to have a source already! */
	assert(!sub->source);

	sub->source = scope;
	wl_list_insert(&scope->subscription_list, &sub->source_link);
}

static void
log_subscription_remove(struct log_subscription *sub)
{
	assert(sub);
	if (sub->source)
		wl_list_remove(&sub->source_link);
	sub->source = NULL;
}

static void
log_run_cb_new_subscription(struct log_subscription *sub)
{
	if (sub->source->new_subscription)
		sub->source->new_subscription(sub, sub->source->user_data);
}

void
log_subscription_create(struct log_subscriber *owner, struct log_scope *scope)
{
	struct log_subscription *sub;
	assert(owner);
	assert(scope);
	assert(scope->name);

	sub = zalloc(sizeof(*sub));
	if (!sub)
		return;

	sub->owner = owner;
	sub->scope_name = strdup(scope->name);

	log_subscription_add(scope, sub);
	log_run_cb_new_subscription(sub);
}

void
log_subscription_destroy(struct log_subscription *sub)
{
	assert(sub);

	if (sub->owner->destroy_subscription)
		sub->owner->destroy_subscription(sub->owner);

	if (sub->source->destroy_subscription)
		sub->source->destroy_subscription(sub, sub->source->user_data);

	log_subscription_remove(sub);
	free(sub->scope_name);
	free(sub);
}

struct log_scope *
log_get_scope(struct log_context *log_ctx, const char *name)
{
	struct log_scope *scope;
	wl_list_for_each(scope, &log_ctx->scope_list, server_link)
		if (strcmp(name, scope->name) == 0)
			return scope;
	return NULL;
}

void
weston_debug_protocol_advertise_scopes(struct log_context *log_ctx,
				       struct wl_resource *res)
{
	struct log_scope *scope;
	wl_list_for_each(scope, &log_ctx->scope_list, server_link)
		weston_debug_v1_send_available(res, scope->name, scope->desc);
}

static void
log_ctx_disable_debug_protocol(struct log_context *log_ctx)
{
	if (!log_ctx->global)
		return;

	wl_global_destroy(log_ctx->global);
	log_ctx->global = NULL;
}

struct log_context *
log_ctx_create(void)
{
	struct log_context *log_ctx;

	log_ctx = zalloc(sizeof *log_ctx);
	if (!log_ctx)
		return NULL;

	wl_list_init(&log_ctx->scope_list);
	wl_list_init(&log_ctx->pending_subscription_list);
	wl_list_init(&log_ctx->server_destroy_listener.link);

	return log_ctx;
}

void
log_ctx_destroy(struct log_context *log_ctx)
{
	struct log_scope *scope;
	struct log_subscription *pending_sub, *pending_sub_tmp;

	/* We can't destroy the log context if there's still a server
	 * that depends on it. This is an user error */
	assert(wl_list_empty(&log_ctx->server_destroy_listener.link));

	log_ctx_disable_debug_protocol(log_ctx);

	wl_list_for_each(scope, &log_ctx->scope_list, server_link)
		fprintf(stderr, "Internal warning: debug scope '%s' has not been destroyed.\n",
			   scope->name);

	/* Remove head to not crash if scope removed later. */
	wl_list_remove(&log_ctx->scope_list);

	/* Remove any pending subscription(s) which nobody subscribed to */
	wl_list_for_each_safe(pending_sub, pending_sub_tmp,
			      &log_ctx->pending_subscription_list, source_link) {
		log_subscription_destroy_pending(pending_sub);
	}

	/* pending_subscription_list should be empty at this point */

	free(log_ctx);
}

static void
server_destroy_listener(struct wl_listener *listener, void *data)
{
	struct log_context *log_ctx =
		wl_container_of(listener, log_ctx, server_destroy_listener);

	/* We have to keep this list initialized as log_ctx_destroy() has
	 * to check if there's any server destroy listener registered */
	wl_list_remove(&log_ctx->server_destroy_listener.link);
	wl_list_init(&log_ctx->server_destroy_listener.link);

	log_ctx_disable_debug_protocol(log_ctx);
}

void
server_enable_debug_protocol(struct wet_server *server)
{
	struct log_context *log_ctx = server->log_ctx;
	assert(log_ctx);
	if (log_ctx->global)
		return;

	log_ctx->global = wl_global_create(server->wl_display,
				       &weston_debug_v1_interface, 1,
				       log_ctx, weston_debug_bind);
	if (!log_ctx->global)
		return;

	log_ctx->server_destroy_listener.notify = server_destroy_listener;
	wl_signal_add(&server->destroy_signal, &log_ctx->server_destroy_listener);

	fprintf(stderr, "WARNING: debug protocol has been enabled. "
		   "This is a potential denial-of-service attack vector and "
		   "information leak.\n");
}

bool
server_is_debug_protocol_enabled(struct wet_server *server)
{
	return server->log_ctx->global != NULL;
}

struct log_scope *
log_ctx_add_log_scope(struct log_context *log_ctx,
		      const char *name,
		      const char *description,
		      log_scope_cb new_subscription,
		      log_scope_cb destroy_subscription,
		      void *user_data)
{
	struct log_subscription *pending_sub = NULL;
	struct log_scope *scope;

	if (!name || !description) {
		fprintf(stderr, "Error: cannot add a debug scope without name or description.\n");
		return NULL;
	}

	if (!log_ctx) {
		fprintf(stderr, "Error: cannot add debug scope '%s', infra not initialized.\n",
			   name);
		return NULL;
	}

	if (log_get_scope(log_ctx, name)){
		fprintf(stderr, "Error: debug scope named '%s' is already registered.\n",
			   name);
		return NULL;
	}

	scope = zalloc(sizeof *scope);
	if (!scope) {
		fprintf(stderr, "Error adding debug scope '%s': out of memory.\n",
			   name);
		return NULL;
	}

	scope->name = strdup(name);
	scope->desc = strdup(description);
	scope->new_subscription = new_subscription;
	scope->destroy_subscription = destroy_subscription;
	scope->user_data = user_data;
	wl_list_init(&scope->subscription_list);

	if (!scope->name || !scope->desc) {
		fprintf(stderr, "Error adding debug scope '%s': out of memory.\n",
			   name);
		free(scope->name);
		free(scope->desc);
		free(scope);
		return NULL;
	}

	wl_list_insert(log_ctx->scope_list.prev, &scope->server_link);

	/* check if there are any pending subscriptions to this scope */
	while ((pending_sub = find_pending_subscription(log_ctx, scope->name)) != NULL) {
		log_subscription_create(pending_sub->owner, scope);

		/* remove it from pending */
		log_subscription_destroy_pending(pending_sub);
	}


	return scope;
}

// struct weston_log_scope *
// weston_compositor_add_log_scope(struct wet_server *server,
// 				const char *name,
// 				const char *description,
// 				log_scope_cb new_subscription,
// 				log_scope_cb destroy_subscription,
// 				void *user_data)
// {
// 	struct weston_log_scope *scope;
// 	scope = weston_log_ctx_add_log_scope(server->weston_log_ctx,
// 					     name, description,
//  					     new_subscription,
// 					     destroy_subscription,
// 					     user_data);
// 	return scope;
// }

void
log_scope_destroy(struct log_scope *scope)
{
	struct log_subscription *sub, *sub_tmp;

	if (!scope)
		return;

	wl_list_for_each_safe(sub, sub_tmp, &scope->subscription_list, source_link)
		log_subscription_destroy(sub);

	wl_list_remove(&scope->server_link);
	free(scope->name);
	free(scope->desc);
	free(scope);
}

bool
log_scope_is_enabled(struct log_scope *scope)
{
	if (!scope)
		return false;

	return !wl_list_empty(&scope->subscription_list);
}

void
log_subscription_complete(struct log_subscription *sub)
{
	if (sub->owner && sub->owner->complete)
		sub->owner->complete(sub->owner);
}

void
log_scope_complete(struct log_scope *scope)
{
	struct log_subscription *sub;

	if (!scope)
		return;

	wl_list_for_each(sub, &scope->subscription_list, source_link)
		log_subscription_complete(sub);
}

void
log_scope_write(struct log_scope *scope, const char *data, size_t len)
{
	struct log_subscription *sub;

	if (!scope)
		return;

	wl_list_for_each(sub, &scope->subscription_list, source_link)
		log_subscription_write(sub, data, len);
}

int
log_scope_vprintf(struct log_scope *scope, const char *fmt, va_list ap)
{
	static const char oom[] = "Out of memory";
	char *str;
	int len = 0;

	if (!log_scope_is_enabled(scope))
		return len;

	len = vasprintf(&str, fmt, ap);
	if (len >= 0) {
		log_scope_write(scope, str, len);
		free(str);
	} else {
		log_scope_write(scope, oom, sizeof oom - 1);
	}

	return len;
}

int
log_scope_printf(struct log_scope *scope, const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = log_scope_vprintf(scope, fmt, ap);
	va_end(ap);

	return len;
}

void
log_subscription_printf(struct log_subscription *sub, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_subscription_vprintf(sub, fmt, ap);
	va_end(ap);
}

char *
log_scope_timestamp(struct log_scope *scope, char *buf, size_t len)
{
	struct timeval tv;
	struct tm *bdt;
	char string[128];
	size_t ret = 0;

	gettimeofday(&tv, NULL);

	bdt = localtime(&tv.tv_sec);
	if (bdt)
		ret = strftime(string, sizeof string,
			       "%Y-%m-%d %H:%M:%S", bdt);

	if (ret > 0) {
		snprintf(buf, len, "[%s.%03ld][%s]", string,
			 tv.tv_usec / 1000,
			 (scope) ? scope->name : "no scope");
	} else {
		snprintf(buf, len, "[?][%s]",
			 (scope) ? scope->name : "no scope");
	}

	return buf;
}

char *
log_timestamp(char *buf, size_t len, int *cached_tm_mday)
{
	struct timeval tv;
	struct tm *brokendown_time;
	char datestr[128];
	char timestr[128];

	gettimeofday(&tv, NULL);

	brokendown_time = localtime(&tv.tv_sec);
	if (brokendown_time == NULL) {
		snprintf(buf, len, "%s", "[(NULL)localtime] ");
		return buf;
	}

	memset(datestr, 0, sizeof(datestr));
	if (cached_tm_mday && brokendown_time->tm_mday != *cached_tm_mday) {
		strftime(datestr, sizeof(datestr), "Date: %Y-%m-%d %Z\n",
			brokendown_time);
		*cached_tm_mday = brokendown_time->tm_mday;
	}

	strftime(timestr, sizeof(timestr), "%H:%M:%S", brokendown_time);
	/* if datestr is empty it prints only timestr */
	snprintf(buf, len, "%s[%s.%03li]", datestr,
		 timestr, (tv.tv_usec / 1000));

	return buf;
}


// void
// log_subscriber_release(struct log_subscriber *subscriber)
// {
// 	struct log_subscription *sub, *sub_tmp;

// 	wl_list_for_each_safe(sub, sub_tmp, &subscriber->subscription_list, owner_link)
// 		weston_log_subscription_destroy(sub);
// }

// WL_EXPORT void
// weston_log_subscriber_destroy(struct weston_log_subscriber *subscriber)
// {
// 	subscriber->destroy(subscriber);
// }


void
log_subscribe(struct log_context *log_ctx,
	      struct log_subscriber *subscriber,
	      const char *scope_name)
{
	assert(log_ctx);
	assert(subscriber);
	assert(scope_name);

	struct log_scope *scope;

	scope = log_get_scope(log_ctx, scope_name);
	if (scope)
		log_subscription_create(subscriber, scope);
	else
		/*
		 * if we don't have already as scope for it, add it to pending
		 * subscription list
		 */
		log_subscription_create_pending(subscriber, scope_name, log_ctx);
}

// WL_EXPORT struct weston_log_subscription *
// weston_log_subscription_iterate(struct weston_log_scope *scope,
// 				struct weston_log_subscription *sub_iter)
// {
// 	struct wl_list *list = &scope->subscription_list;
// 	struct wl_list *node;

// 	/* go to the next item in the list or if not set starts with the head */
// 	if (sub_iter)
// 		node = sub_iter->source_link.next;
// 	else
// 		node = list->next;

// 	assert(node);
// 	assert(!sub_iter || node != &sub_iter->source_link);

// 	/* if we're at the end */
// 	if (node == list)
// 		return NULL;

// 	return container_of(node, struct weston_log_subscription, source_link);
// }

/*****************************************************************************/

typedef int (*log_func_t)(const char *fmt, va_list ap);

static int
default_log_handler(const char *fmt, va_list ap);

static log_func_t log_handler = default_log_handler;

static log_func_t log_continue_handler = default_log_handler;

static int
default_log_handler(const char *fmt, va_list ap)
{
	fprintf(stderr, "weston_log_set_handler() must be called before using of weston_log().\n");
	abort();
}

void
weston_log_set_handler(log_func_t log, log_func_t cont)
{
	log_handler = log;
	log_continue_handler = cont;
}

static int
weston_vlog(const char *fmt, va_list ap)
{
	return log_handler(fmt, ap);
}

int
weston_log(const char *fmt, ...)
{
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = weston_vlog(fmt, argp);
	va_end(argp);

	return l;
}

static int
weston_vlog_continue(const char *fmt, va_list argp)
{
	return log_continue_handler(fmt, argp);
}

int
weston_log_continue(const char *fmt, ...)
{
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = weston_vlog_continue(fmt, argp);
	va_end(argp);

	return l;
}