// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.c
/// \brief      Miscellaneous utility functions
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include <stdarg.h>


/// Buffers for uint64_to_str() and uint64_to_nicestr()
static char bufs[4][128];


// Thousand separator support in uint64_to_str() and uint64_to_nicestr():
//
// DJGPP 2.05 added support for thousands separators but it's broken
// at least under WinXP with Finnish locale that uses a non-breaking space
// as the thousands separator. Workaround by disabling thousands separators
// for DJGPP builds.
//
// MSVC doesn't support thousand separators.
//
// MinGW-w64 supports thousand separators only with its own stdio functions
// which our sysdefs.h disables when _UCRT && HAVE_SMALL.
#if defined(__DJGPP__) || defined(_MSC_VER) \
		|| (defined(__MINGW32__) && __USE_MINGW_ANSI_STDIO == 0)
#	define FORMAT_THOUSAND_SEP(prefix, suffix) prefix suffix
#	define check_thousand_sep(slot) do { } while (0)
#else
#	define FORMAT_THOUSAND_SEP(prefix, suffix) ((thousand == WORKS) \
			? prefix "'" suffix \
			: prefix suffix)

static enum { UNKNOWN, WORKS, BROKEN } thousand = UNKNOWN;

/// Check if thousands separator is supported. Run-time checking is easiest
/// because it seems to be sometimes lacking even on a POSIXish system.
/// Note that trying to use thousands separators when snprintf() doesn't
/// support them results in undefined behavior. This just has happened to
/// work well enough in practice.
///
/// This must be called before using the FORMAT_THOUSAND_SEP macro.
static void
check_thousand_sep(uint32_t slot)
{
	if (thousand == UNKNOWN) {
		bufs[slot][0] = '\0';
		snprintf(bufs[slot], sizeof(bufs[slot]), "%'u", 1U);
		thousand = bufs[slot][0] == '1' ? WORKS : BROKEN;
	}

	return;
}
#endif


extern void *
xrealloc(void *ptr, size_t size)
{
	assert(size > 0);

	// Save ptr so that we can free it if realloc fails.
	// The point is that message_fatal ends up calling stdio functions
	// which in some libc implementations might allocate memory from
	// the heap. Freeing ptr improves the chances that there's free
	// memory for stdio functions if they need it.
	void *p = ptr;
	ptr = realloc(ptr, size);

	if (ptr == NULL) {
		const int saved_errno = errno;
		free(p);
		message_fatal("%s", strerror(saved_errno));
	}

	return ptr;
}


extern char *
xstrdup(const char *src)
{
	assert(src != NULL);
	const size_t size = strlen(src) + 1;
	char *dest = xmalloc(size);
	return memcpy(dest, src, size);
}


extern uint64_t
str_to_uint64(const char *name, const char *value, uint64_t min, uint64_t max)
{
	uint64_t result = 0;

	// Skip blanks.
	while (*value == ' ' || *value == '\t')
		++value;

	// Accept special value "max". Supporting "min" doesn't seem useful.
	if (strcmp(value, "max") == 0)
		return max;

	if (*value < '0' || *value > '9')
		message_fatal(_("%s: %s"), value,
			_("Value is not a non-negative decimal integer"));

	do {
		// Don't overflow.
		if (result > UINT64_MAX / 10)
			goto error;

		result *= 10;

		// Another overflow check
		const uint32_t add = (uint32_t)(*value - '0');
		if (UINT64_MAX - add < result)
			goto error;

		result += add;
		++value;
	} while (*value >= '0' && *value <= '9');

	if (*value != '\0') {
		// Look for suffix. Originally this supported both base-2
		// and base-10, but since there seems to be little need
		// for base-10 in this program, treat everything as base-2
		// and also be more relaxed about the case of the first
		// letter of the suffix.
		uint64_t multiplier = 0;
		if (*value == 'k' || *value == 'K')
			multiplier = UINT64_C(1) << 10;
		else if (*value == 'm' || *value == 'M')
			multiplier = UINT64_C(1) << 20;
		else if (*value == 'g' || *value == 'G')
			multiplier = UINT64_C(1) << 30;

		++value;

		// Allow also e.g. Ki, KiB, and KB.
		if (*value != '\0' && strcmp(value, "i") != 0
				&& strcmp(value, "iB") != 0
				&& strcmp(value, "B") != 0)
			multiplier = 0;

		if (multiplier == 0) {
			message(V_ERROR, _("%s: Invalid multiplier suffix"),
					value - 1);
			message_fatal(_("Valid suffixes are 'KiB' (2^10), "
					"'MiB' (2^20), and 'GiB' (2^30)."));
		}

		// Don't overflow here either.
		if (result > UINT64_MAX / multiplier)
			goto error;

		result *= multiplier;
	}

	if (result < min || result > max)
		goto error;

	return result;

error:
	message_fatal(_("Value of the option '%s' must be in the range "
				"[%" PRIu64 ", %" PRIu64 "]"),
				name, min, max);
}


extern uint64_t
round_up_to_mib(uint64_t n)
{
	return (n >> 20) + ((n & ((UINT32_C(1) << 20) - 1)) != 0);
}


extern const char *
uint64_to_str(uint64_t value, uint32_t slot)
{
	assert(slot < ARRAY_SIZE(bufs));

	check_thousand_sep(slot);

	snprintf(bufs[slot], sizeof(bufs[slot]),
			FORMAT_THOUSAND_SEP("%", PRIu64), value);

	return bufs[slot];
}


extern const char *
uint64_to_nicestr(uint64_t value, enum nicestr_unit unit_min,
		enum nicestr_unit unit_max, bool always_also_bytes,
		uint32_t slot)
{
	assert(unit_min <= unit_max);
	assert(unit_max <= NICESTR_TIB);
	assert(slot < ARRAY_SIZE(bufs));

	check_thousand_sep(slot);

	enum nicestr_unit unit = NICESTR_B;
	char *pos = bufs[slot];
	size_t left = sizeof(bufs[slot]);

	if ((unit_min == NICESTR_B && value < 10000)
			|| unit_max == NICESTR_B) {
		// The value is shown as bytes.
		my_snprintf(&pos, &left, FORMAT_THOUSAND_SEP("%", "u"),
				(unsigned int)value);
	} else {
		// Scale the value to a nicer unit. Unless unit_min and
		// unit_max limit us, we will show at most five significant
		// digits with one decimal place.
		double d = (double)(value);
		do {
			d /= 1024.0;
			++unit;
		} while (unit < unit_min || (d > 9999.9 && unit < unit_max));

		my_snprintf(&pos, &left, FORMAT_THOUSAND_SEP("%", ".1f"), d);
	}

	static const char suffix[5][4] = { "B", "KiB", "MiB", "GiB", "TiB" };
	my_snprintf(&pos, &left, " %s", suffix[unit]);

	if (always_also_bytes && value >= 10000)
		snprintf(pos, left, FORMAT_THOUSAND_SEP(" (%", PRIu64 " B)"),
				value);

	return bufs[slot];
}


extern void
my_snprintf(char **pos, size_t *left, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	const int len = vsnprintf(*pos, *left, fmt, ap);
	va_end(ap);

	// If an error occurred, we want the caller to think that the whole
	// buffer was used. This way no more data will be written to the
	// buffer. We don't need better error handling here, although it
	// is possible that the result looks garbage on the terminal if
	// e.g. an UTF-8 character gets split. That shouldn't (easily)
	// happen though, because the buffers used have some extra room.
	if (len < 0 || (size_t)(len) >= *left) {
		*left = 0;
	} else {
		*pos += len;
		*left -= (size_t)(len);
	}

	return;
}


extern bool
is_tty(int fd)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	// There is no need to check if handle == INVALID_HANDLE_VALUE
	// because it will return false anyway when used in GetConsoleMode().
	// The resulting HANDLE is owned by the file descriptor.
	// The HANDLE must not be closed here.
	intptr_t handle = _get_osfhandle(fd);
	DWORD mode;

	// GetConsoleMode() is an easy way to tell if the HANDLE is a
	// console or not. We do not care about the value of mode since we
	// do not plan to use any further Windows console functions.
	return GetConsoleMode((HANDLE)handle, &mode);
#else
	return isatty(fd);
#endif
}


extern bool
is_tty_stdin(void)
{
	const bool ret = is_tty(STDIN_FILENO);

	if (ret)
		message_error(_("Compressed data cannot be read from "
				"a terminal"));

	return ret;
}


extern bool
is_tty_stdout(void)
{
	const bool ret = is_tty(STDOUT_FILENO);

	if (ret)
		message_error(_("Compressed data cannot be written to "
				"a terminal"));

	return ret;
}
