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
#include <sys/utsname.h>
#include <sys/stat.h>

#include <wlr/util/log.h>

#include <weston-pro.h>
#include <log.h>

#include "shared/os-compatibility.h"
#include "shared/util.h"

#define DEFAULT_FLIGHT_REC_SIZE (5 * 1024 * 1024)
#define DEFAULT_FLIGHT_REC_SCOPES "log,drm-backend"

static FILE *logfile = NULL;
static struct log_scope *log_scope;
static struct log_scope *wlroots_scope;
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

	log_scope_printf(wlroots_scope, "%s ", log_timestamp(timestr, sizeof(timestr), &cached_tm_mday));

	unsigned c = (verbosity < WLR_LOG_IMPORTANCE_LAST) ? verbosity : WLR_LOG_IMPORTANCE_LAST - 1;

	if (colored && isatty(fileno(logfile))) {
		log_scope_printf(wlroots_scope, "%s", verbosity_colors[c]);
	} else {
		log_scope_printf(wlroots_scope, "%s ", verbosity_headers[c]);
	}

	log_scope_vprintf(wlroots_scope, fmt, args);

	if (colored && isatty(fileno(logfile))) {
		log_scope_printf(wlroots_scope, "\x1B[0m");
	}
	log_scope_printf(wlroots_scope, "\n");
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

static const char xdg_error_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR is not set.\n";

static const char xdg_wrong_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR\n"
	"is set to \"%s\", which is not a directory.\n";

static const char xdg_wrong_mode_message[] =
	"warning: XDG_RUNTIME_DIR \"%s\" is not configured\n"
	"correctly.  Unix access mode must be 0700 (current mode is %04o),\n"
	"and must be owned by the user UID %d (current owner is UID %d).\n";

static const char xdg_detail_message[] =
	"Refer to your distribution on how to get it, or\n"
	"http://www.freedesktop.org/wiki/Specifications/basedir-spec\n"
	"on how to implement it.\n";

static void
verify_xdg_runtime_dir(void)
{
	char *dir = getenv("XDG_RUNTIME_DIR");
	struct stat s;

	if (!dir) {
		weston_log(xdg_error_message);
		weston_log_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if (stat(dir, &s) || !S_ISDIR(s.st_mode)) {
		weston_log(xdg_wrong_message, dir);
		weston_log_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if ((s.st_mode & 0777) != 0700 || s.st_uid != getuid()) {
		weston_log(xdg_wrong_mode_message,
			   dir, s.st_mode & 0777, getuid(), s.st_uid);
		weston_log_continue(xdg_detail_message);
	}
}

static void
log_uname(void)
{
	struct utsname usys;

	uname(&usys);

	weston_log("OS: %s, %s, %s, %s\n", usys.sysname, usys.release,
						usys.version, usys.machine);
}

static char *
copy_command_line(int argc, char * const argv[])
{
	FILE *fp;
	char *str = NULL;
	size_t size = 0;
	int i;

	fp = open_memstream(&str, &size);
	if (!fp)
		return NULL;

	fprintf(fp, "%s", argv[0]);
	for (i = 1; i < argc; i++)
		fprintf(fp, " %s", argv[i]);
	fclose(fp);

	return str;
}

static void
sigint_helper(int sig)
{
	raise(SIGUSR2);
}

int main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	int ret = EXIT_FAILURE;
	char *cmdline;
	struct wl_display *display;
	struct wl_event_source *signals[2];
	struct wl_event_loop *loop;
	int i;
	struct wet_server server = { 0 };
	char *log = NULL;
	char *log_scopes = NULL;
	struct log_context *log_ctx = NULL;
	struct log_subscriber *logger = NULL;
	enum wlr_log_importance verbosity = WLR_ERROR;
	sigset_t mask;
	struct sigaction action;

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
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	cmdline = copy_command_line(argc, argv);

	log_ctx = log_ctx_create();
	if (!log_ctx) {
		fprintf(stderr, "Failed to initialize weston debug framework.\n");
		return EXIT_FAILURE;
	}

	log_scope = log_ctx_add_log_scope(log_ctx, "log", "Weston-pro log\n", NULL, NULL, NULL);

	wlroots_scope = log_ctx_add_log_scope(log_ctx, "wlroots", "Wlroots and Wayland log", NULL, NULL, NULL);

	if (!log_file_open(verbosity, log))
		return EXIT_FAILURE;

	log_set_handler(vlog, vlog_continue);

	logger = log_subscriber_create_log(logfile);

	log_subscribe(log_ctx, logger, "log");

	weston_log("Command line: %s\n", cmdline);
	free(cmdline);
	log_uname();

	verify_xdg_runtime_dir();

	server.wl_display = display = wl_display_create();
	if (display == NULL) {
		weston_log("fatal: failed to create display\n");
		goto out_display;
	}

	loop = wl_display_get_event_loop(display);
	signals[0] = wl_event_loop_add_signal(loop, SIGTERM, on_term_signal,
					      display);
	signals[1] = wl_event_loop_add_signal(loop, SIGUSR2, on_term_signal,
					      display);

	// wl_list_init(&wet.child_process_list);
	// signals[2] = wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler,
	// 				      &wet);

	/* When debugging weston, if use wl_event_loop_add_signal() to catch
	 * SIGINT, the debugger can't catch it, and attempting to stop
	 * weston from within the debugger results in weston exiting
	 * cleanly.
	 *
	 * Instead, use the sigaction() function, which sets up the signal
	 * in a way that gdb can successfully catch, but have the handler
	 * for SIGINT send SIGUSR2 (xwayland uses SIGUSR1), which we catch
	 * via wl_event_loop_add_signal().
	 */
	action.sa_handler = sigint_helper;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);
	if (!signals[0] || !signals[1])
		goto out_signals;

	/* Xwayland uses SIGUSR1 for communicating with weston. Since some
	   weston plugins may create additional threads, set up any necessary
	   signal blocking early so that these threads can inherit the settings
	   when created. */
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

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
	log_scope_destroy(log_scope);
	log_scope = NULL;
	log_scope_destroy(wlroots_scope);
	wlroots_scope = NULL;
	log_subscriber_destroy(logger);
	log_ctx_destroy(log_ctx);
	log_file_close();

	return ret;
}
