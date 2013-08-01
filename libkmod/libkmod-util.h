#pragma once
#include "macro.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

char *privkm_getline_wrapped(FILE *fp, unsigned int *linenum) __attribute__((nonnull(1)));
ssize_t privkm_read_str_safe(int fd, char *buf, size_t buflen) _must_check_ __attribute__((nonnull(2)));
ssize_t privkm_write_str_safe(int fd, const char *buf, size_t buflen) __attribute__((nonnull(2)));
char *privkm_path_make_absolute_cwd(const char *p) _must_check_ __attribute__((nonnull(1)));
int privkm_mkdir_p(const char *path, int len, mode_t mode);
int privkm_mkdir_parents(const char *path, mode_t mode);

#define streq(a, b) (strcmp((a), (b)) == 0)
#define strstartswith(a, b) (strncmp(a, b, strlen(b)) == 0)
#define get_unaligned(ptr)			\
({						\
	struct __attribute__((packed)) {	\
		typeof(*(ptr)) __v;		\
	} *__p = (typeof(__p)) (ptr);		\
	__p->__v;				\
})

#define put_unaligned(val, ptr)		\
do {						\
	struct __attribute__((packed)) {	\
		typeof(*(ptr)) __v;		\
	} *__p = (typeof(__p)) (ptr);		\
	__p->__v = (val);			\
} while(0)

static inline void *memdup(const void *p, size_t n) 
{
	void *r = malloc(n);

	if (r == NULL)
		return NULL;

	return memcpy(r, p, n);
}

static inline int alias_normalize(const char *alias, char buf[PATH_MAX], size_t *len)
{
	size_t s;

	for (s = 0; s < PATH_MAX - 1; s++) {
		const char c = alias[s];
		switch (c) {
		case '-':
			buf[s] = '_';
			break;
		case ']':
			return -EINVAL;
		case '[':
			while (alias[s] != ']' && alias[s] != '\0') {
				buf[s] = alias[s];
				s++;
			}

			if (alias[s] != ']')
				return -EINVAL;

			buf[s] = alias[s];
			break;
		case '\0':
			goto finish;
		default:
			buf[s] = c;
		}
	}

finish:
	buf[s] = '\0';

	if (len)
		*len = s;

	return 0;
}

static inline char *modname_normalize(const char *modname, char buf[PATH_MAX],
								size_t *len)
{
	size_t s;

	for (s = 0; s < PATH_MAX - 1; s++) {
		const char c = modname[s];
		if (c == '-')
			buf[s] = '_';
		else if (c == '\0' || c == '.')
			break;
		else
			buf[s] = c;
	}

	buf[s] = '\0';

	if (len)
		*len = s;

	return buf;
}

static inline char *path_to_modname(const char *path, char buf[PATH_MAX], size_t *len)
{
	char *modname;

	modname = basename(path);
	if (modname == NULL || modname[0] == '\0')
		return NULL;

	return modname_normalize(modname, buf, len);
}


#define USEC_PER_SEC  1000000ULL
#define NSEC_PER_USEC 1000ULL
static inline unsigned long long ts_usec(const struct timespec *ts)
{
	return (unsigned long long) ts->tv_sec * USEC_PER_SEC +
	       (unsigned long long) ts->tv_nsec / NSEC_PER_USEC;
}

static inline unsigned long long stat_mstamp(const struct stat *st)
{
#ifdef HAVE_STRUCT_STAT_ST_MTIM
	return ts_usec(&st->st_mtim);
#else
	return (unsigned long long) st->st_mtime;
#endif
}
