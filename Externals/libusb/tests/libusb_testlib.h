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

#ifndef LIBUSB_TESTLIB_H
#define LIBUSB_TESTLIB_H

#include <stdio.h>

#if !defined(bool)
#define bool int
#endif
#if !defined(true)
#define true (1 == 1)
#endif
#if !defined(false)
#define false (!true)
#endif

/** Values returned from a test function to indicate test result */
typedef enum {
	/** Indicates that the test ran successfully. */
	TEST_STATUS_SUCCESS,
	/** Indicates that the test failed one or more test. */
	TEST_STATUS_FAILURE,
	/** Indicates that an unexpected error occurred. */
	TEST_STATUS_ERROR,
	/** Indicates that the test can't be run. For example this may be
	* due to no suitable device being connected to perform the tests.*/
	TEST_STATUS_SKIP
} libusb_testlib_result;

/**
 * Context for test library functions
 */
typedef struct {
	char ** test_names;
	int test_count;
	bool list_tests;
	bool verbose;
	int old_stdout;
	int old_stderr;
	FILE* output_file;
	int null_fd;
} libusb_testlib_ctx;

/**
 * Logs some test information or state
 */
void libusb_testlib_logf(libusb_testlib_ctx * ctx, 
                          const char* fmt, ...);

/**
 * Function pointer for a libusb test function.
 *
 * Should return TEST_STATUS_SUCCESS on success or another TEST_STATUS value.
 */
typedef libusb_testlib_result
(*libusb_testlib_test_function)(libusb_testlib_ctx * ctx);

/**
 * Structure holding a test description.
 */
typedef struct {
	/** Human readable name of the test. */
	const char * name;
	/** The test library will call this function to run the test. */
	libusb_testlib_test_function function;
} libusb_testlib_test;

/**
 * Value to use at the end of a test array to indicate the last
 * element.
 */
#define LIBUSB_NULL_TEST {NULL, NULL}

/**
 * Runs the tests provided.
 *
 * Before running any tests argc and argv will be processed
 * to determine the mode of operation.
 *
 * \param argc The argc from main
 * \param argv The argv from main
 * \param tests A NULL_TEST terminated array of tests
 * \return 0 on success, non-zero on failure
 */
int libusb_testlib_run_tests(int argc,
                              char ** argv,
                              const libusb_testlib_test * tests);

#endif //LIBUSB_TESTLIB_H
