// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <wlr/util/log.h>

#include <weston-pro.h>
#include <log.h>

#include <config-parser.h>

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
handle_primary_client_destroyed(struct wl_listener *listener, void *data)
{
	struct wl_client *client = data;

	weston_log("Primary client died.  Closing...\n");

	wl_display_terminate(wl_client_get_display(client));
}

static int
create_listening_socket(struct wl_display *display, const char *socket_name)
{
	char name_candidate[32];

	if (socket_name) {
		if (wl_display_add_socket(display, socket_name)) {
			weston_log("fatal: failed to add socket: %s\n",
				   strerror(errno));
			return -1;
		}

		setenv("WAYLAND_DISPLAY", socket_name, 1);
		return 0;
	} else {
		for (int i = 1; i <= 32; i++) {
			sprintf(name_candidate, "wayland-%d", i);
			if (wl_display_add_socket(display, name_candidate) >= 0) {
				setenv("WAYLAND_DISPLAY", name_candidate, 1);
				return 0;
			}
		}
		weston_log("fatal: failed to add socket: %s\n",
			   strerror(errno));
		return -1;
	}
}

static size_t
module_path_from_env(const char *name, char *path, size_t path_len)
{
	const char *mapping = getenv("WESTON_MODULE_MAP");
	const int name_len = strlen(name);
	const char *end;

	if (!mapping)
		return 0;

	end = mapping + strlen(mapping);
	while (mapping < end && *mapping) {
		const char *filename, *next;

		/* early out: impossibly short string */
		if (end - mapping < name_len + 1)
			return 0;

		filename = &mapping[name_len + 1];
		next = strchrnul(mapping, ';');

		if (strncmp(mapping, name, name_len) == 0 &&
		    mapping[name_len] == '=') {
			size_t file_len = next - filename; /* no trailing NUL */
			if (file_len >= path_len)
				return 0;
			strncpy(path, filename, file_len);
			path[file_len] = '\0';
			return file_len;
		}

		mapping = next + 1;
	}

	return 0;
}

static void *
load_module_entrypoint(const char *name, const char *entrypoint, const char *module_dir)
{
	char path[PATH_MAX];
	void *module, *init;
	size_t len;

	if (name == NULL)
		return NULL;

	if (name[0] != '/') {
		len = module_path_from_env(name, path, sizeof(path));
		if (len == 0)
			len = snprintf(path, sizeof(path), "%s/%s",
				       module_dir, name);
	} else {
		len = snprintf(path, sizeof(path), "%s", name);
	}

	/* snprintf returns the length of the string it would've written,
	 * _excluding_ the NUL byte. So even being equal to the size of
	 * our buffer is an error here. */
	if (len >= sizeof(path))
		return NULL;

	module = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
	if (module) {
		weston_log("Module '%s' already loaded\n", path);
	} else {
		weston_log("Loading module '%s'\n", path);
		module = dlopen(path, RTLD_NOW);
		if (!module) {
			weston_log("Failed to load module: %s\n", dlerror());
			return NULL;
		}
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		weston_log("Failed to lookup init function: %s\n", dlerror());
		dlclose(module);
		return NULL;
	}

	return init;
}

#define MODULEDIR "/usr/local/lib/x86_64-linux-gnu/weston-pro"

static int
load_module(struct server *server, const char *name, int *argc, char *argv[])
{
	int (*module_init)(struct server *server, int *argc, char *argv[]);

	module_init = load_module_entrypoint(name, "wet_module_init", MODULEDIR);
	if (!module_init)
		return -1;
	if (module_init(server, argc, argv) < 0)
		return -1;
	return 0;
}

static int
load_shell(struct server *server, const char *name, int *argc, char *argv[])
{
	int (*shell_init)(struct server *server, int *argc, char *argv[]);

	shell_init = load_module_entrypoint(name, "wet_shell_init", MODULEDIR);
	if (!shell_init)
		return -1;
	if (shell_init(server, argc, argv) < 0)
		return -1;
	return 0;
}

static int
load_modules(struct server *server, const char *modules, int *argc, char *argv[])
{
	const char *p, *end;
	char buffer[256];

	if (modules == NULL)
		return 0;

	p = modules;
	while (*p) {
		end = strchrnul(p, ',');
		snprintf(buffer, sizeof buffer, "%.*s", (int) (end - p), p);

		if (strstr(buffer, "xwayland.so")) {
			weston_log("fatal: Old Xwayland module loading detected: "
				   "Please use --xwayland command line option "
				   "or set xwayland=true in the [core] section "
				   "in weston.ini\n");
			return -1;
		} else {
			if (load_module(server, buffer, argc, argv) < 0)
				return -1;
		}

		p = end;
		while (*p == ',')
			p++;
	}

	return 0;
}

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
usage(int status)
{
	FILE *out = status ? stderr : stdout;
	fprintf(out,
"Usage: weston-pro [OPTIONS]\n\n"

"This is weston-pro version 1.0.0, the Wayland compositor based on the wlroots.\n"
"Weston-pro supports more protocols than Weston and is compatible with most of weston's features.\n"
"Copyright (C) 2024 He Yong <hyyoxhk@163.com>\n\n"

"Core options:\n\n"

"  --version\t\tPrint weston-pro version\n"
"  --shell=MODULE\tShell module, defaults to desktop-shell.so\n"
"  -S, --socket=NAME\tName of socket to listen on\n"
"  -i, --idle-time=SECS\tIdle time in seconds\n"
"  --modules\t\tLoad the comma-separated list of modules\n"
"  --log=FILE\t\tLog to the given file\n"
"  -c, --config=FILE\tConfig file to load, defaults to weston.ini\n"
"  --no-config\t\tDo not read weston.ini\n"
"  --wait-for-debugger\tRaise SIGSTOP on start-up\n"
"  --debug\t\tEnable debug extension\n"
"  -l, --logger-scopes=SCOPE\n"
"\t\t\tSpecify log scopes to subscribe to.\n"
"\t\t\tCan specify multiple scopes, each followed by comma\n"
"  -h, --help\t\tThis help message\n\n");
}

static int
load_configuration(struct config **config, int32_t noconfig,
		   const char *config_file)
{
	const char *file = "weston.ini";
	const char *full_path;

	*config = NULL;

	if (config_file)
		file = config_file;

	if (noconfig == 0)
		*config = config_parse(file);

	if (*config) {
		full_path = config_get_full_path(*config);

		weston_log("Using config file '%s'\n", full_path);
		setenv(CONFIG_FILE_ENV_VAR, full_path, 1);

		return 0;
	}

	if (config_file && noconfig == 0) {
		weston_log("fatal: error opening or reading config file"
			   " '%s'.\n", config_file);

		return -1;
	}

	weston_log("Starting with no config file.\n");
	setenv(CONFIG_FILE_ENV_VAR, "", 1);

	return 0;
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
	int ret = EXIT_FAILURE;
	char *cmdline;
	struct wl_display *display;
	struct wl_event_source *signals[2];
	struct wl_event_loop *loop;
	int i, fd;
	char *backend = NULL;
	char *shell = NULL;
	char *modules = NULL;
	char *option_modules = NULL;
	char *socket_name = NULL;
	char *config_file = NULL;
	int32_t idle_time = -1;
	int32_t help = 0;
	int32_t version = 0;
	int32_t noconfig = 0;
	int32_t debug_protocol = 0;
	struct server *server;
	struct config *config = NULL;
	struct config_section *section;
	struct wl_client *primary_client;
	struct wl_listener primary_client_destroyed;
	char *log = NULL;
	char *log_scopes = NULL;
	struct log_context *log_ctx = NULL;
	struct log_subscriber *logger = NULL;
	enum wlr_log_importance verbosity = WLR_ERROR;
	char *server_socket = NULL;
	sigset_t mask;
	struct sigaction action;

	bool wait_for_debugger = false;


	const struct option core_options[] = {
		{ OPTION_STRING, "shell", 0, &shell },
		{ OPTION_STRING, "socket", 'S', &socket_name },
		{ OPTION_INTEGER, "idle-time", 'i', &idle_time },
		{ OPTION_STRING, "modules", 0, &option_modules },
		{ OPTION_STRING, "log", 0, &log },
		{ OPTION_BOOLEAN, "help", 'h', &help },
		{ OPTION_BOOLEAN, "version", 0, &version },
		{ OPTION_BOOLEAN, "no-config", 0, &noconfig },
		{ OPTION_STRING, "config", 'c', &config_file },
		{ OPTION_BOOLEAN, "wait-for-debugger", 0, &wait_for_debugger },
		{ OPTION_BOOLEAN, "debug", 0, &debug_protocol },
		{ OPTION_STRING, "logger-scopes", 'l', &log_scopes },
	};

	os_fd_set_cloexec(fileno(stdin));

	cmdline = copy_command_line(argc, argv);
	parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);

	if (help) {
		free(cmdline);
		usage(EXIT_SUCCESS);

		return EXIT_SUCCESS;
	}

	if (version) {
		printf("weston-pro 1.0.0\n");
		free(cmdline);

		return EXIT_SUCCESS;
	}

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

	display = wl_display_create();
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

	if (load_configuration(&config, noconfig, config_file) < 0)
		goto out_signals;

	section = config_get_section(config, "core", NULL, NULL);

	if (!wait_for_debugger) {
		config_section_get_bool(section, "wait-for-debugger",
					&wait_for_debugger, false);
	}
	if (wait_for_debugger) {
		weston_log("Weston PID is %ld - "
			   "waiting for debugger, send SIGCONT to continue...\n",
			   (long)getpid());
		raise(SIGSTOP);
	}

	server = server_create(display, log_ctx);
	if (server == NULL) {
		weston_log("fatal: failed to create server\n");
		goto out_signals;
	}

	server_socket = getenv("WAYLAND_SERVER_SOCKET");
	if (server_socket) {
		weston_log("Running with single client\n");
		if (!safe_strtoint(server_socket, &fd))
			fd = -1;
	} else {
		fd = -1;
	}

	if (fd != -1) {
		primary_client = wl_client_create(display, fd);
		if (!primary_client) {
			weston_log("fatal: failed to add client: %s\n",
				   strerror(errno));
			goto out_signals;
		}
		primary_client_destroyed.notify =
			handle_primary_client_destroyed;
		wl_client_add_destroy_listener(primary_client,
					       &primary_client_destroyed);
	} else if (create_listening_socket(display, socket_name)) {
		goto out_signals;
	}

	if (!shell)
		config_section_get_string(section, "shell", &shell, "mydesktop-shell.so");

	if (load_shell(server, shell, &argc, argv) < 0)
		goto out_signals;

	config_section_get_string(section, "modules", &modules, "");
	if (load_modules(server, modules, &argc, argv) < 0)
		goto out_signals;

	if (load_modules(server, option_modules, &argc, argv) < 0)
		goto out_signals;

	wl_display_run(server->wl_display);

out_signals:
	for (i = ARRAY_LENGTH(signals) - 1; i >= 0; i--)
		if (signals[i])
			wl_event_source_remove(signals[i]);

	wl_display_destroy_clients(server->wl_display);
	wl_display_destroy(server->wl_display);

out_display:
	log_scope_destroy(log_scope);
	log_scope = NULL;
	log_scope_destroy(wlroots_scope);
	wlroots_scope = NULL;
	log_subscriber_destroy(logger);
	log_ctx_destroy(log_ctx);
	log_file_close();

	if (config)
		config_destroy(config);
	free(config_file);
	free(shell);
	free(socket_name);
	free(option_modules);
	free(log);
	free(log_scopes);
	free(modules);

	return ret;
}
