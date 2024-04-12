// SPDX-License-Identifier: MIT
/*
 * Copyright © 2019 Collabora Ltd.
 * 
 * Ported from Weston-11.0 - libweston/weston-log-file.c
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <wayland-util.h>

#include <log.h>
#include "log-internal.h"
#include "shared/util.h"

struct debug_log_file {
	struct log_subscriber base;
	FILE *file;
};

static void
log_file_write(struct log_subscriber *sub, const char *data, size_t len)
{
	struct debug_log_file *stream = wl_container_of(sub, stream, base);
	fwrite(data, len, 1, stream->file);
}

static void
log_subscriber_destroy_log(struct log_subscriber *sub)
{
	struct debug_log_file *file = wl_container_of(sub, file, base);
	free(file);
}

struct log_subscriber *
log_subscriber_create_log(FILE *dump_to)
{
	struct debug_log_file *file = zalloc(sizeof(*file));

	if (!file)
		return NULL;

	if (dump_to)
		file->file = dump_to;
	else
		file->file = stderr;


	file->base.write = log_file_write;
	file->base.destroy = log_subscriber_destroy_log;
	file->base.destroy_subscription = NULL;
	file->base.complete = NULL;

	return &file->base;
}