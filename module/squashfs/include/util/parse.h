/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * parse.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_PARSE_H
#define UTIL_PARSE_H

#include "sqfs/predef.h"

enum {
	ISTREAM_LINE_LTRIM = 0x01,
	ISTREAM_LINE_RTRIM = 0x02,
	ISTREAM_LINE_SKIP_EMPTY = 0x04,
};

enum {
	SPLIT_LINE_OK = 0,
	SPLIT_LINE_ALLOC = -1,
	SPLIT_LINE_UNMATCHED_QUOTE = -2,
	SPLIT_LINE_ESCAPE = -3,
};

typedef struct {
	size_t count;
	char *args[];
} split_line_t;

#ifdef __cplusplus
extern "C" {
#endif

SQFS_INTERNAL void ltrim(char *buffer);

SQFS_INTERNAL void rtrim(char *buffer);

SQFS_INTERNAL void trim(char *buffer);

/**
 * @brief Read a line of text from an input stream
 *
 * @memberof sqfs_istream_t
 *
 * The line returned is allocated using malloc and must subsequently be
 * freed when it is no longer needed. The line itself is always null-terminated
 * and never includes the line break characters (LF or CR-LF).
 *
 * If the flag @ref ISTREAM_LINE_LTRIM is set, leading white space characters
 * are removed. If the flag @ref ISTREAM_LINE_RTRIM is set, trailing white space
 * characters are remvoed.
 *
 * If the flag @ref ISTREAM_LINE_SKIP_EMPTY is set and a line is discovered to
 * be empty (after the optional trimming), the function discards the empty line
 * and retries. The given line_num pointer is used to increment the line
 * number.
 *
 * @param strm A pointer to an input stream.
 * @param out Returns a pointer to a line on success.
 * @param line_num This is incremented if lines are skipped.
 * @param flags A combination of flags controling the functions behaviour.
 *
 * @return Zero on success, a negative value on error, a positive value if
 *         end-of-file was reached without reading any data.
 */
SQFS_INTERNAL int istream_get_line(sqfs_istream_t *strm, char **out,
				   size_t *line_num, int flags);

/**
 * @brief Parse an unsigned decimal integer from a string
 *
 * The function expects to find at least one decimal digit, and stops parsing
 * if it finds something that is not a digit. If diff is NULL, it requires that
 * the entire string was consumed (either length exhausted or a null-byte was
 * found), otherwise it returns success.
 *
 * Altough numeric overflow is checked for while parsing, the result is only
 * tested against vmin and vmax, if vmin is less than vmax. Setting them to
 * the same value disables the range check.
 *
 * @param in A pointer to a string to parse
 * @param len The maximum number of characters to read
 * @param diff If not NULL, returns the number of characters successfully read
 * @param vmin A lower bound. If the parsed value is below this, return an error
 * @param vmax An upper bound. If the value is above this, return an error
 * @param out If not NULL, returns the result value
 *
 * @return Zero on success, @ref SQFS_ERROR_OVERFLOW on numeric overflow,
 *         @ref SQFS_ERROR_OUT_OF_BOUNDS if the range check failed,
 *         @ref SQFS_ERROR_CORRUPTED if the string is not a number.
 */
SQFS_INTERNAL int parse_uint(const char *in, size_t len, size_t *diff,
			     sqfs_u64 vmin, sqfs_u64 vmax, sqfs_u64 *out);

/**
 * @brief A variant of @ref parse_uint that can parse signed numbers
 *
 * The function internally uses @ref parse_uint, but allows an optional
 * sign prefix and flips the result if it is negative. Range checking
 * can also be performed using negative bounds.
 *
 * Arguments and return values are the same as for @ref parse_uint, but signed.
 */
SQFS_INTERNAL int parse_int(const char *in, size_t len, size_t *diff,
			    sqfs_s64 vmin, sqfs_s64 vmax, sqfs_s64 *out);

/**
 * @brief Same as @ref parse_uint, but expects octal instead of decimal
 */
SQFS_INTERNAL int parse_uint_oct(const char *in, size_t len, size_t *diff,
				 sqfs_u64 vmin, sqfs_u64 vmax, sqfs_u64 *out);

/**
 * @brief Split a line of special character separated tokens
 *
 * The underlying string is modified, replacing sequences of separator
 * characters with a single null byte and compacting the string. Every
 * occourance of a termianted string is recorded in the returned structure.
 *
 * @param line A modifyable buffer holding a line
 * @param len The maximum length of the string in the buffer to process
 * @param sep A string of valid separator caracaters
 * @param out Returns the token list, free this with free()
 *
 * @return Zero on success, a negative SPLIT_LINE_* error code on failure
 */
SQFS_INTERNAL int split_line(char *line, size_t len,
			     const char *sep, split_line_t **out);

/**
 * @brief Remove the first N components of a tokenized line
 *
 * @param sep A successfully split up line
 * @param count Number of components to remove from the front
 */
SQFS_INTERNAL void split_line_remove_front(split_line_t *sep, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_PARSE_H */
