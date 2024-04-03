// SPDX-License-Identifier: MIT
/*
 * Copyright © 2012 Kristian Høgsberg
 * 
 * Ported from Weston-11.0 - include/libweston/config-parser.h
 */

#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#define CONFIG_FILE_ENV_VAR "WESTON_CONFIG_FILE"

enum option_type {
	OPTION_INTEGER,
	OPTION_UNSIGNED_INTEGER,
	OPTION_STRING,
	OPTION_BOOLEAN
};

struct option {
	enum option_type type;
	const char *name;
	char short_name;
	void *data;
};

int
parse_options(const struct option *options, int count, int *argc, char *argv[]);

struct config_section;
struct config;

struct config_section *
config_get_section(struct config *config, const char *section, const char *key,
                   const char *value);
int
config_section_get_int(struct config_section *section, const char *key,
		       int32_t *value, int32_t default_value);
int
config_section_get_uint(struct config_section *section, const char *key,
			uint32_t *value, uint32_t default_value);
int
config_section_get_color(struct config_section *section, const char *key,
			 uint32_t *color, uint32_t default_color);
int
config_section_get_double(struct config_section *section, const char *key,
			  double *value, double default_value);
int
config_section_get_string(struct config_section *section, const char *key,
			  char **value, const char *default_value);
int
config_section_get_bool(struct config_section *section, const char *key,
			bool *value, bool default_value);

const char *
config_get_name_from_env(void);

struct config *
config_parse(const char *name);

const char *
config_get_full_path(struct config *config);

int
config_next_section(struct config *config, struct config_section **section,
		    const char **name);
void
config_destroy(struct config *config);

#endif /* CONFIGPARSER_H */
