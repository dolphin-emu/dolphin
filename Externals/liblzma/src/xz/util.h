// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.h
/// \brief      Miscellaneous utility functions
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

/// \brief      Safe malloc() that never returns NULL
///
/// \note       xmalloc(), xrealloc(), and xstrdup() must not be used when
///             there are files open for writing, that should be cleaned up
///             before exiting.
#define xmalloc(size) xrealloc(NULL, size)


/// \brief      Safe realloc() that never returns NULL
lzma_attr_alloc_size(2)
extern void *xrealloc(void *ptr, size_t size);


/// \brief      Safe strdup() that never returns NULL
extern char *xstrdup(const char *src);


/// \brief      Fancy version of strtoull()
///
/// \param      name    Name of the option to show in case of an error
/// \param      value   String containing the number to be parsed; may
///                     contain suffixes "k", "M", "G", "Ki", "Mi", or "Gi"
/// \param      min     Minimum valid value
/// \param      max     Maximum valid value
///
/// \return     Parsed value that is in the range [min, max]. Does not return
///             if an error occurs.
///
extern uint64_t str_to_uint64(const char *name, const char *value,
		uint64_t min, uint64_t max);


/// \brief      Round an integer up to the next full MiB and convert to MiB
///
/// This is used when printing memory usage and limit.
extern uint64_t round_up_to_mib(uint64_t n);


/// \brief      Convert uint64_t to a string
///
/// Convert the given value to a string with locale-specific thousand
/// separators, if supported by the snprintf() implementation. The string
/// is stored into an internal static buffer indicated by the slot argument.
/// A pointer to the selected buffer is returned.
///
/// This function exists, because non-POSIX systems don't support thousand
/// separator in format strings. Solving the problem in a simple way doesn't
/// work, because it breaks gettext (specifically, the xgettext tool).
extern const char *uint64_to_str(uint64_t value, uint32_t slot);


enum nicestr_unit {
	NICESTR_B,
	NICESTR_KIB,
	NICESTR_MIB,
	NICESTR_GIB,
	NICESTR_TIB,
};


/// \brief      Convert uint64_t to a nice human readable string
///
/// This is like uint64_to_str() but uses B, KiB, MiB, GiB, or TiB suffix
/// and optionally includes the exact size in parenthesis.
///
/// \param      value     Value to be printed
/// \param      unit_min  Smallest unit to use. This and unit_max are used
///                       e.g. when showing the progress indicator to force
///                       the unit to MiB.
/// \param      unit_max  Biggest unit to use. assert(unit_min <= unit_max).
/// \param      always_also_bytes
///                       Show also the exact byte value in parenthesis
///                       if the nicely formatted string uses bigger unit
///                       than bytes.
/// \param      slot      Which static buffer to use to hold the string.
///                       This is shared with uint64_to_str().
///
/// \return     Pointer to statically allocated buffer containing the string.
///
/// \note       This uses double_to_str() internally so the static buffer
///             in double_to_str() will be overwritten.
///
extern const char *uint64_to_nicestr(uint64_t value,
		enum nicestr_unit unit_min, enum nicestr_unit unit_max,
		bool always_also_bytes, uint32_t slot);


/// \brief      Wrapper for snprintf() to help constructing a string in pieces
///
/// A maximum of *left bytes is written starting from *pos. *pos and *left
/// are updated accordingly.
lzma_attribute((__format__(__printf__, 3, 4)))
extern void my_snprintf(char **pos, size_t *left, const char *fmt, ...);


/// \brief      Test if file descriptor is a terminal
///
/// For POSIX systems, this is a simple wrapper around isatty(). However on
/// Windows, isatty() returns true for all character devices, not just
/// terminals.
///
/// \param      fd    File descriptor to test
///
/// \return     bool:
///             - true if file descriptor is a terminal
///             - false otherwise
extern bool is_tty(int fd);


/// \brief      Test if stdin is a terminal
///
/// If stdin is a terminal, an error message is printed and exit status set
/// to EXIT_ERROR.
extern bool is_tty_stdin(void);


/// \brief      Test if stdout is a terminal
///
/// If stdout is a terminal, an error message is printed and exit status set
/// to EXIT_ERROR.
extern bool is_tty_stdout(void);
