/*
 * Common
 */

#ifndef _COMMON_H_
#define	_COMMON_H_

#include <stdio.h>
#include <err.h>
#include <errno.h>

#define	ERR_EXIT(cond) \
	if (cond) { \
		err(EXIT_FAILURE, NULL); \
	}

#define	SIG_VAL 128
#define	SYNTAX_ERR 1

#endif // _COMMON_H_
