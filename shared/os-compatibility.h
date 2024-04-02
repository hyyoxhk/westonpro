/*
 * Copyright © 2012 Collabora, Ltd.
 *
 * Ported from Weston-11.0 - shared/os-compatibility.h
 */

#ifndef OS_COMPATIBILITY_H
#define OS_COMPATIBILITY_H

#include "config.h"

#include <sys/types.h>

int
os_fd_clear_cloexec(int fd);

int
os_fd_set_cloexec(int fd);

int
os_socketpair_cloexec(int domain, int type, int protocol, int *sv);

int
os_epoll_create_cloexec(void);

int
os_create_anonymous_file(off_t size);

#ifndef HAVE_STRCHRNUL
char *
strchrnul(const char *s, int c);
#endif

struct ro_anonymous_file;

enum ro_anonymous_file_mapmode {
	RO_ANONYMOUS_FILE_MAPMODE_PRIVATE,
	RO_ANONYMOUS_FILE_MAPMODE_SHARED,
};

struct ro_anonymous_file *
os_ro_anonymous_file_create(size_t size,
			    const char *data);

void
os_ro_anonymous_file_destroy(struct ro_anonymous_file *file);

size_t
os_ro_anonymous_file_size(struct ro_anonymous_file *file);

int
os_ro_anonymous_file_get_fd(struct ro_anonymous_file *file,
			    enum ro_anonymous_file_mapmode mapmode);

int
os_ro_anonymous_file_put_fd(int fd);

#endif /* OS_COMPATIBILITY_H */
