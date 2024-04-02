// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023 He Yong <hyyoxhk@163.com>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include <wlr/util/log.h>

#include <weston-pro.h>
#include <log.h>

#include "shared/os-compatibility.h"
#include "shared/util.h"

#define DEFAULT_FLIGHT_REC_SIZE (5 * 1024 * 1024)
#define DEFAULT_FLIGHT_REC_SCOPES "log,drm-backend"

static FILE *logfile = NULL;
static struct log_scope *log_scope;
static struct log_scope *protocol_scope;
static int cached_tm_mday = -1;

static bool colored = true;
static enum wlr_log_importance log_importance = WLR_ERROR;

static const char *verbosity_colors[] = {
	[WLR_SILENT] = "",
	[WLR_ERROR] = "\x1B[1;31m",
	[WLR_INFO] = "\x1B[1;34m",
	[WLR_DEBUG] = "\x1B[1;90m",
};

static const char *verbosity_headers[] = {
	[WLR_SILENT] = "",
	[WLR_ERROR] = "[ERROR]",
	[WLR_INFO] = "[INFO]",
	[WLR_DEBUG] = "[DEBUG]",
};

static void
custom_handler(enum wlr_log_importance verbosity, const char *fmt, va_list args)
{
	char timestr[512];

	if (verbosity > log_importance) {
		return;
	}

	log_scope_printf(log_scope, "%s ", log_timestamp(timestr, sizeof(timestr), &cached_tm_mday));

	unsigned c = (verbosity < WLR_LOG_IMPORTANCE_LAST) ? verbosity : WLR_LOG_IMPORTANCE_LAST - 1;

	if (colored && isatty(fileno(logfile))) {
		log_scope_printf(log_scope, "%s", verbosity_colors[c]);
	} else {
		log_scope_printf(log_scope, "%s ", verbosity_headers[c]);
	}

	log_scope_vprintf(log_scope, fmt, args);

	if (colored && isatty(fileno(logfile))) {
		log_scope_printf(log_scope, "\x1B[0m");
	}
	log_scope_printf(log_scope, "\n");
}

static bool
log_file_open(enum wlr_log_importance verbosity, const char *filename)
{
	wlr_log_init(verbosity, custom_handler);

	if (filename != NULL) {
		logfile = fopen(filename, "a");
		if (logfile) {
			os_fd_set_cloexec(fileno(logfile));
		} else {
			fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
			return false;
		}
	}

	if (logfile == NULL)
		logfile = stderr;
	else
		setvbuf(logfile, NULL, _IOLBF, 256);

	return true;
}

static void
log_file_close(void)
{
	if ((logfile != stderr) && (logfile != NULL))
		fclose(logfile);
	logfile = stderr;
}

static int
vlog(const char *fmt, va_list ap)
{
	const char *oom = "Out of memory";
	char timestr[128];
	int len = 0;
	char *str;

	if (log_scope_is_enabled(log_scope)) {
		int len_va;
		char *logtimestamp = log_timestamp(timestr, sizeof(timestr), &cached_tm_mday);
		len_va = vasprintf(&str, fmt, ap);
		if (len_va >= 0) {
			len = log_scope_printf(log_scope, "%s %s", logtimestamp, str);
			free(str);
		} else {
			len = log_scope_printf(log_scope, "%s %s", logtimestamp, oom);
		}
	}

	return len;
}

static int
vlog_continue(const char *fmt, va_list argp)
{
	return log_scope_vprintf(log_scope, fmt, argp);
}

static int
on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	//printf("caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

int main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	int ret = EXIT_FAILURE;
	struct wl_display *display;
	struct wl_event_source *signals[2];
	struct wl_event_loop *loop;
	int i;
	struct wet_server server = { 0 };
	sigset_t mask;
	char *log = NULL;
	char *log_scopes = NULL;
	char *flight_rec_scopes = NULL;
	struct log_context *log_ctx = NULL;
	struct log_subscriber *logger = NULL;
	struct log_subscriber *flight_rec = NULL;
	enum wlr_log_importance verbosity = WLR_ERROR;

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'd':
			verbosity = WLR_DEBUG;
			break;
		case 'V':
			verbosity = WLR_INFO;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	log_ctx = log_ctx_create();
	if (!log_ctx) {
		fprintf(stderr, "Failed to initialize weston debug framework.\n");
		return EXIT_FAILURE;
	}

	log_scope = log_ctx_add_log_scope(log_ctx, "log",
			"Weston-pro, Wlroots and Wayland log\n", NULL, NULL, NULL);

	if (!log_file_open(verbosity, log))
		return EXIT_FAILURE;

	// log_set_handler(vlog, vlog_continue);

	logger = log_subscriber_create_log(logfile);

	// if (!flight_rec_scopes)
	// 	flight_rec_scopes = DEFAULT_FLIGHT_REC_SCOPES;

	// if (flight_rec_scopes && strlen(flight_rec_scopes) > 0)
	// 	flight_rec = weston_log_subscriber_create_flight_rec(DEFAULT_FLIGHT_REC_SIZE);

	// weston_log_subscribe_to_scopes(log_ctx, logger, flight_rec,
	// 			       log_scopes, flight_rec_scopes);

	log_subscribe(log_ctx, logger, "log");

	server.wl_display = display = wl_display_create();
	if (display == NULL) {
		printf("fatal: failed to create display\n");
		goto out_display;
	}

	loop = wl_display_get_event_loop(display);
	signals[0] = wl_event_loop_add_signal(loop, SIGTERM, on_term_signal,
					      display);
	signals[1] = wl_event_loop_add_signal(loop, SIGINT, on_term_signal,
					      display);

	if (!signals[0] || !signals[1])
		goto out_signals;


	// TODO


	if (!server_init(&server))
		return -1;

	if (!server_start(&server))
		return -1;

	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}

	wl_display_run(server.wl_display);

	/* Once wl_display_run returns, we shut down the server. */
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);

out_signals:
	for (i = ARRAY_LENGTH(signals) - 1; i >= 0; i--)
		if (signals[i])
			wl_event_source_remove(signals[i]);
out_display:

	return ret;
}
