#pragma once

#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

/* For verbose debug spew. */
extern bool tracei_enabled;

void tracei(char *fmt, ...);

/* 3-value booleans.
 * Must be set before testing. */
typedef struct {
	enum {
		_bOOL_TRUE = 2,
		_bOOL_FALSE,
	} _internal;
} bool_t;

static inline bool b_test(bool_t x)
{
	if (x._internal == _bOOL_TRUE)
		return 1;
	else if (x._internal == _bOOL_FALSE)
		return 0;
	else
		assert(0);
}

static inline bool_t b_set(bool x)
{
	return (bool_t) { ._internal = x ? _bOOL_TRUE : _bOOL_FALSE };
}

static inline bool_t b_not(bool_t x)
{
	if (x._internal == _bOOL_TRUE)
		return (bool_t) { ._internal = _bOOL_FALSE };
	else if (x._internal == _bOOL_FALSE)
		return (bool_t) { ._internal = _bOOL_TRUE };
	else
		return (bool_t) { 0 };
}


