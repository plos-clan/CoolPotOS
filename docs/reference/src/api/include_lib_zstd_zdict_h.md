# include/lib/zstd/zdict.h

## `/*! ZDICT_trainFromBuffer(): * Train a dictionary from an array of samples. * Redirect towards ZDICT_optimizeTrainFromBuffer_fastCover() single-threaded, with d=8, steps=4, * f=20, and accel=1. * Samples must be stored concatenated in a single flat buffer `samplesBuffer`, * supplied with an array of sizes `samplesSizes`, providing the size of each sample, in order. * The resulting dictionary will be saved into `dictBuffer`. * @return: size of dictionary stored into `dictBuffer` (<= `dictBufferCapacity`) * or an error code, which can be tested with ZDICT_isError(). * Note: Dictionary training will fail if there are not enough samples to construct a * dictionary, or if most of the samples are too small (< 8 bytes being the lower limit). * If dictionary training fails, you should use zstd without a dictionary, as the dictionary * would've been ineffective anyways. If you believe your samples would benefit from a dictionary * please open an issue with details, and we can look into it. * Note: ZDICT_trainFromBuffer()'s memory usage is about 6 MB. * Tips: In general, a reasonable dictionary has a size of ~ 100 KB. * It's possible to select smaller or larger size, just by specifying `dictBufferCapacity`. * In general, it's recommended to provide a few thousands samples, though this can vary a lot. * It's recommended that total size of all samples be about ~x100 times the target size of dictionary. */ ZDICTLIB_API size_t ZDICT_trainFromBuffer(void *dictBuffer, size_t dictBufferCapacity, const void *samplesBuffer, const size_t *samplesSizes, unsigned nbSamples);`

****************************************************************************
Zstd dictionary builder

FAQ
===
Why should I use a dictionary?
------------------------------

Zstd can use dictionaries to improve compression ratio of small data.
Traditionally small files don't compress well because there is very little
repetition in a single sample, since it is small. But, if you are compressing
many similar files, like a bunch of JSON records that share the same
structure, you can train a dictionary on ahead of time on some samples of
these files. Then, zstd can use the dictionary to find repetitions that are
present across samples. This can vastly improve compression ratio.

When is a dictionary useful?
----------------------------

Dictionaries are useful when compressing many small files that are similar.
The larger a file is, the less benefit a dictionary will have. Generally,
we don't expect dictionary compression to be effective past 100KB. And the
smaller a file is, the more we would expect the dictionary to help.

How do I use a dictionary?
--------------------------

Simply pass the dictionary to the zstd compressor with
`ZSTD_CCtx_loadDictionary()`. The same dictionary must then be passed to
the decompressor, using `ZSTD_DCtx_loadDictionary()`. There are other
more advanced functions that allow selecting some options, see zstd.h for
complete documentation.

What is a zstd dictionary?
--------------------------

A zstd dictionary has two pieces: Its header, and its content. The header
contains a magic number, the dictionary ID, and entropy tables. These
entropy tables allow zstd to save on header costs in the compressed file,
which really matters for small data. The content is just bytes, which are
repeated content that is common across many samples.

What is a raw content dictionary?
---------------------------------

A raw content dictionary is just bytes. It doesn't have a zstd dictionary
header, a dictionary ID, or entropy tables. Any buffer is a valid raw
content dictionary.

How do I train a dictionary?
----------------------------

Gather samples from your use case. These samples should be similar to each
other. If you have several use cases, you could try to train one dictionary
per use case.

Pass those samples to `ZDICT_trainFromBuffer()` and that will train your
dictionary. There are a few advanced versions of this function, but this
is a great starting point. If you want to further tune your dictionary
you could try `ZDICT_optimizeTrainFromBuffer_cover()`. If that is too slow
you can try `ZDICT_optimizeTrainFromBuffer_fastCover()`.

If the dictionary training function fails, that is likely because you
either passed too few samples, or a dictionary would not be effective
for your data. Look at the messages that the dictionary trainer printed,
if it doesn't say too few samples, then a dictionary would not be effective.

How large should my dictionary be?
----------------------------------

A reasonable dictionary size, the `dictBufferCapacity`, is about 100KB.
The zstd CLI defaults to a 110KB dictionary. You likely don't need a
dictionary larger than that. But, most use cases can get away with a
smaller dictionary. The advanced dictionary builders can automatically
shrink the dictionary for you, and select a the smallest size that
doesn't hurt compression ratio too much. See the `shrinkDict` parameter.
A smaller dictionary can save memory, and potentially speed up
compression.

How many samples should I provide to the dictionary builder?
------------------------------------------------------------

We generally recommend passing ~100x the size of the dictionary
in samples. A few thousand should suffice. Having too few samples
can hurt the dictionaries effectiveness. Having more samples will
only improve the dictionaries effectiveness. But having too many
samples can slow down the dictionary builder.

How do I determine if a dictionary will be effective?
-----------------------------------------------------

Simply train a dictionary and try it out. You can use zstd's built in
benchmarking tool to test the dictionary effectiveness.

# Benchmark levels 1-3 without a dictionary
zstd -b1e3 -r /path/to/my/files
# Benchmark levels 1-3 with a dictionary
zstd -b1e3 -r /path/to/my/files -D /path/to/my/dictionary

When should I retrain a dictionary?
-----------------------------------

You should retrain a dictionary when its effectiveness drops. Dictionary
effectiveness drops as the data you are compressing changes. Generally, we do
expect dictionaries to "decay" over time, as your data changes, but the rate
at which they decay depends on your use case. Internally, we regularly
retrain dictionaries, and if the new dictionary performs significantly
better than the old dictionary, we will ship the new dictionary.

I have a raw content dictionary, how do I turn it into a zstd dictionary?
-------------------------------------------------------------------------

If you have a raw content dictionary, e.g. by manually constructing it, or
using a third-party dictionary builder, you can turn it into a zstd
dictionary by using `ZDICT_finalizeDictionary()`. You'll also have to
provide some samples of the data. It will add the zstd header to the
raw content, which contains a dictionary ID and entropy tables, which
will improve compression ratio, and allow zstd to write the dictionary ID
into the frame, if you so choose.

Do I have to use zstd's dictionary builder?
-------------------------------------------

No! You can construct dictionary content however you please, it is just
bytes. It will always be valid as a raw content dictionary. If you want
a zstd dictionary, which can improve compression ratio, use
`ZDICT_finalizeDictionary()`.

What is the attack surface of a zstd dictionary?
------------------------------------------------

Zstd is heavily fuzz tested, including loading fuzzed dictionaries, so
zstd should never crash, or access out-of-bounds memory no matter what
the dictionary is. However, if an attacker can control the dictionary
during decompression, they can cause zstd to generate arbitrary bytes,
just like if they controlled the compressed data.

****************************************************************************

---

## `ZDICTLIB_API size_t ZDICT_getDictHeaderSize( const void *dictBuffer, size_t dictSize);`

< extracts dictID; @return zero if error (not a valid dictionary) 

---

