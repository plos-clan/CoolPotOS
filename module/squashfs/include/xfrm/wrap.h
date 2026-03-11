/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * wrap.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef XFRM_WRAP_H
#define XFRM_WRAP_H

#include "sqfs/predef.h"
#include "xfrm/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an input stream that transparently decodes data.
 *
 * @memberof sqfs_istream_t
 *
 * This function creates an input stream that wraps an underlying input stream
 * that is encoded/compressed and transparently decodes the data when reading
 * from it.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param xfrm The transformation stream to use.
 *
 * @return A pointer to an input stream on success, NULL on failure.
 */
SQFS_INTERNAL sqfs_istream_t *istream_xfrm_create(sqfs_istream_t *strm,
						  xfrm_stream_t *xfrm);

/**
 * @brief Create an output stream that transparently encodes data.
 *
 * @memberof sqfs_ostream_t
 *
 * This function creates an output stream that transparently encodes
 * (e.g. compresses) all data appended to it and writes it to an
 * underlying, wrapped output stream.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param xfrm The transformation stream to use.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL sqfs_ostream_t *ostream_xfrm_create(sqfs_ostream_t *strm,
						  xfrm_stream_t *xfrm);

#ifdef __cplusplus
}
#endif

#endif /* XFRM_WRAP_H */
