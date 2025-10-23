// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       list.c
/// \brief      Listing information about .xz files
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include "tuklib_integer.h"


/// Information about a .xz file
typedef struct {
	/// Combined Index of all Streams in the file
	lzma_index *idx;

	/// Total amount of Stream Padding
	uint64_t stream_padding;

	/// Highest memory usage so far
	uint64_t memusage_max;

	/// True if all Blocks so far have Compressed Size and
	/// Uncompressed Size fields
	bool all_have_sizes;

	/// Oldest XZ Utils version that will decompress the file
	uint32_t min_version;

} xz_file_info;

#define XZ_FILE_INFO_INIT { NULL, 0, 0, true, 50000002 }


/// Information about a .xz Block
typedef struct {
	/// Size of the Block Header
	uint32_t header_size;

	/// A few of the Block Flags as a string
	char flags[3];

	/// Size of the Compressed Data field in the Block
	lzma_vli compressed_size;

	/// Decoder memory usage for this Block
	uint64_t memusage;

	/// The filter chain of this Block in human-readable form
	char *filter_chain;

} block_header_info;

#define BLOCK_HEADER_INFO_INIT { .filter_chain = NULL }
#define block_header_info_end(bhi) free((bhi)->filter_chain)


/// Strings ending in a colon. These are used for lines like
/// "  Foo:   123 MiB". These are grouped because translated strings
/// may have different maximum string length, and we want to pad all
/// strings so that the values are aligned nicely.
static const char *colon_strs[] = {
	N_("Streams:"),
	N_("Blocks:"),
	N_("Compressed size:"),
	N_("Uncompressed size:"),
	N_("Ratio:"),
	N_("Check:"),
	N_("Stream Padding:"),
	N_("Memory needed:"),
	N_("Sizes in headers:"),
	// This won't be aligned because it's so long:
	//N_("Minimum XZ Utils version:"),
	N_("Number of files:"),
};

/// Enum matching the above strings.
enum {
	COLON_STR_STREAMS,
	COLON_STR_BLOCKS,
	COLON_STR_COMPRESSED_SIZE,
	COLON_STR_UNCOMPRESSED_SIZE,
	COLON_STR_RATIO,
	COLON_STR_CHECK,
	COLON_STR_STREAM_PADDING,
	COLON_STR_MEMORY_NEEDED,
	COLON_STR_SIZES_IN_HEADERS,
	//COLON_STR_MINIMUM_XZ_VERSION,
	COLON_STR_NUMBER_OF_FILES,
};

/// Field widths to use with printf to pad the strings to use the same number
/// of columns on a terminal.
static int colon_strs_fw[ARRAY_SIZE(colon_strs)];

/// Convenience macro to get the translated string and its field width
/// using a COLON_STR_foo enum.
#define COLON_STR(num) colon_strs_fw[num], _(colon_strs[num])


/// Column headings
static struct {
	/// Table column heading string
	const char *str;

	/// Number of terminal-columns to use for this table-column.
	/// If a translated string is longer than the initial value,
	/// this value will be increased in init_headings().
	int columns;

	/// Field width to use for printf() to pad "str" to use "columns"
	/// number of columns on a terminal. This is calculated in
	/// init_headings().
	int fw;

} headings[] = {
	{ N_("Stream"), 6, 0 },
	{ N_("Block"), 9, 0 },
	{ N_("Blocks"), 9, 0 },
	{ N_("CompOffset"), 15, 0 },
	{ N_("UncompOffset"), 15, 0 },
	{ N_("CompSize"), 15, 0 },
	{ N_("UncompSize"), 15, 0 },
	{ N_("TotalSize"), 15, 0 },
	{ N_("Ratio"), 5, 0 },
	{ N_("Check"), 10, 0 },
	{ N_("CheckVal"), 1, 0 },
	{ N_("Padding"), 7, 0 },
	{ N_("Header"), 5, 0 },
	{ N_("Flags"), 2, 0 },
	{ N_("MemUsage"), 7 + 4, 0 }, // +4 is for " MiB"
	{ N_("Filters"), 1, 0 },
};

/// Enum matching the above strings.
enum {
	HEADING_STREAM,
	HEADING_BLOCK,
	HEADING_BLOCKS,
	HEADING_COMPOFFSET,
	HEADING_UNCOMPOFFSET,
	HEADING_COMPSIZE,
	HEADING_UNCOMPSIZE,
	HEADING_TOTALSIZE,
	HEADING_RATIO,
	HEADING_CHECK,
	HEADING_CHECKVAL,
	HEADING_PADDING,
	HEADING_HEADERSIZE,
	HEADING_HEADERFLAGS,
	HEADING_MEMUSAGE,
	HEADING_FILTERS,
};

#define HEADING_STR(num) headings[num].fw, _(headings[num].str)


/// Check ID to string mapping
static const char check_names[LZMA_CHECK_ID_MAX + 1][12] = {
	// TRANSLATORS: Indicates that there is no integrity check.
	// This string is used in tables. In older xz version this
	// string was limited to ten columns in a fixed-width font, but
	// nowadays there is no strict length restriction anymore.
	N_("None"),
	"CRC32",
	// TRANSLATORS: Indicates that integrity check name is not known,
	// but the Check ID is known (here 2). In older xz version these
	// strings were limited to ten columns in a fixed-width font, but
	// nowadays there is no strict length restriction anymore.
	N_("Unknown-2"),
	N_("Unknown-3"),
	"CRC64",
	N_("Unknown-5"),
	N_("Unknown-6"),
	N_("Unknown-7"),
	N_("Unknown-8"),
	N_("Unknown-9"),
	"SHA-256",
	N_("Unknown-11"),
	N_("Unknown-12"),
	N_("Unknown-13"),
	N_("Unknown-14"),
	N_("Unknown-15"),
};

/// Buffer size for get_check_names(). This may be a bit ridiculous,
/// but at least it's enough if some language needs many multibyte chars.
#define CHECKS_STR_SIZE 1024


/// Value of the Check field as hexadecimal string.
/// This is set by parse_check_value().
static char check_value[2 * LZMA_CHECK_SIZE_MAX + 1];


/// Totals that are displayed if there was more than one file.
/// The "files" counter is also used in print_info_adv() to show
/// the file number.
static struct {
	uint64_t files;
	uint64_t streams;
	uint64_t blocks;
	uint64_t compressed_size;
	uint64_t uncompressed_size;
	uint64_t stream_padding;
	uint64_t memusage_max;
	uint32_t checks;
	uint32_t min_version;
	bool all_have_sizes;
} totals = { 0, 0, 0, 0, 0, 0, 0, 0, 50000002, true };


/// Initialize colon_strs_fw[].
static void
init_colon_strs(void)
{
	// Lengths of translated strings as bytes.
	size_t lens[ARRAY_SIZE(colon_strs)];

	// Lengths of translated strings as columns.
	size_t widths[ARRAY_SIZE(colon_strs)];

	// Maximum number of columns needed by a translated string.
	size_t width_max = 0;

	for (unsigned i = 0; i < ARRAY_SIZE(colon_strs); ++i) {
		widths[i] = tuklib_mbstr_width(_(colon_strs[i]), &lens[i]);

		// If debugging is enabled, catch invalid strings with
		// an assertion. However, when not debugging, use the
		// byte count as the fallback width. This shouldn't
		// ever happen unless there is a bad string in the
		// translations, but in such case I guess it's better
		// to try to print something useful instead of failing
		// completely.
		assert(widths[i] != (size_t)-1);
		if (widths[i] == (size_t)-1)
			widths[i] = lens[i];

		if (widths[i] > width_max)
			width_max = widths[i];
	}

	// Calculate the field width for printf("%*s") so that the strings
	// will use width_max columns on a terminal.
	for (unsigned i = 0; i < ARRAY_SIZE(colon_strs); ++i)
		colon_strs_fw[i] = (int)(lens[i] + width_max - widths[i]);

	return;
}


/// Initialize headings[].
static void
init_headings(void)
{
	// Before going through the heading strings themselves, treat
	// the Check heading specially: Look at the widths of the various
	// check names and increase the width of the Check column if needed.
	// The width of the heading name "Check" will then be handled normally
	// with other heading names in the second loop in this function.
	for (unsigned i = 0; i < ARRAY_SIZE(check_names); ++i) {
		size_t len;
		size_t w = tuklib_mbstr_width(_(check_names[i]), &len);

		// Error handling like in init_colon_strs().
		assert(w != (size_t)-1);
		if (w == (size_t)-1)
			w = len;

		// If the translated string is wider than the minimum width
		// set at compile time, increase the width.
		if ((size_t)(headings[HEADING_CHECK].columns) < w)
			headings[HEADING_CHECK].columns = (int)w;
	}

	for (unsigned i = 0; i < ARRAY_SIZE(headings); ++i) {
		size_t len;
		size_t w = tuklib_mbstr_width(_(headings[i].str), &len);

		// Error handling like in init_colon_strs().
		assert(w != (size_t)-1);
		if (w == (size_t)-1)
			w = len;

		// If the translated string is wider than the minimum width
		// set at compile time, increase the width.
		if ((size_t)(headings[i].columns) < w)
			headings[i].columns = (int)w;

		// Calculate the field width for printf("%*s") so that
		// the string uses .columns number of columns on a terminal.
		headings[i].fw = (int)(len + (size_t)headings[i].columns - w);
	}

	return;
}


/// Initialize the printf field widths that are needed to get nicely aligned
/// output with translated strings.
static void
init_field_widths(void)
{
	init_colon_strs();
	init_headings();
	return;
}


/// Convert XZ Utils version number to a string.
static const char *
xz_ver_to_str(uint32_t ver)
{
	static char buf[32];

	unsigned int major = ver / 10000000U;
	ver -= major * 10000000U;

	unsigned int minor = ver / 10000U;
	ver -= minor * 10000U;

	unsigned int patch = ver / 10U;
	ver -= patch * 10U;

	const char *stability = ver == 0 ? "alpha" : ver == 1 ? "beta" : "";

	snprintf(buf, sizeof(buf), "%u.%u.%u%s",
			major, minor, patch, stability);
	return buf;
}


/// \brief      Parse the Index(es) from the given .xz file
///
/// \param      xfi     Pointer to structure where the decoded information
///                     is stored.
/// \param      pair    Input file
///
/// \return     On success, false is returned. On error, true is returned.
///
static bool
parse_indexes(xz_file_info *xfi, file_pair *pair)
{
	if (pair->src_st.st_size <= 0) {
		message_error(_("%s: File is empty"),
				tuklib_mask_nonprint(pair->src_name));
		return true;
	}

	if (pair->src_st.st_size < 2 * LZMA_STREAM_HEADER_SIZE) {
		message_error(_("%s: Too small to be a valid .xz file"),
				tuklib_mask_nonprint(pair->src_name));
		return true;
	}

	io_buf buf;
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *idx = NULL;

	lzma_ret ret = lzma_file_info_decoder(&strm, &idx,
			hardware_memlimit_get(MODE_LIST),
			(uint64_t)(pair->src_st.st_size));
	if (ret != LZMA_OK) {
		message_error(_("%s: %s"),
				tuklib_mask_nonprint(pair->src_name),
				message_strm(ret));
		return true;
	}

	while (true) {
		if (strm.avail_in == 0) {
			strm.next_in = buf.u8;
			strm.avail_in = io_read(pair, &buf, IO_BUFFER_SIZE);
			if (strm.avail_in == SIZE_MAX)
				goto error;
		}

		ret = lzma_code(&strm, LZMA_RUN);

		switch (ret) {
		case LZMA_OK:
			break;

		case LZMA_SEEK_NEEDED:
			// liblzma won't ask us to seek past the known size
			// of the input file.
			assert(strm.seek_pos
					<= (uint64_t)(pair->src_st.st_size));
			if (io_seek_src(pair, strm.seek_pos))
				goto error;

			// avail_in must be zero so that we will read new
			// input.
			strm.avail_in = 0;
			break;

		case LZMA_STREAM_END: {
			lzma_end(&strm);
			xfi->idx = idx;

			// Calculate xfi->stream_padding.
			lzma_index_iter iter;
			lzma_index_iter_init(&iter, xfi->idx);
			while (!lzma_index_iter_next(&iter,
					LZMA_INDEX_ITER_STREAM))
				xfi->stream_padding += iter.stream.padding;

			return false;
		}

		default:
			message_error(_("%s: %s"),
					tuklib_mask_nonprint(pair->src_name),
					message_strm(ret));

			// If the error was too low memory usage limit,
			// show also how much memory would have been needed.
			if (ret == LZMA_MEMLIMIT_ERROR)
				message_mem_needed(V_ERROR,
						lzma_memusage(&strm));

			goto error;
		}
	}

error:
	lzma_end(&strm);
	return true;
}


/// \brief      Parse the Block Header
///
/// The result is stored into *bhi. The caller takes care of initializing it.
///
/// \return     False on success, true on error.
static bool
parse_block_header(file_pair *pair, const lzma_index_iter *iter,
		block_header_info *bhi, xz_file_info *xfi)
{
#if IO_BUFFER_SIZE < LZMA_BLOCK_HEADER_SIZE_MAX
#	error IO_BUFFER_SIZE < LZMA_BLOCK_HEADER_SIZE_MAX
#endif

	// Get the whole Block Header with one read, but don't read past
	// the end of the Block (or even its Check field).
	const uint32_t size = my_min(iter->block.total_size
				- lzma_check_size(iter->stream.flags->check),
			LZMA_BLOCK_HEADER_SIZE_MAX);
	io_buf buf;
	if (io_pread(pair, &buf, size, iter->block.compressed_file_offset))
		return true;

	// Zero would mean Index Indicator and thus not a valid Block.
	if (buf.u8[0] == 0)
		goto data_error;

	// Initialize the block structure and decode Block Header Size.
	lzma_filter filters[LZMA_FILTERS_MAX + 1];
	lzma_block block;
	block.version = 0;
	block.check = iter->stream.flags->check;
	block.filters = filters;

	block.header_size = lzma_block_header_size_decode(buf.u8[0]);
	if (block.header_size > size)
		goto data_error;

	// Decode the Block Header.
	switch (lzma_block_header_decode(&block, NULL, buf.u8)) {
	case LZMA_OK:
		break;

	case LZMA_OPTIONS_ERROR:
		message_error(_("%s: %s"),
				tuklib_mask_nonprint(pair->src_name),
				message_strm(LZMA_OPTIONS_ERROR));
		return true;

	case LZMA_DATA_ERROR:
		goto data_error;

	default:
		message_bug();
	}

	// Check the Block Flags. These must be done before calling
	// lzma_block_compressed_size(), because it overwrites
	// block.compressed_size.
	//
	// NOTE: If you add new characters here, update the minimum number of
	// columns in headings[HEADING_HEADERFLAGS] to match the number of
	// characters used here.
	bhi->flags[0] = block.compressed_size != LZMA_VLI_UNKNOWN
			? 'c' : '-';
	bhi->flags[1] = block.uncompressed_size != LZMA_VLI_UNKNOWN
			? 'u' : '-';
	bhi->flags[2] = '\0';

	// Collect information if all Blocks have both Compressed Size
	// and Uncompressed Size fields. They can be useful e.g. for
	// multi-threaded decompression so it can be useful to know it.
	xfi->all_have_sizes &= block.compressed_size != LZMA_VLI_UNKNOWN
			&& block.uncompressed_size != LZMA_VLI_UNKNOWN;

	// Validate or set block.compressed_size.
	switch (lzma_block_compressed_size(&block,
			iter->block.unpadded_size)) {
	case LZMA_OK:
		// Validate also block.uncompressed_size if it is present.
		// If it isn't present, there's no need to set it since
		// we aren't going to actually decompress the Block; if
		// we were decompressing, then we should set it so that
		// the Block decoder could validate the Uncompressed Size
		// that was stored in the Index.
		if (block.uncompressed_size == LZMA_VLI_UNKNOWN
				|| block.uncompressed_size
					== iter->block.uncompressed_size)
			break;

		// If the above fails, the file is corrupt so
		// LZMA_DATA_ERROR is a good error code.
		FALLTHROUGH;

	case LZMA_DATA_ERROR:
		// Free the memory allocated by lzma_block_header_decode().
		lzma_filters_free(filters, NULL);
		goto data_error;

	default:
		message_bug();
	}

	// Copy the known sizes.
	bhi->header_size = block.header_size;
	bhi->compressed_size = block.compressed_size;

	// Calculate the decoder memory usage and update the maximum
	// memory usage of this Block.
	bhi->memusage = lzma_raw_decoder_memusage(filters);
	if (xfi->memusage_max < bhi->memusage)
		xfi->memusage_max = bhi->memusage;

	// Determine the minimum XZ Utils version that supports this Block.
	//   - RISC-V filter needs 5.6.0.
	//
	//   - ARM64 filter needs 5.4.0.
	//
	//   - 5.0.0 doesn't support empty LZMA2 streams and thus empty
	//     Blocks that use LZMA2. This decoder bug was fixed in 5.0.2.
	if (xfi->min_version < 50060002U) {
		for (size_t i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i) {
			if (filters[i].id == LZMA_FILTER_RISCV) {
				xfi->min_version = 50060002U;
				break;
			}
		}
	}

	if (xfi->min_version < 50040002U) {
		for (size_t i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i) {
			if (filters[i].id == LZMA_FILTER_ARM64) {
				xfi->min_version = 50040002U;
				break;
			}
		}
	}

	if (xfi->min_version < 50000022U) {
		size_t i = 0;
		while (filters[i + 1].id != LZMA_VLI_UNKNOWN)
			++i;

		if (filters[i].id == LZMA_FILTER_LZMA2
				&& iter->block.uncompressed_size == 0)
			xfi->min_version = 50000022U;
	}

	// Convert the filter chain to human readable form.
	const lzma_ret str_ret = lzma_str_from_filters(
			&bhi->filter_chain, filters,
			LZMA_STR_DECODER | LZMA_STR_GETOPT_LONG, NULL);

	// Free the memory allocated by lzma_block_header_decode().
	lzma_filters_free(filters, NULL);

	// Check if the stringification succeeded.
	if (str_ret != LZMA_OK) {
		message_error(_("%s: %s"),
				tuklib_mask_nonprint(pair->src_name),
				message_strm(str_ret));
		return true;
	}

	return false;

data_error:
	// Show the error message.
	message_error(_("%s: %s"),
			tuklib_mask_nonprint(pair->src_name),
			message_strm(LZMA_DATA_ERROR));
	return true;
}


/// \brief      Parse the Check field and put it into check_value[]
///
/// \return     False on success, true on error.
static bool
parse_check_value(file_pair *pair, const lzma_index_iter *iter)
{
	// Don't read anything from the file if there is no integrity Check.
	if (iter->stream.flags->check == LZMA_CHECK_NONE) {
		snprintf(check_value, sizeof(check_value), "---");
		return false;
	}

	// Locate and read the Check field.
	const uint32_t size = lzma_check_size(iter->stream.flags->check);
	const uint64_t offset = iter->block.compressed_file_offset
			+ iter->block.total_size - size;
	io_buf buf;
	if (io_pread(pair, &buf, size, offset))
		return true;

	// CRC32 and CRC64 are in little endian. Guess that all the future
	// 32-bit and 64-bit Check values are little endian too. It shouldn't
	// be a too big problem if this guess is wrong.
	if (size == 4)
		snprintf(check_value, sizeof(check_value),
				"%08" PRIx32, conv32le(buf.u32[0]));
	else if (size == 8)
		snprintf(check_value, sizeof(check_value),
				"%016" PRIx64, conv64le(buf.u64[0]));
	else
		for (size_t i = 0; i < size; ++i)
			snprintf(check_value + i * 2, 3, "%02x", buf.u8[i]);

	return false;
}


/// \brief      Parse detailed information about a Block
///
/// Since this requires seek(s), listing information about all Blocks can
/// be slow.
///
/// \param      pair    Input file
/// \param      iter    Location of the Block whose Check value should
///                     be printed.
/// \param      bhi     Pointer to structure where to store the information
///                     about the Block Header field.
/// \param      xfi     Pointer to structure where to store the information
///                     about the entire .xz file.
///
/// \return     False on success, true on error. If an error occurs,
///             the error message is printed too so the caller doesn't
///             need to worry about that.
static bool
parse_details(file_pair *pair, const lzma_index_iter *iter,
		block_header_info *bhi, xz_file_info *xfi)
{
	if (parse_block_header(pair, iter, bhi, xfi))
		return true;

	if (parse_check_value(pair, iter))
		return true;

	return false;
}


/// \brief      Get the compression ratio
///
/// This has slightly different format than that is used in message.c.
static const char *
get_ratio(uint64_t compressed_size, uint64_t uncompressed_size)
{
	if (uncompressed_size == 0)
		return "---";

	const double ratio = (double)(compressed_size)
			/ (double)(uncompressed_size);
	if (ratio > 9.999)
		return "---";

	static char buf[16];
	snprintf(buf, sizeof(buf), "%.3f", ratio);
	return buf;
}


/// \brief      Get a comma-separated list of Check names
///
/// The check names are translated with gettext except when in robot mode.
///
/// \param      buf     Buffer to hold the resulting string
/// \param      checks  Bit mask of Checks to print
/// \param      space_after_comma
///                     It's better to not use spaces in table-like listings,
///                     but in more verbose formats a space after a comma
///                     is good for readability.
static void
get_check_names(char buf[CHECKS_STR_SIZE],
		uint32_t checks, bool space_after_comma)
{
	// If we get called when there are no Checks to print, set checks
	// to 1 so that we print "None". This can happen in the robot mode
	// when printing the totals line if there are no valid input files.
	if (checks == 0)
		checks = 1;

	char *pos = buf;
	size_t left = CHECKS_STR_SIZE;

	const char *sep = space_after_comma ? ", " : ",";
	bool comma = false;

	for (size_t i = 0; i <= LZMA_CHECK_ID_MAX; ++i) {
		if (checks & (UINT32_C(1) << i)) {
			my_snprintf(&pos, &left, "%s%s",
					comma ? sep : "",
					opt_robot ? check_names[i]
						: _(check_names[i]));
			comma = true;
		}
	}

	return;
}


static bool
print_info_basic(const xz_file_info *xfi, file_pair *pair)
{
	static bool headings_displayed = false;
	if (!headings_displayed) {
		headings_displayed = true;
		// TRANSLATORS: These are column headings. From Strms (Streams)
		// to Ratio, the columns are right aligned. Check and Filename
		// are left aligned. If you need longer words, it's OK to
		// use two lines here. Test with "xz -l foo.xz".
		puts(_("Strms  Blocks   Compressed Uncompressed  Ratio  "
				"Check   Filename"));
	}

	char checks[CHECKS_STR_SIZE];
	get_check_names(checks, lzma_index_checks(xfi->idx), false);

	const char *cols[6] = {
		uint64_to_str(lzma_index_stream_count(xfi->idx), 0),
		uint64_to_str(lzma_index_block_count(xfi->idx), 1),
		uint64_to_nicestr(lzma_index_file_size(xfi->idx),
			NICESTR_B, NICESTR_TIB, false, 2),
		uint64_to_nicestr(lzma_index_uncompressed_size(xfi->idx),
			NICESTR_B, NICESTR_TIB, false, 3),
		get_ratio(lzma_index_file_size(xfi->idx),
			lzma_index_uncompressed_size(xfi->idx)),
		checks,
	};
	printf("%*s %*s  %*s  %*s  %*s  %-*s %s\n",
			tuklib_mbstr_fw(cols[0], 5), cols[0],
			tuklib_mbstr_fw(cols[1], 7), cols[1],
			tuklib_mbstr_fw(cols[2], 11), cols[2],
			tuklib_mbstr_fw(cols[3], 11), cols[3],
			tuklib_mbstr_fw(cols[4], 5), cols[4],
			tuklib_mbstr_fw(cols[5], 7), cols[5],
			tuklib_mask_nonprint(pair->src_name));

	return false;
}


static void
print_adv_helper(uint64_t stream_count, uint64_t block_count,
		uint64_t compressed_size, uint64_t uncompressed_size,
		uint32_t checks, uint64_t stream_padding)
{
	char checks_str[CHECKS_STR_SIZE];
	get_check_names(checks_str, checks, true);

	printf("  %-*s %s\n", COLON_STR(COLON_STR_STREAMS),
			uint64_to_str(stream_count, 0));
	printf("  %-*s %s\n", COLON_STR(COLON_STR_BLOCKS),
			uint64_to_str(block_count, 0));
	printf("  %-*s %s\n", COLON_STR(COLON_STR_COMPRESSED_SIZE),
			uint64_to_nicestr(compressed_size,
				NICESTR_B, NICESTR_TIB, true, 0));
	printf("  %-*s %s\n", COLON_STR(COLON_STR_UNCOMPRESSED_SIZE),
			uint64_to_nicestr(uncompressed_size,
				NICESTR_B, NICESTR_TIB, true, 0));
	printf("  %-*s %s\n", COLON_STR(COLON_STR_RATIO),
			get_ratio(compressed_size, uncompressed_size));
	printf("  %-*s %s\n", COLON_STR(COLON_STR_CHECK), checks_str);
	printf("  %-*s %s\n", COLON_STR(COLON_STR_STREAM_PADDING),
			uint64_to_nicestr(stream_padding,
				NICESTR_B, NICESTR_TIB, true, 0));
	return;
}


static bool
print_info_adv(xz_file_info *xfi, file_pair *pair)
{
	// Print the overall information.
	print_adv_helper(lzma_index_stream_count(xfi->idx),
			lzma_index_block_count(xfi->idx),
			lzma_index_file_size(xfi->idx),
			lzma_index_uncompressed_size(xfi->idx),
			lzma_index_checks(xfi->idx),
			xfi->stream_padding);

	// Size of the biggest Check. This is used to calculate the width
	// of the CheckVal field. The table would get insanely wide if
	// we always reserved space for 64-byte Check (128 chars as hex).
	uint32_t check_max = 0;

	// Print information about the Streams.
	//
	// All except Check are right aligned; Check is left aligned.
	// Test with "xz -lv foo.xz".
	printf("  %s\n    %*s %*s %*s %*s %*s %*s  %*s  %-*s %*s\n",
			_(colon_strs[COLON_STR_STREAMS]),
			HEADING_STR(HEADING_STREAM),
			HEADING_STR(HEADING_BLOCKS),
			HEADING_STR(HEADING_COMPOFFSET),
			HEADING_STR(HEADING_UNCOMPOFFSET),
			HEADING_STR(HEADING_COMPSIZE),
			HEADING_STR(HEADING_UNCOMPSIZE),
			HEADING_STR(HEADING_RATIO),
			HEADING_STR(HEADING_CHECK),
			HEADING_STR(HEADING_PADDING));

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, xfi->idx);

	while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM)) {
		const char *cols1[4] = {
			uint64_to_str(iter.stream.number, 0),
			uint64_to_str(iter.stream.block_count, 1),
			uint64_to_str(iter.stream.compressed_offset, 2),
			uint64_to_str(iter.stream.uncompressed_offset, 3),
		};
		printf("    %*s %*s %*s %*s ",
			tuklib_mbstr_fw(cols1[0],
				headings[HEADING_STREAM].columns),
			cols1[0],
			tuklib_mbstr_fw(cols1[1],
				headings[HEADING_BLOCKS].columns),
			cols1[1],
			tuklib_mbstr_fw(cols1[2],
				headings[HEADING_COMPOFFSET].columns),
			cols1[2],
			tuklib_mbstr_fw(cols1[3],
				headings[HEADING_UNCOMPOFFSET].columns),
			cols1[3]);

		const char *cols2[5] = {
			uint64_to_str(iter.stream.compressed_size, 0),
			uint64_to_str(iter.stream.uncompressed_size, 1),
			get_ratio(iter.stream.compressed_size,
				iter.stream.uncompressed_size),
			_(check_names[iter.stream.flags->check]),
			uint64_to_str(iter.stream.padding, 2),
		};
		printf("%*s %*s  %*s  %-*s %*s\n",
			tuklib_mbstr_fw(cols2[0],
				headings[HEADING_COMPSIZE].columns),
			cols2[0],
			tuklib_mbstr_fw(cols2[1],
				headings[HEADING_UNCOMPSIZE].columns),
			cols2[1],
			tuklib_mbstr_fw(cols2[2],
				headings[HEADING_RATIO].columns),
			cols2[2],
			tuklib_mbstr_fw(cols2[3],
				headings[HEADING_CHECK].columns),
			cols2[3],
			tuklib_mbstr_fw(cols2[4],
				headings[HEADING_PADDING].columns),
			cols2[4]);

		// Update the maximum Check size.
		if (lzma_check_size(iter.stream.flags->check) > check_max)
			check_max = lzma_check_size(iter.stream.flags->check);
	}

	// Cache the verbosity level to a local variable.
	const bool detailed = message_verbosity_get() >= V_DEBUG;

	// Print information about the Blocks but only if there is
	// at least one Block.
	if (lzma_index_block_count(xfi->idx) > 0) {
		// Calculate the width of the CheckVal column. This can be
		// used as is as the field width for printf() when printing
		// the actual check value as it is hexadecimal. However, to
		// print the column heading, further calculation is needed
		// to handle a translated string (it's done a few lines later).
		assert(check_max <= LZMA_CHECK_SIZE_MAX);
		const int checkval_width = my_max(
				headings[HEADING_CHECKVAL].columns,
				(int)(2 * check_max));

		// All except Check are right aligned; Check is left aligned.
		printf("  %s\n    %*s %*s %*s %*s %*s %*s  %*s  %-*s",
				_(colon_strs[COLON_STR_BLOCKS]),
				HEADING_STR(HEADING_STREAM),
				HEADING_STR(HEADING_BLOCK),
				HEADING_STR(HEADING_COMPOFFSET),
				HEADING_STR(HEADING_UNCOMPOFFSET),
				HEADING_STR(HEADING_TOTALSIZE),
				HEADING_STR(HEADING_UNCOMPSIZE),
				HEADING_STR(HEADING_RATIO),
				detailed ? headings[HEADING_CHECK].fw : 1,
				_(headings[HEADING_CHECK].str));

		if (detailed) {
			// CheckVal (Check value), Flags, and Filters are
			// left aligned. Block Header Size, CompSize, and
			// MemUsage are right aligned. Test with
			// "xz -lvv foo.xz".
			printf(" %-*s  %*s  %-*s %*s %*s  %s",
				headings[HEADING_CHECKVAL].fw
					+ checkval_width
					- headings[HEADING_CHECKVAL].columns,
				_(headings[HEADING_CHECKVAL].str),
				HEADING_STR(HEADING_HEADERSIZE),
				HEADING_STR(HEADING_HEADERFLAGS),
				HEADING_STR(HEADING_COMPSIZE),
				HEADING_STR(HEADING_MEMUSAGE),
				_(headings[HEADING_FILTERS].str));
		}

		putchar('\n');

		lzma_index_iter_init(&iter, xfi->idx);

		// Iterate over the Blocks.
		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK)) {
			// If in detailed mode, collect the information from
			// Block Header before starting to print the next line.
			block_header_info bhi = BLOCK_HEADER_INFO_INIT;
			if (detailed && parse_details(pair, &iter, &bhi, xfi))
				return true;

			const char *cols1[4] = {
				uint64_to_str(iter.stream.number, 0),
				uint64_to_str(
					iter.block.number_in_stream, 1),
				uint64_to_str(
					iter.block.compressed_file_offset, 2),
				uint64_to_str(
					iter.block.uncompressed_file_offset, 3)
			};
			printf("    %*s %*s %*s %*s ",
				tuklib_mbstr_fw(cols1[0],
					headings[HEADING_STREAM].columns),
				cols1[0],
				tuklib_mbstr_fw(cols1[1],
					headings[HEADING_BLOCK].columns),
				cols1[1],
				tuklib_mbstr_fw(cols1[2],
					headings[HEADING_COMPOFFSET].columns),
				cols1[2],
				tuklib_mbstr_fw(cols1[3], headings[
					HEADING_UNCOMPOFFSET].columns),
				cols1[3]);

			const char *cols2[4] = {
				uint64_to_str(iter.block.total_size, 0),
				uint64_to_str(iter.block.uncompressed_size,
						1),
				get_ratio(iter.block.total_size,
					iter.block.uncompressed_size),
				_(check_names[iter.stream.flags->check])
			};
			printf("%*s %*s  %*s  %-*s",
				tuklib_mbstr_fw(cols2[0],
					headings[HEADING_TOTALSIZE].columns),
				cols2[0],
				tuklib_mbstr_fw(cols2[1],
					headings[HEADING_UNCOMPSIZE].columns),
				cols2[1],
				tuklib_mbstr_fw(cols2[2],
					headings[HEADING_RATIO].columns),
				cols2[2],
				tuklib_mbstr_fw(cols2[3], detailed
					? headings[HEADING_CHECK].columns : 1),
				cols2[3]);

			if (detailed) {
				const lzma_vli compressed_size
						= iter.block.unpadded_size
						- bhi.header_size
						- lzma_check_size(
						iter.stream.flags->check);

				const char *cols3[6] = {
					check_value,
					uint64_to_str(bhi.header_size, 0),
					bhi.flags,
					uint64_to_str(compressed_size, 1),
					uint64_to_str(
						round_up_to_mib(bhi.memusage),
						2),
					bhi.filter_chain
				};
				// Show MiB for memory usage, because it
				// is the only size which is not in bytes.
				printf(" %-*s  %*s  %-*s %*s %*s MiB  %s",
					checkval_width, cols3[0],
					tuklib_mbstr_fw(cols3[1], headings[
						HEADING_HEADERSIZE].columns),
					cols3[1],
					tuklib_mbstr_fw(cols3[2], headings[
						HEADING_HEADERFLAGS].columns),
					cols3[2],
					tuklib_mbstr_fw(cols3[3], headings[
						HEADING_COMPSIZE].columns),
					cols3[3],
					tuklib_mbstr_fw(cols3[4], headings[
						HEADING_MEMUSAGE].columns - 4),
					cols3[4],
					cols3[5]);
			}

			putchar('\n');
			block_header_info_end(&bhi);
		}
	}

	if (detailed) {
		printf("  %-*s %s MiB\n", COLON_STR(COLON_STR_MEMORY_NEEDED),
				uint64_to_str(
				round_up_to_mib(xfi->memusage_max), 0));
		printf("  %-*s %s\n", COLON_STR(COLON_STR_SIZES_IN_HEADERS),
				xfi->all_have_sizes ? _("Yes") : _("No"));
		//printf("  %-*s %s\n", COLON_STR(COLON_STR_MINIMUM_XZ_VERSION),
		printf("  %s %s\n", _("Minimum XZ Utils version:"),
				xz_ver_to_str(xfi->min_version));
	}

	return false;
}


static bool
print_info_robot(xz_file_info *xfi, file_pair *pair)
{
	char checks[CHECKS_STR_SIZE];
	get_check_names(checks, lzma_index_checks(xfi->idx), false);

	// Robot mode has to mask at least some control chars to prevent
	// the output from getting out of sync if filename is malicious.
	// Masking all non-printable chars is more than we need but
	// perhaps this is good enough in practice.
	printf("name\t%s\n", tuklib_mask_nonprint(pair->src_name));

	printf("file\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
			"\t%s\t%s\t%" PRIu64 "\n",
			lzma_index_stream_count(xfi->idx),
			lzma_index_block_count(xfi->idx),
			lzma_index_file_size(xfi->idx),
			lzma_index_uncompressed_size(xfi->idx),
			get_ratio(lzma_index_file_size(xfi->idx),
				lzma_index_uncompressed_size(xfi->idx)),
			checks,
			xfi->stream_padding);

	if (message_verbosity_get() >= V_VERBOSE) {
		lzma_index_iter iter;
		lzma_index_iter_init(&iter, xfi->idx);

		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM))
			printf("stream\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
				"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
				"\t%s\t%s\t%" PRIu64 "\n",
				iter.stream.number,
				iter.stream.block_count,
				iter.stream.compressed_offset,
				iter.stream.uncompressed_offset,
				iter.stream.compressed_size,
				iter.stream.uncompressed_size,
				get_ratio(iter.stream.compressed_size,
					iter.stream.uncompressed_size),
				check_names[iter.stream.flags->check],
				iter.stream.padding);

		lzma_index_iter_rewind(&iter);

		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK)) {
			block_header_info bhi = BLOCK_HEADER_INFO_INIT;
			if (message_verbosity_get() >= V_DEBUG
					&& parse_details(
						pair, &iter, &bhi, xfi))
				return true;

			printf("block\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
					"\t%" PRIu64 "\t%" PRIu64
					"\t%" PRIu64 "\t%" PRIu64 "\t%s\t%s",
					iter.stream.number,
					iter.block.number_in_stream,
					iter.block.number_in_file,
					iter.block.compressed_file_offset,
					iter.block.uncompressed_file_offset,
					iter.block.total_size,
					iter.block.uncompressed_size,
					get_ratio(iter.block.total_size,
						iter.block.uncompressed_size),
					check_names[iter.stream.flags->check]);

			if (message_verbosity_get() >= V_DEBUG)
				printf("\t%s\t%" PRIu32 "\t%s\t%" PRIu64
						"\t%" PRIu64 "\t%s",
						check_value,
						bhi.header_size,
						bhi.flags,
						bhi.compressed_size,
						bhi.memusage,
						bhi.filter_chain);

			putchar('\n');
			block_header_info_end(&bhi);
		}
	}

	if (message_verbosity_get() >= V_DEBUG)
		printf("summary\t%" PRIu64 "\t%s\t%" PRIu32 "\n",
				xfi->memusage_max,
				xfi->all_have_sizes ? "yes" : "no",
				xfi->min_version);

	return false;
}


static void
update_totals(const xz_file_info *xfi)
{
	// TODO: Integer overflow checks
	++totals.files;
	totals.streams += lzma_index_stream_count(xfi->idx);
	totals.blocks += lzma_index_block_count(xfi->idx);
	totals.compressed_size += lzma_index_file_size(xfi->idx);
	totals.uncompressed_size += lzma_index_uncompressed_size(xfi->idx);
	totals.stream_padding += xfi->stream_padding;
	totals.checks |= lzma_index_checks(xfi->idx);

	if (totals.memusage_max < xfi->memusage_max)
		totals.memusage_max = xfi->memusage_max;

	if (totals.min_version < xfi->min_version)
		totals.min_version = xfi->min_version;

	totals.all_have_sizes &= xfi->all_have_sizes;

	return;
}


static void
print_totals_basic(void)
{
	// Print a separator line.
	char line[80];
	memset(line, '-', sizeof(line));
	line[sizeof(line) - 1] = '\0';
	puts(line);

	// Get the check names.
	char checks[CHECKS_STR_SIZE];
	get_check_names(checks, totals.checks, false);

	// Print the totals except the file count, which needs
	// special handling.
	printf("%5s %7s  %11s  %11s  %5s  %-7s ",
			uint64_to_str(totals.streams, 0),
			uint64_to_str(totals.blocks, 1),
			uint64_to_nicestr(totals.compressed_size,
				NICESTR_B, NICESTR_TIB, false, 2),
			uint64_to_nicestr(totals.uncompressed_size,
				NICESTR_B, NICESTR_TIB, false, 3),
			get_ratio(totals.compressed_size,
				totals.uncompressed_size),
			checks);

#if defined(__sun) && (defined(__GNUC__) || defined(__clang__))
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	// Since we print totals only when there are at least two files,
	// the English message will always use "%s files". But some other
	// languages need different forms for different plurals so we
	// have to translate this with ngettext().
	//
	// TRANSLATORS: %s is an integer. Only the plural form of this
	// message is used (e.g. "2 files"). Test with "xz -l foo.xz bar.xz".
	printf(ngettext("%s file\n", "%s files\n",
			totals.files <= ULONG_MAX ? totals.files
				: (totals.files % 1000000) + 1000000),
			uint64_to_str(totals.files, 0));
#if defined(__sun) && (defined(__GNUC__) || defined(__clang__))
#	pragma GCC diagnostic pop
#endif

	return;
}


static void
print_totals_adv(void)
{
	putchar('\n');
	puts(_("Totals:"));
	printf("  %-*s %s\n", COLON_STR(COLON_STR_NUMBER_OF_FILES),
			uint64_to_str(totals.files, 0));
	print_adv_helper(totals.streams, totals.blocks,
			totals.compressed_size, totals.uncompressed_size,
			totals.checks, totals.stream_padding);

	if (message_verbosity_get() >= V_DEBUG) {
		printf("  %-*s %s MiB\n", COLON_STR(COLON_STR_MEMORY_NEEDED),
				uint64_to_str(
				round_up_to_mib(totals.memusage_max), 0));
		printf("  %-*s %s\n", COLON_STR(COLON_STR_SIZES_IN_HEADERS),
				totals.all_have_sizes ? _("Yes") : _("No"));
		//printf("  %-*s %s\n", COLON_STR(COLON_STR_MINIMUM_XZ_VERSION),
		printf("  %s %s\n", _("Minimum XZ Utils version:"),
				xz_ver_to_str(totals.min_version));
	}

	return;
}


static void
print_totals_robot(void)
{
	char checks[CHECKS_STR_SIZE];
	get_check_names(checks, totals.checks, false);

	printf("totals\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
			"\t%s\t%s\t%" PRIu64 "\t%" PRIu64,
			totals.streams,
			totals.blocks,
			totals.compressed_size,
			totals.uncompressed_size,
			get_ratio(totals.compressed_size,
				totals.uncompressed_size),
			checks,
			totals.stream_padding,
			totals.files);

	if (message_verbosity_get() >= V_DEBUG)
		printf("\t%" PRIu64 "\t%s\t%" PRIu32,
				totals.memusage_max,
				totals.all_have_sizes ? "yes" : "no",
				totals.min_version);

	putchar('\n');

	return;
}


extern void
list_totals(void)
{
	if (opt_robot) {
		// Always print totals in --robot mode. It can be convenient
		// in some cases and doesn't complicate usage of the
		// single-file case much.
		print_totals_robot();

	} else if (totals.files > 1) {
		// For non-robot mode, totals are printed only if there
		// is more than one file.
		if (message_verbosity_get() <= V_WARNING)
			print_totals_basic();
		else
			print_totals_adv();
	}

	return;
}


extern void
list_file(const char *filename)
{
	if (opt_format != FORMAT_XZ && opt_format != FORMAT_AUTO) {
		// The 'lzmainfo' message is printed only when --format=lzma
		// is used (it is implied if using "lzma" as the command
		// name). Thus instead of using message_fatal(), print
		// the messages separately and then call tuklib_exit()
		// like message_fatal() does.
		message(V_ERROR, _("--list works only on .xz files "
				"(--format=xz or --format=auto)"));

		if (opt_format == FORMAT_LZMA)
			message(V_ERROR,
				_("Try 'lzmainfo' with .lzma files."));

		tuklib_exit(E_ERROR, E_ERROR, false);
	}

	message_filename(filename);

	if (filename == stdin_filename) {
		message_error(_("--list does not support reading from "
				"standard input"));
		return;
	}

	init_field_widths();

	// Unset opt_stdout so that io_open_src() won't accept special files.
	// Set opt_force so that io_open_src() will follow symlinks.
	opt_stdout = false;
	opt_force = true;
	file_pair *pair = io_open_src(filename);
	if (pair == NULL)
		return;

	xz_file_info xfi = XZ_FILE_INFO_INIT;
	if (!parse_indexes(&xfi, pair)) {
		bool fail;

		// We have three main modes:
		//  - --robot, which has submodes if --verbose is specified
		//    once or twice
		//  - Normal --list without --verbose
		//  - --list with one or two --verbose
		if (opt_robot)
			fail = print_info_robot(&xfi, pair);
		else if (message_verbosity_get() <= V_WARNING)
			fail = print_info_basic(&xfi, pair);
		else
			fail = print_info_adv(&xfi, pair);

		// Update the totals that are displayed after all
		// the individual files have been listed. Don't count
		// broken files.
		if (!fail)
			update_totals(&xfi);

		lzma_index_end(xfi.idx, NULL);
	}

	io_close(pair, false);
	return;
}
