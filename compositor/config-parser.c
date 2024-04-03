// SPDX-License-Identifier: MIT
/*
 * Copyright © 2012 Kristian Høgsberg
 * 
 * Ported from Weston-11.0 - shared/config-parser.c
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


#include <wayland-util.h>

#include <config-parser.h>

#include "shared/util.h"


struct config_entry {
	char *key;
	char *value;
	struct wl_list link;
};

struct config_section {
	char *name;
	struct wl_list entry_list;
	struct wl_list link;
};

struct config {
	struct wl_list section_list;
	char path[PATH_MAX];
};

static int
open_config_file(struct config *c, const char *name)
{
	const char *config_dir  = getenv("XDG_CONFIG_HOME");
	const char *home_dir	= getenv("HOME");
	const char *config_dirs = getenv("XDG_CONFIG_DIRS");
	const char *p, *next;
	int fd;

	if (name[0] == '/') {
		snprintf(c->path, sizeof c->path, "%s", name);
		return open(name, O_RDONLY | O_CLOEXEC);
	}

	/* Precedence is given to config files in the home directory,
	 * then to directories listed in XDG_CONFIG_DIRS. */

	/* $XDG_CONFIG_HOME */
	if (config_dir) {
		snprintf(c->path, sizeof c->path, "%s/%s", config_dir, name);
		fd = open(c->path, O_RDONLY | O_CLOEXEC);
		if (fd >= 0)
			return fd;
	}

	/* $HOME/.config */
	if (home_dir) {
		snprintf(c->path, sizeof c->path,
			 "%s/.config/%s", home_dir, name);
		fd = open(c->path, O_RDONLY | O_CLOEXEC);
		if (fd >= 0)
			return fd;
	}

	/* For each $XDG_CONFIG_DIRS: weston/<config_file> */
	if (!config_dirs)
		config_dirs = "/etc/xdg";  /* See XDG base dir spec. */

	for (p = config_dirs; *p != '\0'; p = next) {
		next = strchrnul(p, ':');
		snprintf(c->path, sizeof c->path,
			 "%.*s/weston/%s", (int)(next - p), p, name);
		fd = open(c->path, O_RDONLY | O_CLOEXEC);
		if (fd >= 0)
			return fd;

		if (*next == ':')
			next++;
	}

	return -1;
}

static struct config_entry *
config_section_get_entry(struct config_section *section, const char *key)
{
	struct config_entry *e;

	if (section == NULL)
		return NULL;
	wl_list_for_each(e, &section->entry_list, link)
		if (strcmp(e->key, key) == 0)
			return e;

	return NULL;
}

struct config_section *
config_get_section(struct config *config, const char *section, const char *key,
		   const char *value)
{
	struct config_section *s;
	struct config_entry *e;

	if (config == NULL)
		return NULL;
	wl_list_for_each(s, &config->section_list, link) {
		if (strcmp(s->name, section) != 0)
			continue;
		if (key == NULL)
			return s;
		e = config_section_get_entry(s, key);
		if (e && strcmp(e->value, value) == 0)
			return s;
	}

	return NULL;
}

int
config_section_get_int(struct config_section *section, const char *key,
		       int32_t *value, int32_t default_value)
{
	struct config_entry *entry;

	entry = config_section_get_entry(section, key);
	if (entry == NULL) {
		*value = default_value;
		errno = ENOENT;
		return -1;
	}

	if (!safe_strtoint(entry->value, value)) {
		*value = default_value;
		return -1;
	}

	return 0;
}

int
config_section_get_uint(struct config_section *section, const char *key,
			uint32_t *value, uint32_t default_value)
{
	long int ret;
	struct config_entry *entry;
	char *end;

	entry = config_section_get_entry(section, key);
	if (entry == NULL) {
		*value = default_value;
		errno = ENOENT;
		return -1;
	}

	errno = 0;
	ret = strtol(entry->value, &end, 0);
	if (errno != 0 || end == entry->value || *end != '\0') {
		*value = default_value;
		errno = EINVAL;
		return -1;
	}

	/* check range */
	if (ret < 0 || ret > INT_MAX) {
		*value = default_value;
		errno = ERANGE;
		return -1;
	}

	*value = ret;

	return 0;
}

int
config_section_get_color(struct config_section *section, const char *key,
			 uint32_t *color, uint32_t default_color)
{
	struct config_entry *entry;
	int len;
	char *end;

	entry = config_section_get_entry(section, key);
	if (entry == NULL) {
		*color = default_color;
		errno = ENOENT;
		return -1;
	}

	len = strlen(entry->value);
	if (len == 1 && entry->value[0] == '0') {
		*color = 0;
		return 0;
	} else if (len != 8 && len != 10) {
		*color = default_color;
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	*color = strtoul(entry->value, &end, 16);
	if (errno != 0 || end == entry->value || *end != '\0') {
		*color = default_color;
		errno = EINVAL;
		return -1;
	}

	return 0;
}

int
config_section_get_double(struct config_section *section, const char *key,
			  double *value, double default_value)
{
	struct config_entry *entry;
	char *end;

	entry = config_section_get_entry(section, key);
	if (entry == NULL) {
		*value = default_value;
		errno = ENOENT;
		return -1;
	}

	*value = strtod(entry->value, &end);
	if (*end != '\0') {
		*value = default_value;
		errno = EINVAL;
		return -1;
	}

	return 0;
}

int
config_section_get_string(struct config_section *section, const char *key,
			  char **value, const char *default_value)
{
	struct config_entry *entry;

	entry = config_section_get_entry(section, key);
	if (entry == NULL) {
		if (default_value)
			*value = strdup(default_value);
		else
			*value = NULL;
		errno = ENOENT;
		return -1;
	}

	*value = strdup(entry->value);

	return 0;
}

int
config_section_get_bool(struct config_section *section, const char *key,
			bool *value, bool default_value)
{
	struct config_entry *entry;

	entry = config_section_get_entry(section, key);
	if (entry == NULL) {
		*value = default_value;
		errno = ENOENT;
		return -1;
	}

	if (strcmp(entry->value, "false") == 0)
		*value = false;
	else if (strcmp(entry->value, "true") == 0)
		*value = true;
	else {
		*value = default_value;
		errno = EINVAL;
		return -1;
	}

	return 0;
}

const char *
config_get_name_from_env(void)
{
	const char *name;

	name = getenv(CONFIG_FILE_ENV_VAR);
	if (name)
		return name;

	return "weston.ini";
}

static struct config_section *
config_add_section(struct config *config, const char *name)
{
	struct config_section *section;

	section = zalloc(sizeof *section);
	if (section == NULL)
		return NULL;

	section->name = strdup(name);
	if (section->name == NULL) {
		free(section);
		return NULL;
	}

	wl_list_init(&section->entry_list);
	wl_list_insert(config->section_list.prev, &section->link);

	return section;
}

static struct config_entry *
section_add_entry(struct config_section *section, const char *key, const char *value)
{
	struct config_entry *entry;

	entry = zalloc(sizeof *entry);
	if (entry == NULL)
		return NULL;

	entry->key = strdup(key);
	if (entry->key == NULL) {
		free(entry);
		return NULL;
	}

	entry->value = strdup(value);
	if (entry->value == NULL) {
		free(entry->key);
		free(entry);
		return NULL;
	}

	wl_list_insert(section->entry_list.prev, &entry->link);

	return entry;
}

static bool
config_parse_internal(struct config *config, FILE *fp)
{
	struct config_section *section = NULL;
	char line[512], *p;
	int i;

	wl_list_init(&config->section_list);

	while (fgets(line, sizeof line, fp)) {
		switch (line[0]) {
		case '#':
		case '\n':
			continue;
		case '[':
			p = strchr(&line[1], ']');
			if (!p || p[1] != '\n') {
				fprintf(stderr, "malformed "
					"section header: %s\n", line);
				return false;
			}
			p[0] = '\0';
			section = config_add_section(config, &line[1]);
			continue;
		default:
			p = strchr(line, '=');
			if (!p || p == line || !section) {
				fprintf(stderr, "malformed "
					"config line: %s\n", line);
				return false;
			}

			p[0] = '\0';
			p++;
			while (isspace(*p))
				p++;
			i = strlen(p);
			while (i > 0 && isspace(p[i - 1])) {
				p[i - 1] = '\0';
				i--;
			}
			section_add_entry(section, line, p);
			continue;
		}
	}

	return true;
}

struct config *
config_parse(const char *name)
{
	FILE *fp;
	struct stat filestat;
	struct config *config;
	int fd;
	bool ret;

	config = zalloc(sizeof *config);
	if (config == NULL)
		return NULL;

	fd = open_config_file(config, name);
	if (fd == -1) {
		free(config);
		return NULL;
	}

	if (fstat(fd, &filestat) < 0 ||
	    !S_ISREG(filestat.st_mode)) {
		close(fd);
		free(config);
		return NULL;
	}

	fp = fdopen(fd, "r");
	if (fp == NULL) {
		close(fd);
		free(config);
		return NULL;
	}

	ret = config_parse_internal(config, fp);

	fclose(fp);

	if (!ret) {
		config_destroy(config);
		return NULL;
	}

	return config;
}

const char *
config_get_full_path(struct config *config)
{
	return config == NULL ? NULL : config->path;
}

int
config_next_section(struct config *config, struct config_section **section,
		    const char **name)
{
	if (config == NULL)
		return 0;

	if (*section == NULL)
		*section = wl_container_of(config->section_list.next,
					   (struct config_section *)NULL, link);
	else
		*section = wl_container_of((*section)->link.next,
					   (struct config_section *)NULL, link);

	if (&(*section)->link == &config->section_list)
		return 0;

	*name = (*section)->name;

	return 1;
}

void
config_destroy(struct config *config)
{
	struct config_section *s, *next_s;
	struct config_entry *e, *next_e;

	if (config == NULL)
		return;

	wl_list_for_each_safe(s, next_s, &config->section_list, link) {
		wl_list_for_each_safe(e, next_e, &s->entry_list, link) {
			free(e->key);
			free(e->value);
			free(e);
		}
		free(s->name);
		free(s);
	}

	free(config);
}

