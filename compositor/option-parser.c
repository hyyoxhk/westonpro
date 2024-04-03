// SPDX-License-Identifier: MIT
/*
 * Copyright © 2012 Kristian Høgsberg
 * 
 * Ported from Weston-11.0 - shared/option-parser.c
 */

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <config-parser.h>

#include "shared/util.h"

static bool
handle_option(const struct option *option, char *value)
{
	char* p;

	switch (option->type) {
	case OPTION_INTEGER:
		if (!safe_strtoint(value, option->data))
			return false;
		return true;
	case OPTION_UNSIGNED_INTEGER:
		errno = 0;
		* (uint32_t *) option->data = strtoul(value, &p, 10);
		if (errno != 0 || p == value || *p != '\0')
			return false;
		return true;
	case OPTION_STRING:
		* (char **) option->data = strdup(value);
		return true;
	default:
		assert(0);
		return false;
	}
}

static bool
long_option(const struct option *options, int count, char *arg)
{
	int k, len;

	for (k = 0; k < count; k++) {
		if (!options[k].name)
			continue;

		len = strlen(options[k].name);
		if (strncmp(options[k].name, arg + 2, len) != 0)
			continue;

		if (options[k].type == OPTION_BOOLEAN) {
			if (!arg[len + 2]) {
				* (bool *) options[k].data = true;

				return true;
			}
		} else if (arg[len+2] == '=') {
			return handle_option(options + k, arg + len + 3);
		}
	}

	return false;
}

static bool
long_option_with_arg(const struct option *options, int count, char *arg,
		     char *param)
{
	int k, len;

	for (k = 0; k < count; k++) {
		if (!options[k].name)
			continue;

		len = strlen(options[k].name);
		if (strncmp(options[k].name, arg + 2, len) != 0)
			continue;

		/* Since long_option() should handle all booleans, we should
		 * never reach this
		 */
		assert(options[k].type != OPTION_BOOLEAN);

		return handle_option(options + k, param);
	}

	return false;
}

static bool
short_option(const struct option *options, int count, char *arg)
{
	int k;

	if (!arg[1])
		return false;

	for (k = 0; k < count; k++) {
		if (options[k].short_name != arg[1])
			continue;

		if (options[k].type == OPTION_BOOLEAN) {
			if (!arg[2]) {
				* (bool *) options[k].data = true;

				return true;
			}
		} else if (arg[2]) {
			return handle_option(options + k, arg + 2);
		} else {
			return false;
		}
	}

	return false;
}

static bool
short_option_with_arg(const struct option *options, int count, char *arg, char *param)
{
	int k;

	if (!arg[1])
		return false;

	for (k = 0; k < count; k++) {
		if (options[k].short_name != arg[1])
			continue;

		if (options[k].type == OPTION_BOOLEAN)
			continue;

		return handle_option(options + k, param);
	}

	return false;
}

int
parse_options(const struct option *options, int count, int *argc, char *argv[])
{
	int i, j;

	for (i = 1, j = 1; i < *argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				/* Long option, e.g. --foo or --foo=bar */
				if (long_option(options, count, argv[i]))
					continue;

				/* ...also handle --foo bar */
				if (i + 1 < *argc &&
				    long_option_with_arg(options, count,
							 argv[i], argv[i+1])) {
					i++;
					continue;
				}
			} else {
				/* Short option, e.g -f or -f42 */
				if (short_option(options, count, argv[i]))
					continue;

				/* ...also handle -f 42 */
				if (i+1 < *argc &&
				    short_option_with_arg(options, count, argv[i], argv[i+1])) {
					i++;
					continue;
				}
			}
		}
		argv[j++] = argv[i];
	}
	argv[j] = NULL;
	*argc = j;

	return j;
}
