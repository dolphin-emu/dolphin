/*
 * libusb test library helper functions
 * Copyright © 2012 Toby Gray <toby.gray@realvnc.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libusb_testlib.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#if !defined(_WIN32_WCE)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#if defined(_WIN32_WCE)
// No support for selective redirection of STDOUT on WinCE.
#define DISABLE_STDOUT_REDIRECTION
#define STDOUT_FILENO 1
#elif defined(_WIN32)
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define open _open
#define close _close
#define fdopen _fdopen
#define NULL_PATH "nul"
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#else
#include <unistd.h>
#define NULL_PATH "/dev/null"
#endif
#define INVALID_FD -1
#define IGNORE_RETVAL(expr) do { (void)(expr); } while(0)

/**
 * Converts a test result code into a human readable string.
 */
static const char* test_result_to_str(libusb_testlib_result result)
{
	switch (result) {
	case TEST_STATUS_SUCCESS:
		return "Success";
	case TEST_STATUS_FAILURE:
		return "Failure";
	case TEST_STATUS_ERROR:
		return "Error";
	case TEST_STATUS_SKIP:
		return "Skip";
	default:
		return "Unknown";
	}
}

static void print_usage(int argc, char ** argv)
{
	printf("Usage: %s [-l] [-v] [<test_name> ...]\n",
		argc > 0 ? argv[0] : "test_*");
	printf("   -l   List available tests\n");
	printf("   -v   Don't redirect STDERR/STDOUT during tests\n");
}

static void cleanup_test_output(libusb_testlib_ctx * ctx)
{
#ifndef DISABLE_STDOUT_REDIRECTION
	if (!ctx->verbose) {
		if (ctx->old_stdout != INVALID_FD) {
			IGNORE_RETVAL(dup2(ctx->old_stdout, STDOUT_FILENO));
			ctx->old_stdout = INVALID_FD;
		}
		if (ctx->old_stderr != INVALID_FD) {
			IGNORE_RETVAL(dup2(ctx->old_stderr, STDERR_FILENO));
			ctx->old_stderr = INVALID_FD;
		}
		if (ctx->null_fd != INVALID_FD) {
			close(ctx->null_fd);
			ctx->null_fd = INVALID_FD;
		}
		if (ctx->output_file != stdout) {
			fclose(ctx->output_file);
			ctx->output_file = stdout;
		}
	}
#endif
}

/**
 * Setup test output handles
 * \return zero on success, non-zero on failure
 */
static int setup_test_output(libusb_testlib_ctx * ctx)
{
#ifndef DISABLE_STDOUT_REDIRECTION
	/* Stop output to stdout and stderr from being displayed if using non-verbose output */
	if (!ctx->verbose) {
		/* Keep a copy of STDOUT and STDERR */
		ctx->old_stdout = dup(STDOUT_FILENO);
		if (ctx->old_stdout < 0) {
			ctx->old_stdout = INVALID_FD;
			printf("Failed to duplicate stdout handle: %d\n", errno);
			return 1;
		}
		ctx->old_stderr = dup(STDERR_FILENO);
		if (ctx->old_stderr < 0) {
			ctx->old_stderr = INVALID_FD;
			cleanup_test_output(ctx);
			printf("Failed to duplicate stderr handle: %d\n", errno);
			return 1;
		}
		/* Redirect STDOUT_FILENO and STDERR_FILENO to /dev/null or "nul"*/
		ctx->null_fd = open(NULL_PATH, O_WRONLY);
		if (ctx->null_fd < 0) {
			ctx->null_fd = INVALID_FD;
			cleanup_test_output(ctx);
			printf("Failed to open null handle: %d\n", errno);
			return 1;
		}
		if ((dup2(ctx->null_fd, STDOUT_FILENO) < 0) ||
			(dup2(ctx->null_fd, STDERR_FILENO) < 0)) {
				cleanup_test_output(ctx);
				return 1;
		}
		ctx->output_file = fdopen(ctx->old_stdout, "w");
		if (!ctx->output_file) {
			ctx->output_file = stdout;
			cleanup_test_output(ctx);
			printf("Failed to open FILE for output handle: %d\n", errno);
			return 1;
		}
	}
#endif
	return 0;
}

void libusb_testlib_logf(libusb_testlib_ctx * ctx,
	const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(ctx->output_file, fmt, va);
	va_end(va);
	fprintf(ctx->output_file, "\n");
	fflush(ctx->output_file);
}

int libusb_testlib_run_tests(int argc,
	char ** argv,
	const libusb_testlib_test * tests)
{
	int run_count = 0;
	int idx = 0;
	int pass_count = 0;
	int fail_count = 0;
	int error_count = 0;
	int skip_count = 0;
	int r, j;
	size_t arglen;
	libusb_testlib_result test_result;
	libusb_testlib_ctx ctx;

	/* Setup default mode of operation */
	ctx.test_names = NULL;
	ctx.test_count = 0;
	ctx.list_tests = false;
	ctx.verbose = false;
	ctx.old_stdout = INVALID_FD;
	ctx.old_stderr = INVALID_FD;
	ctx.output_file = stdout;
	ctx.null_fd = INVALID_FD;

	/* Parse command line options */
	if (argc >= 2) {
		for (j = 1; j < argc; j++) {
			arglen = strlen(argv[j]);
			if ( ((argv[j][0] == '-') || (argv[j][0] == '/')) &&
				arglen >=2 ) {
					switch (argv[j][1]) {
					case 'l':
						ctx.list_tests = true;
						break;
					case 'v':
						ctx.verbose = true;
						break;
					default:
						printf("Unknown option: '%s'\n", argv[j]);
						print_usage(argc, argv);
						return 1;
					}
			} else {
				/* End of command line options, remaining must be list of tests to run */
				ctx.test_names = argv + j;
				ctx.test_count = argc - j;
				break;
			}
		}
	}

	/* Validate command line options */
	if (ctx.test_names && ctx.list_tests) {
		printf("List of tests requested but test list provided\n");
		print_usage(argc, argv);
		return 1;
	}

	/* Setup test log output */
	r = setup_test_output(&ctx);
	if (r != 0)
		return r;  

	/* Act on any options not related to running tests */
	if (ctx.list_tests) {
		while (tests[idx].function != NULL) {
			libusb_testlib_logf(&ctx, tests[idx].name);
			++idx;
		}
		cleanup_test_output(&ctx);
		return 0;
	}

	/* Run any requested tests */
	while (tests[idx].function != NULL) {
		const libusb_testlib_test * test = &tests[idx];
		++idx;
		if (ctx.test_count > 0) {
			/* Filtering tests to run, check if this is one of them */
			int i;
			for (i = 0; i < ctx.test_count; ++i) {
				if (strcmp(ctx.test_names[i], test->name) == 0)
					/* Matches a requested test name */
					break;
			}
			if (i >= ctx.test_count) {
				/* Failed to find a test match, so do the next loop iteration */
				continue;
			}
		}
		libusb_testlib_logf(&ctx,
			"Starting test run: %s...", test->name);
		test_result = test->function(&ctx);
		libusb_testlib_logf(&ctx,
			"%s (%d)",
			test_result_to_str(test_result), test_result);
		switch (test_result) {
		case TEST_STATUS_SUCCESS: pass_count++; break;
		case TEST_STATUS_FAILURE: fail_count++; break;
		case TEST_STATUS_ERROR: error_count++; break;
		case TEST_STATUS_SKIP: skip_count++; break;
		}
		++run_count;
	}
	libusb_testlib_logf(&ctx, "---");
	libusb_testlib_logf(&ctx, "Ran %d tests", run_count);
	libusb_testlib_logf(&ctx, "Passed %d tests", pass_count);
	libusb_testlib_logf(&ctx, "Failed %d tests", fail_count);
	libusb_testlib_logf(&ctx, "Error in %d tests", error_count);
	libusb_testlib_logf(&ctx, "Skipped %d tests", skip_count);

	cleanup_test_output(&ctx);
	return pass_count != run_count;
}
