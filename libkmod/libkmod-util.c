/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 * Copyright (C) 2012  Lucas De Marchi <lucas.de.marchi@gmail.com>
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "libkmod.h"
#include "libkmod-internal.h"

/*
 * Read one logical line from a configuration file.
 *
 * Line endings may be escaped with backslashes, to form one logical line from
 * several physical lines.  No end of line character(s) are included in the
 * result.
 *
 * If linenum is not NULL, it is incremented by the number of physical lines
 * which have been read.
 */
char *privkm_getline_wrapped(FILE *fp, unsigned int *linenum)
{
	int size = 256;
	int i = 0;
	char *buf = malloc(size);

	if (buf == NULL)
		return NULL;

	for(;;) {
		int ch = getc_unlocked(fp);

		switch(ch) {
		case EOF:
			if (i == 0) {
				free(buf);
				return NULL;
			}
			/* else fall through */

		case '\n':
			if (linenum)
				(*linenum)++;
			if (i == size)
				buf = realloc(buf, size + 1);
			buf[i] = '\0';
			return buf;

		case '\\':
			ch = getc_unlocked(fp);

			if (ch == '\n') {
				if (linenum)
					(*linenum)++;
				continue;
			}
			/* else fall through */

		default:
			buf[i++] = ch;

			if (i == size) {
				size *= 2;
				buf = realloc(buf, size);
			}
		}
	}
}

ssize_t privkm_read_str_safe(int fd, char *buf, size_t buflen)
{
	size_t todo = buflen - 1;
	size_t done = 0;

	do {
		ssize_t r = read(fd, buf + done, todo);

		if (r == 0)
			break;
		else if (r > 0) {
			todo -= r;
			done += r;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
								errno == EINTR)
				continue;
			else
				return -errno;
		}
	} while (todo > 0);

	buf[done] = '\0';
	return done;
}

ssize_t privkm_write_str_safe(int fd, const char *buf, size_t buflen)
{
	size_t todo = buflen;
	size_t done = 0;

	do {
		ssize_t r = write(fd, buf + done, todo);

		if (r == 0)
			break;
		else if (r > 0) {
			todo -= r;
			done += r;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
								errno == EINTR)
				continue;
			else
				return -errno;
		}
	} while (todo > 0);

	return done;
}

static inline bool path_is_absolute(const char *p)
{
	assert(p != NULL);

	return p[0] == '/';
}

char *privkm_path_make_absolute_cwd(const char *p)
{
	char *cwd, *r;
	size_t plen;
	size_t cwdlen;

	if (path_is_absolute(p))
		return strdup(p);

	cwd = get_current_dir_name();
	if (cwd == NULL)
		return NULL;

	plen = strlen(p);
	cwdlen = strlen(cwd);

	/* cwd + '/' + p + '\0' */
	r = realloc(cwd, cwdlen + 1 + plen + 1);
	if (r == NULL) {
		free(cwd);
		return NULL;
	}

	r[cwdlen] = '/';
	memcpy(&r[cwdlen + 1], p, plen + 1);

	return r;
}

static inline int is_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st) >= 0)
		return S_ISDIR(st.st_mode);

	return -errno;
}

int privkm_mkdir_p(const char *path, int len, mode_t mode)
{
	char *start, *end;

	start = strndupa(path, len);
	end = start + len;

	/*
	 * scan backwards, replacing '/' with '\0' while the component doesn't
	 * exist
	 */
	for (;;) {
		int r = is_dir(start);
		if (r > 0) {
			end += strlen(end);

			if (end == start + len)
				return 0;

			/* end != start, since it would be caught on the first
			 * iteration */
			*end = '/';
			break;
		} else if (r == 0)
			return -ENOTDIR;

		if (end == start)
			break;

		*end = '\0';

		/* Find the next component, backwards, discarding extra '/'*/
		while (end > start && *end != '/')
			end--;

		while (end > start && *(end - 1) == '/')
			end--;
	}

	for (; end < start + len;) {
		if (mkdir(start, mode) < 0 && errno != EEXIST)
			return -errno;

		end += strlen(end);
		*end = '/';
	}

	return 0;
}

int privkm_mkdir_parents(const char *path, mode_t mode)
{
	char *end = strrchr(path, '/');

	/* no parent directories */
	if (end == NULL)
		return 0;

	return privkm_mkdir_p(path, end - path, mode);
}
