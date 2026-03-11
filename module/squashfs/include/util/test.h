/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * test.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TEST_H
#define TEST_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

#if defined(__GNUC__) || defined(__clang__)
#	define ATTRIB_UNUSED __attribute__ ((unused))
#else
#	define ATTRIB_UNUSED
#endif

static ATTRIB_UNUSED FILE *test_open_read(const char *path)
{
	FILE *fp = fopen(path, "rb");

	if (fp == NULL) {
		perror(path);
		abort();
	}

	return fp;
}

static ATTRIB_UNUSED void test_assert(const char *expr, int value,
				      int linenum)
{
	if (value == 0) {
		fprintf(stderr, "%d: '%s' is false!\n", linenum, expr);
		abort();
	}
}

static ATTRIB_UNUSED void test_str_equal(const char *expr, const char *value,
					 const char *ref, int linenum)
{
	if (strcmp(value, ref) != 0) {
		fprintf(stderr,
			"%d: '%s' should be '%s', but actually is '%s'!\n",
			linenum, expr, ref, value);
		abort();
	}
}

static ATTRIB_UNUSED void test_not_null(const void *value, const char *expr,
					int linenum)
{
	if (value == NULL) {
		fprintf(stderr, "%d: '%s' should not be NULL, but is!\n",
			linenum, expr);
		abort();
	}
}

static ATTRIB_UNUSED void test_null(const void *value, const char *expr,
				    int linenum)
{
	if (value != NULL) {
		fprintf(stderr, "%d: '%s' should be NULL, but is!\n",
			linenum, expr);
		abort();
	}
}

static ATTRIB_UNUSED void test_equal_ul(const char *lhs, const char *rhs,
					unsigned long lval, unsigned long rval,
					int linenum)
{
	if (lval != rval) {
		fprintf(stderr, "%d: %s (%lu) does not equal %s (%lu)!\n",
			linenum, lhs, lval, rhs, rval);
		abort();
	}
}

static ATTRIB_UNUSED void test_equal_sl(const char *lhs, const char *rhs,
					long lval, long rval, int linenum)
{
	if (lval != rval) {
		fprintf(stderr, "%d: %s (%ld) does not equal %s (%ld)!\n",
			linenum, lhs, lval, rhs, rval);
		abort();
	}
}

static ATTRIB_UNUSED void test_lt_ul(const char *lhs, const char *rhs,
				     unsigned long lval, unsigned long rval,
				     int linenum)
{
	if (lval >= rval) {
		fprintf(stderr, "%d: %s (%lu) is not less than %s (%lu)!\n",
			linenum, lhs, lval, rhs, rval);
		abort();
	}
}

#define TEST_STR_EQUAL(str, ref) test_str_equal(#str, str, ref, __LINE__)

#define TEST_NULL(expr) test_null(expr, #expr, __LINE__)

#define TEST_NOT_NULL(expr) test_not_null(expr, #expr, __LINE__)

#define TEST_EQUAL_I(a, b) test_equal_sl(#a, #b, a, b, __LINE__)

#define TEST_EQUAL_UI(a, b) test_equal_ul(#a, #b, a, b, __LINE__)

#define TEST_LESS_THAN_UI(a, b) test_lt_ul(#a, #b, a, b, __LINE__)

#define TEST_ASSERT(expr) test_assert(#expr, (expr), __LINE__)

#endif /* TEST_H */
