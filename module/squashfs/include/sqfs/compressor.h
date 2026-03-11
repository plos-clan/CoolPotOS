/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * compressor.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_COMPRESSOR_H
#define SQFS_COMPRESSOR_H

#include "sqfs/predef.h"
#include "sqfs/super.h"

/**
 * @file compressor.h
 *
 * @brief Contains declarations to everything related to data compression.
 */

/**
 * @interface sqfs_compressor_t
 *
 * @extends sqfs_object_t
 *
 * @brief Encapsultes a compressor with a simple interface to compress or
 *        extract chunks of data.
 */
struct sqfs_compressor_t {
	sqfs_object_t base;

	/**
	 * @brief Get the current compressor configuration.
	 *
	 * @param cmp A pointer to the compressor object.
	 * @param cfg A pointer to the object to write the configuration to.
	 */
	void (*get_configuration)(const sqfs_compressor_t *cmp,
				  sqfs_compressor_config_t *cfg);

	/**
	 * @brief Write compressor options to disk if non-default settings
	 *        have been used.
	 *
	 * The options are stored in an uncompressed meta data block directly
	 * after the super block.
	 *
	 * @param cmp A pointer to a compressor object.
	 * @param file A file to write to.
	 *
	 * @return The number of bytes written on success, 0 means default
	 *         settings are used. A negative value is an @ref SQFS_ERROR
	 *         identifier.
	 */
	int (*write_options)(sqfs_compressor_t *cmp, sqfs_file_t *file);

	/**
	 * @brief Read compressor options from disk.
	 *
	 * @param cmp A pointer to a compressor object.
	 * @param file A file to read from.
	 *
	 * @return Zero on success or an @ref SQFS_ERROR value.
	 */
	int (*read_options)(sqfs_compressor_t *cmp, sqfs_file_t *file);

	/**
	 * @brief Compress or uncompress a chunk of data.
	 *
	 * @param cmp A pointer to a compressor object.
	 * @param in A pointer to the input buffer to read from.
	 * @param size The number of bytes to read from the input and compress.
	 * @param out The destination buffer to write the result to.
	 * @param outsize The available space in the destination buffer.
	 *
	 * @return The number of bytes written to the buffer, a negative
	 *         value is an @ref SQFS_ERROR value. The value 0 means
	 *         the output buffer was too small when extracting or that
	 *         the result is larger than the input when compressing.
	 */
	sqfs_s32 (*do_block)(sqfs_compressor_t *cmp, const sqfs_u8 *in,
			     sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize);
};

/**
 * @struct sqfs_compressor_config_t
 *
 * @brief Configuration parameters for instantiating a compressor backend.
 *
 * The unused fields MUST be set to 0. The easiest way to do this is by always
 * clearing the struct using memset before setting anything, or using
 * @ref sqfs_compressor_config_init to set defaults and then modify the struct
 * from there.
 */
struct sqfs_compressor_config_t {
	/**
	 * @brief An @ref SQFS_COMPRESSOR identifier
	 */
	sqfs_u16 id;

	/**
	 * @brief A combination of @ref SQFS_COMP_FLAG flags.
	 */
	sqfs_u16 flags;

	/**
	 * @brief The intended data block size.
	 */
	sqfs_u32 block_size;

	/**
	 * @brief Compression level.
	 *
	 * Valid range and default value depend on the selected compressor.
	 */
	sqfs_u32 level;

	/**
	 * @brief Backend specific options for fine tuing.
	 */
	union {
		/**
		 * @brief Options for the zlib compressor.
		 */
		struct {
			/**
			 * @brief Deflate window size. Value between 8 and 15.
			 *
			 * Default is 15, i.e. 32k window.
			 */
			sqfs_u16 window_size;

			sqfs_u8 padd0[14];
		} gzip;

		/**
		 * @brief Options for the lzo compressor.
		 */
		struct {
			/**
			 * @brief Which variant of lzo should be used.
			 *
			 * An @ref SQFS_LZO_ALGORITHM value. Default is
			 * @ref SQFS_LZO1X_999, i.e. best compression.
			 */
			sqfs_u16 algorithm;

			sqfs_u8 padd0[14];
		} lzo;

		/**
		 * @brief Options for the LZMA and XZ (LZMA v2) compressors.
		 */
		struct {
			/**
			 * @brief LZMA dictionary size.
			 *
			 * This value must either be a power of two or the sum
			 * of two consecutive powers of two.
			 *
			 * Default is setting this to the same as the
			 * block size.
			 */
			sqfs_u32 dict_size;

			/**
			 * @brief Number of literal context bits.
			 *
			 * How many of the highest bits of the previous
			 * uncompressed byte to take into account when
			 * predicting the bits of the next byte.
			 *
			 * The sum lc + lp must be at MOST 4. Default value of
			 * lc is 3.
			 */
			sqfs_u8 lc;

			/**
			 * @brief Number of literal position bits.
			 *
			 * lp affects what kind of alignment in the uncompressed
			 * data is assumed when encoding bytes.
			 * See pb below for more information about alignment.
			 *
			 * The sum lc + lp must be at MOST 4. Default value of
			 * lp is 0.
			 */
			sqfs_u8 lp;

			/**
			 * @brief Number of position bits.
			 *
			 * This is the log2 of the assumed underlying alignment
			 * of the input data, i.e. pb=0 means single byte
			 * allignment, pb=1 means 16 bit, 2 means 32 bit.
			 *
			 * When the alignment is known, setting pb may reduce
			 * the file size.
			 *
			 * The default value is 2, i.e. 32 bit alignment.
			 */
			sqfs_u8 pb;

			sqfs_u8 padd0[9];
		} xz, lzma;

		sqfs_u64 padd0[2];
	} opt;
};

/**
 * @enum SQFS_COMP_FLAG
 *
 * @brief Flags for configuring the compressor.
 */
typedef enum {
	/**
	 * @brief For LZ4, set this to use high compression mode.
	 */
	SQFS_COMP_FLAG_LZ4_HC = 0x0001,
	SQFS_COMP_FLAG_LZ4_ALL = 0x0001,

	/**
	 * @brief Tell the LZMAv1 compressor to try the "extreme" option.
	 *
	 * The "extreme" option means that the compressor should try some
	 * strategies that it normally wouldn't, that may drastically increase
	 * compression time, but will not increase the decompressors memory
	 * consumption.
	 */
	SQFS_COMP_FLAG_LZMA_EXTREME = 0x0001,
	SQFS_COMP_FLAG_LZMA_ALL = 0x0001,

	/**
	 * @brief For XZ, set this to select the x86 BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_X86 = 0x0001,

	/**
	 * @brief For XZ, set this to select the PowerPC BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_POWERPC = 0x0002,

	/**
	 * @brief For XZ, set this to select the Itanium BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_IA64 = 0x0004,

	/**
	 * @brief For XZ, set this to select the ARM BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_ARM = 0x0008,

	/**
	 * @brief For XZ, set this to select the ARM Thumb BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_ARMTHUMB = 0x0010,

	/**
	 * @brief For XZ, set this to select the Sparc BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_SPARC = 0x0020,

	/**
	 * @brief Tell the XZ compressor to try the "extreme" option.
	 */
	SQFS_COMP_FLAG_XZ_EXTREME = 0x0100,

	SQFS_COMP_FLAG_XZ_ALL = 0x013F,

	/**
	 * @brief For zlib deflate, set this to try the default strategy.
	 */
	SQFS_COMP_FLAG_GZIP_DEFAULT = 0x0001,

	/**
	 * @brief For zlib deflate, set this to try the "filtered" strategy.
	 */
	SQFS_COMP_FLAG_GZIP_FILTERED = 0x0002,

	/**
	 * @brief For zlib deflate, set this to try the huffman only strategy.
	 */
	SQFS_COMP_FLAG_GZIP_HUFFMAN = 0x0004,

	/**
	 * @brief For zlib deflate, set this to try the RLE strategy.
	 */
	SQFS_COMP_FLAG_GZIP_RLE = 0x0008,

	/**
	 * @brief For zlib deflate, set this to try the fixed strategy.
	 */
	SQFS_COMP_FLAG_GZIP_FIXED = 0x0010,
	SQFS_COMP_FLAG_GZIP_ALL = 0x001F,

	/**
	 * @brief Set this if the compressor should actually extract
	 *        instead of compress data.
	 */
	SQFS_COMP_FLAG_UNCOMPRESS = 0x8000,
	SQFS_COMP_FLAG_GENERIC_ALL = 0x8000,
} SQFS_COMP_FLAG;

/**
 * @enum SQFS_LZO_ALGORITHM
 *
 * @brief The available LZO algorithms.
 */
typedef enum {
	SQFS_LZO1X_1 = 0,
	SQFS_LZO1X_1_11 = 1,
	SQFS_LZO1X_1_12 = 2,
	SQFS_LZO1X_1_15 = 3,
	SQFS_LZO1X_999	= 4,
} SQFS_LZO_ALGORITHM;

#define SQFS_GZIP_DEFAULT_LEVEL (9)
#define SQFS_GZIP_DEFAULT_WINDOW (15)

#define SQFS_LZO_DEFAULT_ALG SQFS_LZO1X_999
#define SQFS_LZO_DEFAULT_LEVEL (8)

#define SQFS_ZSTD_DEFAULT_LEVEL (15)

#define SQFS_GZIP_MIN_LEVEL (1)
#define SQFS_GZIP_MAX_LEVEL (9)

#define SQFS_LZO_MIN_LEVEL (0)
#define SQFS_LZO_MAX_LEVEL (9)

#define SQFS_ZSTD_MIN_LEVEL (1)
#define SQFS_ZSTD_MAX_LEVEL (22)

#define SQFS_GZIP_MIN_WINDOW (8)
#define SQFS_GZIP_MAX_WINDOW (15)

#define SQFS_XZ_MIN_LEVEL (0)
#define SQFS_XZ_MAX_LEVEL (9)
#define SQFS_XZ_DEFAULT_LEVEL (6)

#define SQFS_XZ_MIN_LC 0
#define SQFS_XZ_MAX_LC 4
#define SQFS_XZ_DEFAULT_LC 3

#define SQFS_XZ_MIN_LP 0
#define SQFS_XZ_MAX_LP 4
#define SQFS_XZ_DEFAULT_LP 0

#define SQFS_XZ_MIN_PB 0
#define SQFS_XZ_MAX_PB 4
#define SQFS_XZ_DEFAULT_PB 2

#define SQFS_LZMA_MIN_LEVEL (0)
#define SQFS_LZMA_MAX_LEVEL (9)
#define SQFS_LZMA_DEFAULT_LEVEL (5)

#define SQFS_LZMA_MIN_LC 0
#define SQFS_LZMA_MAX_LC 4
#define SQFS_LZMA_DEFAULT_LC 3

#define SQFS_LZMA_MIN_LP 0
#define SQFS_LZMA_MAX_LP 4
#define SQFS_LZMA_DEFAULT_LP 0

#define SQFS_LZMA_MIN_PB 0
#define SQFS_LZMA_MAX_PB 4
#define SQFS_LZMA_DEFAULT_PB 2

#define SQFS_LZMA_MIN_DICT_SIZE SQFS_META_BLOCK_SIZE
#define SQFS_LZMA_MAX_DICT_SIZE SQFS_MAX_BLOCK_SIZE

#define SQFS_XZ_MIN_DICT_SIZE SQFS_META_BLOCK_SIZE
#define SQFS_XZ_MAX_DICT_SIZE SQFS_MAX_BLOCK_SIZE

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a compressor configuration
 *
 * The detail configuration options are all initialized to the defaults for
 * the compressor in question.
 *
 * @param cfg A pointer to a compressor configuration to initialize
 * @param id The compressor id to set.
 * @param block_size The block size to set.
 * @param flags The compressor flags to set.
 *
 * @return Zero on success, an @ref SQFS_ERROR value if some of the options
 *         don't make sense (e.g. unknown flags are used).
 */
SQFS_API int sqfs_compressor_config_init(sqfs_compressor_config_t *cfg,
					 SQFS_COMPRESSOR id,
					 size_t block_size, sqfs_u16 flags);

/**
 * @brief Create an instance of a compressor implementation.
 *
 * If this function returns @ref SQFS_ERROR_UNSUPPORTED, it can mean that
 * either the compressor is not supported at all by the version of libsquashfs
 * you are using, or that the specific configuration that has been requested
 * is not supported (e.g. unknown flags, or the local version can only
 * uncompress, but not compress).
 *
 * @param cfg A pointer to a compressor configuration.
 * @param out Returns a pointer to the compressor on success.
 *
 * @return Zero on success, an SQFS_ERROR code on failure.
 */
SQFS_API int sqfs_compressor_create(const sqfs_compressor_config_t *cfg,
				    sqfs_compressor_t **out);

/**
 * @brief Get the name of a compressor backend from its ID.
 *
 * @param id An @ref SQFS_COMPRESSOR identifier.
 *
 * @return A string holding the name of the compressor, NULL if the compressor
 *         ID is not known.
 */
SQFS_API const char *sqfs_compressor_name_from_id(SQFS_COMPRESSOR id);

/**
 * @brief Get the compressor ID using just the name of the backend.
 *
 * @param name The name of the compressor backend.
 *
 * @return A positive, @ref SQFS_COMPRESSOR identifier on success
 *         or @ref SQFS_ERROR_UNSUPPORTED if the backend is unknown.
 */
SQFS_API int sqfs_compressor_id_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_COMPRESSOR_H */
