// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       file_io.h
/// \brief      I/O types and functions
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

// Some systems have suboptimal BUFSIZ. Use a bit bigger value on them.
// We also need that IO_BUFFER_SIZE is a multiple of 8 (sizeof(uint64_t))
#if BUFSIZ <= 1024
#	define IO_BUFFER_SIZE 8192
#else
#	define IO_BUFFER_SIZE (BUFSIZ & ~7U)
#endif

#ifdef _MSC_VER
	// The first one renames both "struct stat" -> "struct _stat64"
	// and stat() -> _stat64(). The documentation mentions only
	// "struct __stat64", not "struct _stat64", but the latter
	// works too.
#	define stat _stat64
#	define fstat _fstat64
#	define off_t __int64
#endif


/// is_sparse() accesses the buffer as uint64_t for maximum speed.
/// The u32 and u64 members must only be access through this union
/// to avoid strict aliasing violations. Taking a pointer of u8
/// should be fine as long as uint8_t maps to unsigned char which
/// can alias anything.
typedef union {
	uint8_t u8[IO_BUFFER_SIZE];
	uint32_t u32[IO_BUFFER_SIZE / sizeof(uint32_t)];
	uint64_t u64[IO_BUFFER_SIZE / sizeof(uint64_t)];
} io_buf;


typedef struct {
	/// Name of the source filename (as given on the command line) or
	/// pointer to static "(stdin)" when reading from standard input.
	const char *src_name;

	/// Destination filename converted from src_name or pointer to static
	/// "(stdout)" when writing to standard output.
	char *dest_name;

	/// File descriptor of the source file
	int src_fd;

	/// File descriptor of the target file
	int dest_fd;

#ifndef TUKLIB_DOSLIKE
	/// File descriptor of the directory of the target file (which is
	/// also the directory of the source file)
	int dir_fd;
#endif

	/// True once end of the source file has been detected.
	bool src_eof;

	/// For --flush-timeout: True if at least one byte has been read
	/// since the previous flush or the start of the file.
	bool src_has_seen_input;

	/// For --flush-timeout: True when flushing is needed.
	bool flush_needed;

	/// If true, we look for long chunks of zeros and try to create
	/// a sparse file.
	bool dest_try_sparse;

	/// This is used only if dest_try_sparse is true. This holds the
	/// number of zero bytes we haven't written out, because we plan
	/// to make that byte range a sparse chunk.
	off_t dest_pending_sparse;

	/// Stat of the source file.
	struct stat src_st;

	/// Stat of the destination file.
	struct stat dest_st;

} file_pair;


/// \brief      Initialize the I/O module
extern void io_init(void);


#ifndef TUKLIB_DOSLIKE
/// \brief      Write a byte to user_abort_pipe[1]
///
/// This is called from a signal handler.
extern void io_write_to_user_abort_pipe(void);
#endif


/// \brief      Disable creation of sparse files when decompressing
extern void io_no_sparse(void);


/// \brief      Open the source file
extern file_pair *io_open_src(const char *src_name);


/// \brief      Open the destination file
extern bool io_open_dest(file_pair *pair);


/// \brief      Closes the file descriptors and frees possible allocated memory
///
/// The success argument determines if source or destination file gets
/// unlinked:
///  - false: The destination file is unlinked.
///  - true: The source file is unlinked unless writing to stdout or --keep
///    was used.
extern void io_close(file_pair *pair, bool success);


/// \brief      Reads from the source file to a buffer
///
/// \param      pair    File pair having the source file open for reading
/// \param      buf     Destination buffer to hold the read data
/// \param      size    Size of the buffer; must be at most IO_BUFFER_SIZE
///
/// \return     On success, number of bytes read is returned. On end of
///             file zero is returned and pair->src_eof set to true.
///             On error, SIZE_MAX is returned and error message printed.
extern size_t io_read(file_pair *pair, io_buf *buf, size_t size);


/// \brief      Fix the position in src_fd
///
/// This is used when --single-thream has been specified and decompression
/// is successful. If the input file descriptor supports seeking, this
/// function fixes the input position to point to the next byte after the
/// decompressed stream.
///
/// \param      pair        File pair having the source file open for reading
/// \param      rewind_size How many bytes of extra have been read i.e.
///                         how much to seek backwards.
extern void io_fix_src_pos(file_pair *pair, size_t rewind_size);


/// \brief      Seek to the given absolute position in the source file
///
/// This calls lseek() and also clears pair->src_eof.
///
/// \param      pair    Seekable source file
/// \param      pos     Offset relative to the beginning of the file,
///                     from which the data should be read.
///
/// \return     On success, false is returned. On error, error message
///             is printed and true is returned.
extern bool io_seek_src(file_pair *pair, uint64_t pos);


/// \brief      Read from source file from given offset to a buffer
///
/// This is remotely similar to standard pread(). This uses lseek() though,
/// so the read offset is changed on each call.
///
/// \param      pair    Seekable source file
/// \param      buf     Destination buffer
/// \param      size    Amount of data to read
/// \param      pos     Offset relative to the beginning of the file,
///                     from which the data should be read.
///
/// \return     On success, false is returned. On error, error message
///             is printed and true is returned.
extern bool io_pread(file_pair *pair, io_buf *buf, size_t size, uint64_t pos);


/// \brief      Writes a buffer to the destination file
///
/// \param      pair    File pair having the destination file open for writing
/// \param      buf     Buffer containing the data to be written
/// \param      size    Size of the buffer; must be at most IO_BUFFER_SIZE
///
/// \return     On success, false is returned. On error, error message
///             is printed and true is returned.
extern bool io_write(file_pair *pair, const io_buf *buf, size_t size);
