# WIA file format description

This document describes the WIA disc image format, version 1.00, as implemented in wit v2.40a. A few notes about Dolphin's implementation of the format are also included, where Dolphin differs from wit. The unique features of WIA compared to older formats like GCZ are:

 - Support for the compression algorithms bzip2, LZMA, and LZMA2
 - Wii partition data is stored decrypted and without hashes, making it compressible
 
Like essentially all compressed GC/Wii disc image formats, WIA divides the data into blocks (called chunks in wit). Each chunk is compressed separately, making random access of compressed data possible.

The struct names and variable names below are taken directly from wit. Data in WIA files can be stored in any order unless otherwise noted. All integers are big endian unless otherwise noted. The type `sha1_hash_t` refers to an array of 20 bytes.

## `wia_file_head_t`

This struct is stored at offset 0x0 and is 0x48 bytes long. The wit source code says its format will never be changed.

A short note from the wit source code about how version numbers are encoded:

```
//-----------------------------------------------------
// Format of version number: AABBCCDD = A.BB | A.BB.CC
// If D != 0x00 && D != 0xff => append: 'beta' D
//-----------------------------------------------------
```

|Type and name|Description|
|--|--|
|`char magic[4]`|Always contains `"WIA\x1"`.|
|`u32 version`|The WIA format version.|
|`u32 version_compatible`|If the reading program supports the version of WIA indicated here, it can read the file. `version` can be higher than `version_compatible` (wit v2.40a sets the former to `0x01000000` and the latter to `0x00090000`).|
|`u32 disc_size`|The size of the `wia_disc_t` struct. wit v2.40a always includes the full 7 bytes of `compr_data` when writing this.|
|`sha1_hash_t disc_hash`|The SHA-1 hash of the `wia_disc_t` struct. The number of bytes to hash is determined by `disc_size`. For instance, you may have to hash all 7 bytes of `compr_data` regardless of what `compr_data_len` says.|
|`u64 iso_file_size`|The original size of the disc (or in other words, the size of the ISO file that has the same contents as this WIA file).|
|`u64 wia_file_size`|The size of this file.|
|`sha1_hash_t file_head_hash`|The SHA-1 hash of this struct, up to but not including `file_head_hash` itself.|

## `wia_disc_t`

This struct is stored at offset 0x48, immediately after `wia_file_head_t`.

|Type and name|Description|
|--|--|
|`u32 disc_type`|wit sets this to 0 for "unknown" (does this ever happen in practice?), 1 for GameCube discs, 2 for Wii discs.|
|`u32 compression`|0 for NONE, 1 for PURGE (see the `wia_segment_t` section), 2 for BZIP2, 3 for LZMA, 4 for LZMA2.
|`u32 compr_level`|The compression level used by the compressor. The possible values are compressor-specific. For informational purposes only.|
|`u32 chunk_size`|The size of the chunks that data is divided into. Must be a multiple of 2 MiB.|
|`u8 dhead[0x80]`|The first 0x80 bytes of the disc image.
|`u32 n_part`|The number of `wia_part_t` structs.|
|`u32 part_t_size`|The size of one `wia_part_t` struct. If this is smaller than `sizeof(wia_part_t)`, fill the missing bytes with `0x00`.|
|`u64 part_off`|The offset in the file where the `wia_part_t` structs are stored (uncompressed).|
|`sha1_hash_t part_hash`|The SHA-1 hash of the `wia_part_t` structs. The number of bytes to hash is determined by `n_part * part_t_size`.|
|`u32 n_raw_data`|The number of `wia_raw_data_t` structs.|
|`u64 raw_data_off`|The offset in the file where the `wia_raw_data_t` structs are stored (compressed).|
|`u32 raw_data_size`|The total compressed size of the `wia_raw_data_t` structs.|
|`u32 n_groups`|The number of `wia_group_t` structs.|
|`u64 group_off`|The offset in the file where the `wia_group_t` structs are stored (compressed).|
|`u32 group_size`|The total compressed size of the `wia_group_t` structs.|
|`u8 compr_data_len`|The number of used bytes in the `compr_data` array.|
|`u8 compr_data[7]`|Compressor specific data (see below).|

If the compression method is NONE, PURGE or BZIP2, `compr_data_len`is 0. If the compression method is LZMA or LZMA2, the compressor specific data is stored in the format used by the 7-Zip SDK. It needs to be converted if you are using e.g. liblzma.

For LZMA, the data is 5 bytes long. The first byte encodes the `lc`, `pb`, and `lp` parameters, and the four other bytes encode the dictionary size in little endian. The first byte can be decoded as follows (code from the 7-Zip SDK):

```
d = data[0];
if (d >= (9 * 5 * 5))
  return SZ_ERROR_UNSUPPORTED;

p->lc = d % 9;
d /= 9;
p->pb = d / 5;
p->lp = d % 5;
```

For LZMA2, the data consists of a single byte that encodes the dictionary size. It can be decoded as follows (code from the 7-Zip SDK):

```
#define LZMA2_DIC_SIZE_FROM_PROP(p) (((UInt32)2 | ((p) & 1)) << ((p) / 2 + 11))

if (prop > 40)
  return SZ_ERROR_UNSUPPORTED;
dicSize = (prop == 40) ? 0xFFFFFFFF : LZMA2_DIC_SIZE_FROM_PROP(prop);
```

Preset dictionaries are not used for any compression method.

## `wia_part_data_t`

|Type and name|Description|
|--|--|
|`u32 first_sector`|The sector on the disc at which this data starts. One sector is 32 KiB (or 31 KiB excluding hashes).|
|`u32 n_sectors`|The number of sectors on the disc covered by this struct. One sector is 32 KiB (or 31 KiB excluding hashes).|
|`u32 group_index`|The index of the first `wia_group_t` struct that points to the data covered by this struct. The other `wia_group_t` indices follow sequentially.|
|`u32 n_groups`|The number of `wia_group_t` structs used for this data.|

## `wia_part_t`

This struct is used for keeping track of Wii partition data that on the actual disc is encrypted and hashed. This does not include the unencrypted area at the beginning of partitions that contains the ticket, TMD, certificate chain, and H3 table. So for a typical game partition, `pd[0].first_sector * 0x8000` would be 0x0F820000, not 0x0F800000.

Wii partition data is stored decrypted and with hashes removed. For each 0x8000 bytes on the disc, 0x7C00 bytes are stored in the WIA file (prior to compression). If the hashes are desired, the reading program must first recalculate the hashes as done when creating a Wii disc image from scratch (see https://wiibrew.org/wiki/Wii_Disc), and must then apply the hash exceptions which are stored along with the data (see the `wia_except_list_t` section).

|Type and name|Description|
|--|--|
|`u8 part_key[16]`|The title key for this partition (128-bit AES), which can be used for re-encrypting the partition data. This key can be used directly, without decrypting it using the Wii common key.|
|`wia_part_data_t pd[2]`|To quote the wit source code: `segment 0 is small and defined for management data (boot .. fst). segment 1 takes the remaining data`. The point at which wit splits the two segments is the FST end offset rounded up to the next 2 MiB. Giving the first segment a size which is not a multiple of 2 MiB is likely a bad idea (unless the second segment has a size of 0).|

## `wia_raw_data_t`

This struct is used for keeping track of disc data that is not stored as `wia_part_t`. The data is stored as is (other than compression being applied).

The first `wia_raw_data_t` has `raw_data_off` set to 0x80 and  `raw_data_size` set to 0x4FF80, but despite this, it actually contains 0x50000 bytes of data. (However, the first 0x80 bytes should be read from `wia_disc_t` instead.) This should be handled by rounding the offset down to the previous multiple of 0x8000 (and adding the equivalent amount to the size so that the end offset stays the same), not by special casing the first `wia_raw_data_t`.

|Type and name|Description|
|--|--|
|`u64 raw_data_off`|The offset on the disc at which this data starts.|
|`u64 raw_data_size`|The number of bytes on the disc covered by this struct.|
|`u32 group_index`|The index of the first `wia_group_t` struct that points to the data covered by this struct. The other `wia_group_t` indices follow sequentially.|
|`u32 n_groups`|The number of `wia_group_t` structs used for this data.|

## `wia_group_t`

This struct points directly to the actual disc data, stored compressed. The data is interpreted differently depending on whether the `wia_group_t` is referenced by a `wia_part_data_t` or a `wia_raw_data_t` (see the `wia_part_t` section for details).

A `wia_group_t` normally contains `chunk_size` bytes of decompressed data (or `chunk_size / 0x8000 * 0x7C00` for Wii partition data when not counting hashes), not counting any `wia_except_list_t` structs. However, the last `wia_group_t` of a `wia_part_data_t` or `wia_raw_data_t` contains less data than that if `n_sectors * 0x8000` (for `wia_part_data_t`) or  `raw_data_size` (for `wia_raw_data_t`)  is not evenly divisible by `chunk_size`.

|Type and name|Description|
|--|--|
|`u32 data_off4`|The offset in the file where the compressed data is stored, divided by 4.|
|`u32 data_size`|The size of the compressed data, including any `wia_except_list_t` structs. 0 is a special case meaning that every byte of the decompressed data is `0x00` and the `wia_except_list_t` structs (if there are supposed to be any) contain 0 exceptions.|

## `wia_exception_t`

This struct represents a 20-byte difference between the recalculated hash data and the original hash data. (See also `wia_except_list_t` below.)

When recalculating hashes for a `wia_group_t` with a size which is not evenly divisible by 2 MiB (with the size of the hashes included), the missing bytes should be treated as zeroes for the purpose of hashing. (wit's writing code seems to act as if the reading code does not assume that these missing bytes are zero, but both wit's and Dolphin's reading code treat them as zero. Dolphin's writing code assumes that the reading code treats them as zero.)

wit's writing code only outputs `wia_exception_t` structs for mismatches in the actual hash data, not in the padding data (which normally only contains zeroes). Dolphin's writing code outputs `wia_exception_t` structs for both hash data and padding data. When Dolphin needs to write `wia_exception_t` structs for a padding area which is 32 bytes long, it writes one which covers the first 20 bytes of the padding area and one which covers the last 20 bytes of the padding area, generating 12 bytes of overlap between the `wia_exception_t` structs.

|Type and name|Description|
|--|--|
|`u16 offset`|The offset among the hashes. The offsets `0x0000`-`0x0400` here map to the offsets `0x0000`-`0x0400` in the full 2 MiB of data, the offsets `0x0400`-`0x0800` here map to the offsets `0x8000`-`0x8400` in the full 2 MiB of data, and so on. The offsets start over at 0 for each new `wia_except_list_t`.|
|`sha1_hash_t hash`|The hash that the automatically generated hash at the given offset needs to be replaced with. The replacement should happen after calculating all hashes for the current 2 MiB of data but before encrypting the hashes.|

## `wia_except_list_t`

Each `wia_group_t` of Wii partition data contains one or more `wia_except_list_t` structs before the actual data, one for each 2 MiB of data in the `wia_group_t`. The number of `wia_except_list_t` structs per `wia_group_t` is always `chunk_size / 0x200000`, even for a `wia_group_t` which contains less data than normal due to it being at the end of a partition.

For memory management reasons, programs which read WIA files might place a limit on how many exceptions there can be in a `wia_except_list_t`. Dolphin's reading code has a limit of 52×64=3328 (unless the compression method is NONE or PURGE, in which case there is no limit), which is enough to cover all hashes and all padding. wit's reading code seems to be written as if 47×64=3008 is the maximum it needs to be able to handle, which is enough to cover all hashes but not any padding. However, because wit allocates more memory than needed, it seems to be possible to exceed 3008 by some amount without problems. It should be safe for writing code to assume that reading code can handle at least 3328 exceptions per `wia_except_list_t`.

|Type and name|Description|
|--|--|
|`u16 n_exceptions`|The number of `wia_exception_t` structs.|
|`wia_exception_t exception[n_exceptions]`|Each `wia_exception_t` describes one difference between the hashes obtained by hashing the partition data and the original hashes.|

Somewhat ironically, there are exceptions to how `wia_except_list_t` structs are handled:

 - For the compression method PURGE, the `wia_except_list_t` structs are stored uncompressed (in other words, before the first `wia_segment_t`). For BZIP2, LZMA and LZMA2, they are compressed along with the rest of the data.
 - For the compression methods NONE and PURGE, if the end offset of the last ``wia_except_list_t`` is not evenly divisible by 4, padding is inserted after it so that the data afterwards will start at a 4 byte boundary. This padding is not inserted for the other compression methods.

## `wia_segment_t`

This struct is used by the simple compression method PURGE, which stores runs of zeroes efficiently and stores other data as is.

|Type and name|Description|
|--|--|
|`u32 offset`|The offset of `data` within the decompressed data. (Any `wia_except_list_t` structs are not counted as part of the decompressed data.)|
|`u32 size`|The number of bytes in `data`.|
|`u8 data[size]`|Data.|

Each PURGE chunk contains zero or more `wia_segment_t` structs stored in order of ascending `offset`, followed by a SHA-1 hash (0x14 bytes) of the `wia_except_list_t` structs (if any) and the `wia_segment_t` structs. Bytes in the decompressed data that are not covered by any `wia_segment_t` struct are set to `0x00`.

# RVZ file format description

RVZ is a file format which is closely based on WIA. The differences are as follows:

* Zstandard has been added as a compression method. `compression` in `wia_disc_t` is set to 5 when Zstandard is used, and there is no compressor specific data. `compr_level` in `wia_disc_t` should be treated as signed instead of unsigned because Zstandard supports negative compression levels.
* PURGE has been removed as a compression method.
* Chunk sizes smaller than 2 MiB are supported. The following applies when using a chunk size smaller than 2 MiB:
    * The chunk size must be at least 32 KiB and must be a power of two. (Just like with WIA, sizes larger than 2 MiB do not have to be a power of two, they just have to be an integer multiple of 2 MiB.)
    * For Wii partition data, each chunk contains one `wia_except_list_t` which contains exceptions for that chunk (and no other chunks). Offset 0 refers to the first hash of the current chunk, not the first hash of the full 2 MiB of data.
* The `wia_group_t` struct has been expanded. See the `rvz_group_t` section below.
* Pseudorandom padding data is stored losslessly using an encoding scheme described in the *RVZ packing* section below.

## `rvz_group_t`

Compared to `wia_group_t`, `rvz_group_t` changes the meaning of the most significant bit of `data_size` and adds one additional attribute.

"Compressed data" below means the data as it is stored in the file. When compression is disabled, this "compressed data" is actually not compressed.

|Type and name|Description|
|--|--|
|`u32 data_off4`|The offset in the file where the compressed data is stored, divided by 4.|
|`u32 data_size`|The most significant bit is 1 if the data is compressed using the compression method indicated in `wia_disc_t`, and 0 if it is not compressed. The lower 31 bits are the size of the compressed data, including any `wia_except_list_t` structs. The lower 31 bits being 0 is a special case meaning that every byte of the decompressed and unpacked data is `0x00` and the `wia_except_list_t` structs (if there are supposed to be any) contain 0 exceptions.|
|`u32 rvz_packed_size`|The size after decompressing but before decoding the RVZ packing. If this is 0, RVZ packing is not used for this group.|

## RVZ packing

The RVZ packing encoding scheme can be applied to `wia_group_t` data, with any bzip2/LZMA/Zstandard compression being applied on top of it. (In other words, when reading an RVZ file, bzip2/LZMA/Zstandard decompression is done before decoding the RVZ packing.) RVZ packed data can be decoded as follows:

1. Read 4 bytes of data and interpret it as a 32-bit unsigned big endian integer. Call this `size`.
2. If the most significant bit of `size` is not set, read `size` bytes and output them unchanged. If the most significant bit of `size` is set, unset the most significant bit of `size`, then read 68 bytes of PRNG seed data and output `size` bytes using the PRNG algorithm described below.
3. Repeat until all input has been read.

### PRNG algorithm

The PRNG algorithm used for generating padding data on GameCube and Wii discs is a Lagged Fibonacci generator with the parameters f = xor, j = 32, k = 521.

Start by allocating a buffer of 521 32-bit words.

```
u32 buffer[521];
```

Copy the 68 bytes (17 words) of seed data into the start of the buffer. This seed data is stored in big endian in RVZ files, so remember to byteswap each word if the system is not big endian. Then, use the following code to fill the remaining part of the buffer:

```
for (size_t i = 17; i < 521; i++)
    buffer[i] = (buffer[i - 17] << 23) ^ (buffer[i - 16] >> 9) ^ buffer[i - 1];
```

The following code is used for advancing the state of the PRNG by a full buffer length. You must run it 4 times before you can start outputting data, and must then run it once after every 521 words of data you output.

```
for (size_t i = 0; i < 32; i++)
    buffer[i] ^= buffer[i + 521 - 32];

for (size_t i = 32; i < 521; i++)
    buffer[i] ^= buffer[i - 32];
```

After running the above code 4 times, you are ready to output data from the buffer -- but only if the offset (relative to the start of the disc for `wia_raw_data_t` and relative to the start of the partition data for `wia_part_t`) at which you are outputting data is evenly divisible by 32 KiB. Otherwise, you first have to advance the state of the PRNG by `offset % 0x8000` bytes. Please note that the hashes are not counted in the offset for `wia_part_t`, yet the number is still 32 KiB and not 31 KiB.

To finally output a word of data from the buffer, use the following code:

```
u8* out;
u32* buffer_ptr;

/* ... */

*(out++) = *buffer_ptr >> 24;
*(out++) = *buffer_ptr >> 18;  // NB: 18, not 16
*(out++) = *buffer_ptr >> 8;
*(out++) = *buffer_ptr;

buffer_ptr++;
```
