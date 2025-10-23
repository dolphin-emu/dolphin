// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       main.c
/// \brief      main()
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include <ctype.h>


/// Exit status to use. This can be changed with set_exit_status().
static enum exit_status_type exit_status = E_SUCCESS;

#if defined(_WIN32) && !defined(__CYGWIN__)
/// exit_status has to be protected with a critical section due to
/// how "signal handling" is done on Windows. See signals.c for details.
static CRITICAL_SECTION exit_status_cs;
#endif

/// True if --no-warn is specified. When this is true, we don't set
/// the exit status to E_WARNING when something worth a warning happens.
static bool no_warn = false;


extern void
set_exit_status(enum exit_status_type new_status)
{
	assert(new_status == E_WARNING || new_status == E_ERROR);

#if defined(_WIN32) && !defined(__CYGWIN__)
	EnterCriticalSection(&exit_status_cs);
#endif

	if (exit_status != E_ERROR)
		exit_status = new_status;

#if defined(_WIN32) && !defined(__CYGWIN__)
	LeaveCriticalSection(&exit_status_cs);
#endif

	return;
}


extern void
set_exit_no_warn(void)
{
	no_warn = true;
	return;
}


static const char *
read_name(const args_info *args)
{
	// FIXME: Maybe we should have some kind of memory usage limit here
	// like the tool has for the actual compression and decompression.
	// Giving some huge text file with --files0 makes us to read the
	// whole file in RAM.
	static char *name = NULL;
	static size_t size = 256;

	// Allocate the initial buffer. This is never freed, since after it
	// is no longer needed, the program exits very soon. It is safe to
	// use xmalloc() and xrealloc() in this function, because while
	// executing this function, no files are open for writing, and thus
	// there's no need to cleanup anything before exiting.
	if (name == NULL)
		name = xmalloc(size);

	// Write position in name
	size_t pos = 0;

	// Read one character at a time into name.
	while (!user_abort) {
		const int c = fgetc(args->files_file);

		if (ferror(args->files_file)) {
			// Take care of EINTR since we have established
			// the signal handlers already.
			if (errno == EINTR)
				continue;

			message_error(_("%s: Error reading filenames: %s"),
				tuklib_mask_nonprint(args->files_name),
				strerror(errno));
			return NULL;
		}

		if (feof(args->files_file)) {
			if (pos != 0)
				message_error(_("%s: Unexpected end of input "
						"when reading filenames"),
						tuklib_mask_nonprint(
							args->files_name));

			return NULL;
		}

		if (c == args->files_delim) {
			// We allow consecutive newline (--files) or '\0'
			// characters (--files0), and ignore such empty
			// filenames.
			if (pos == 0)
				continue;

			// A non-empty name was read. Terminate it with '\0'
			// and return it.
			name[pos] = '\0';
			return name;
		}

		if (c == '\0') {
			// A null character was found when using --files,
			// which expects plain text input separated with
			// newlines.
			message_error(_("%s: Null character found when "
					"reading filenames; maybe you meant "
					"to use '--files0' instead "
					"of '--files'?"),
					tuklib_mask_nonprint(
						args->files_name));
			return NULL;
		}

		name[pos++] = c;

		// Allocate more memory if needed. There must always be space
		// at least for one character to allow terminating the string
		// with '\0'.
		if (pos == size) {
			size *= 2;
			name = xrealloc(name, size);
		}
	}

	return NULL;
}


int
main(int argc, char **argv)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	InitializeCriticalSection(&exit_status_cs);
#endif

	// Set up the progname variable needed for messages.
	tuklib_progname_init(argv);

	// Initialize the file I/O. This makes sure that
	// stdin, stdout, and stderr are something valid.
	// This must be done before we might open any files
	// even indirectly like locale and gettext initializations.
	io_init();

#ifdef ENABLE_SANDBOX
	// Enable such sandboxing that can always be enabled.
	// This requires that progname has been set up.
	// It's also good that io_init() has been called because it
	// might need to do things that the initial sandbox won't allow.
	// Otherwise this should be called as early as possible.
	//
	// NOTE: Calling this before tuklib_gettext_init() means that
	// translated error message won't be available if sandbox
	// initialization fails. However, sandbox_init() shouldn't
	// fail and this order simply feels better.
	sandbox_init();
#endif

	// Set up the locale and message translations.
	tuklib_gettext_init(PACKAGE, LOCALEDIR);

	// Initialize progress message handling. It's not always needed
	// but it's simpler to do this unconditionally.
	message_init();

	// Set hardware-dependent default values. These can be overridden
	// on the command line, thus this must be done before args_parse().
	hardware_init();

	// Parse the command line arguments and get an array of filenames.
	// This doesn't return if something is wrong with the command line
	// arguments. If there are no arguments, one filename ("-") is still
	// returned to indicate stdin.
	args_info args;
	args_parse(&args, argc, argv);

	if (opt_mode != MODE_LIST && opt_robot)
		message_fatal(_("Compression and decompression with --robot "
			"are not supported yet."));

	// Tell the message handling code how many input files there are if
	// we know it. This way the progress indicator can show it.
	if (args.files_name != NULL)
		message_set_files(0);
	else
		message_set_files(args.arg_count);

	// Refuse to write compressed data to standard output if it is
	// a terminal.
	if (opt_mode == MODE_COMPRESS) {
		if (opt_stdout || (args.arg_count == 1
				&& strcmp(args.arg_names[0], "-") == 0)) {
			if (is_tty_stdout()) {
				message_try_help();
				tuklib_exit(E_ERROR, E_ERROR, false);
			}
		}
	}

	// Set up the signal handlers. We don't need these before we
	// start the actual action and not in --list mode, so this is
	// done after parsing the command line arguments.
	//
	// It's good to keep signal handlers in normal compression and
	// decompression modes even when only writing to stdout, because
	// we might need to restore O_APPEND flag on stdout before exiting.
	// In --test mode, signal handlers aren't really needed, but let's
	// keep them there for consistency with normal decompression.
	if (opt_mode != MODE_LIST)
		signals_init();

#ifdef ENABLE_SANDBOX
	// Read-only sandbox can be enabled if we won't create or delete
	// any files:
	//
	//   - --stdout, --test, or --list was used. Note that --test
	//     implies opt_stdout = true but --list doesn't.
	//
	//   - Output goes to stdout because --files or --files0 wasn't used
	//     and no arguments were given on the command line or the
	//     arguments are all "-" (indicating standard input).
	bool to_stdout_only = opt_stdout || opt_mode == MODE_LIST;
	if (!to_stdout_only && args.files_name == NULL) {
		// If all of the filenames provided are "-" (more than one
		// "-" could be specified), then we are only going to be
		// writing to standard output. Note that if no filename args
		// were provided, args.c puts a single "-" in arg_names[0].
		to_stdout_only = true;

		for (unsigned i = 0; i < args.arg_count; ++i) {
			if (strcmp("-", args.arg_names[i]) != 0) {
				to_stdout_only = false;
				break;
			}
		}
	}

	if (to_stdout_only) {
		sandbox_enable_read_only();

		// Allow strict sandboxing if we are processing exactly one
		// file to standard output. This requires that --files or
		// --files0 wasn't specified (an unknown number of filenames
		// could be provided that way).
		if (args.files_name == NULL && args.arg_count == 1)
			sandbox_allow_strict();
	}
#endif

	// coder_run() handles compression, decompression, and testing.
	// list_file() is for --list.
	void (*run)(const char *filename) = &coder_run;
#ifdef HAVE_DECODERS
	if (opt_mode == MODE_LIST)
		run = &list_file;
#endif

	// Process the files given on the command line. Note that if no names
	// were given, args_parse() gave us a fake "-" filename.
	for (unsigned i = 0; i < args.arg_count && !user_abort; ++i) {
		if (strcmp("-", args.arg_names[i]) == 0) {
			// Processing from stdin to stdout. Check that we
			// aren't writing compressed data to a terminal or
			// reading it from a terminal.
			if (opt_mode == MODE_COMPRESS) {
				if (is_tty_stdout())
					continue;
			} else if (is_tty_stdin()) {
				continue;
			}

			// It doesn't make sense to compress data from stdin
			// if we are supposed to read filenames from stdin
			// too (enabled with --files or --files0).
			if (args.files_name == stdin_filename) {
				message_error(_("Cannot read data from "
						"standard input when "
						"reading filenames "
						"from standard input"));
				continue;
			}

			// Replace the "-" with a special pointer, which is
			// recognized by coder_run() and other things.
			// This way error messages get a proper filename
			// string and the code still knows that it is
			// handling the special case of stdin.
			args.arg_names[i] = (char *)stdin_filename;
		}

		// Do the actual compression or decompression.
		run(args.arg_names[i]);
	}

	// If --files or --files0 was used, process the filenames from the
	// given file or stdin. Note that here we don't consider "-" to
	// indicate stdin like we do with the command line arguments.
	if (args.files_name != NULL) {
		// read_name() checks for user_abort so we don't need to
		// check it as loop termination condition.
		while (true) {
			const char *name = read_name(&args);
			if (name == NULL)
				break;

			// read_name() doesn't return empty names.
			assert(name[0] != '\0');
			run(name);
		}

		if (args.files_name != stdin_filename)
			(void)fclose(args.files_file);
	}

#ifdef HAVE_DECODERS
	// All files have now been handled. If in --list mode, display
	// the totals before exiting. We don't have signal handlers
	// enabled in --list mode, so we don't need to check user_abort.
	if (opt_mode == MODE_LIST) {
		assert(!user_abort);
		list_totals();
	}
#endif

#ifndef NDEBUG
	coder_free();
	args_free();
#endif

	// If we have got a signal, raise it to kill the program instead
	// of calling tuklib_exit().
	signals_exit();

	// Make a local copy of exit_status to keep the Windows code
	// thread safe. At this point it is fine if we miss the user
	// pressing C-c and don't set the exit_status to E_ERROR on
	// Windows.
#if defined(_WIN32) && !defined(__CYGWIN__)
	EnterCriticalSection(&exit_status_cs);
#endif

	enum exit_status_type es = exit_status;

#if defined(_WIN32) && !defined(__CYGWIN__)
	LeaveCriticalSection(&exit_status_cs);
#endif

	// Suppress the exit status indicating a warning if --no-warn
	// was specified.
	if (es == E_WARNING && no_warn)
		es = E_SUCCESS;

	tuklib_exit((int)es, E_ERROR, message_verbosity_get() != V_SILENT);
}
