
/*
The MIT License (MIT)

Copyright (c) 2017 Marko Zivanovic

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define BOOST_TEST_MODULE semver200_modifier_tests

#include <climits>
#include <boost/test/unit_test.hpp>
#include "semver200.h"

using namespace version;

Semver200_comparator c;
Semver200_parser p;
Semver200_modifier m;

#define CHECK_SRC { \
	BOOST_CHECK(v.major() == 1); \
	BOOST_CHECK(v.minor() == 2); \
	BOOST_CHECK(v.patch() == 3); \
	BOOST_CHECK(v.prerelease() == "pre.rel.0"); \
	BOOST_CHECK(v.build() == "build.no.321"); \
}

BOOST_AUTO_TEST_CASE(set_major) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.set_major(2);

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 2);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 3);
	BOOST_CHECK(v2.prerelease() == "pre.rel.0");
	BOOST_CHECK(v2.build() == "build.no.321");

	// Check invalid values
	BOOST_CHECK_THROW(v.set_major(-1), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(set_minor) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.set_minor(3);

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 3);
	BOOST_CHECK(v2.patch() == 3);
	BOOST_CHECK(v2.prerelease() == "pre.rel.0");
	BOOST_CHECK(v2.build() == "build.no.321");

	// Check invalid values
	BOOST_CHECK_THROW(v.set_minor(-1), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(set_patch) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.set_patch(4);

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 4);
	BOOST_CHECK(v2.prerelease() == "pre.rel.0");
	BOOST_CHECK(v2.build() == "build.no.321");

	// Check invalid values
	BOOST_CHECK_THROW(v.set_patch(-1), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(set_prerelease) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.set_prerelease("alpha.1");

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 3);
	BOOST_CHECK(v2.prerelease() == "alpha.1");
	BOOST_CHECK(v2.build() == "build.no.321");

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(set_build) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.set_build("b123");

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 3);
	BOOST_CHECK(v2.prerelease() == "pre.rel.0");
	BOOST_CHECK(v2.build() == "b123");

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(reset_major) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.reset_major(2);

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 2);
	BOOST_CHECK(v2.minor() == 0);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check invalid values
	BOOST_CHECK_THROW(v.reset_major(-1), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(reset_minor) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.reset_minor(3);

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 3);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check invalid values
	BOOST_CHECK_THROW(v.reset_minor(-1), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(reset_patch) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.reset_patch(4);

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 4);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check invalid values
	BOOST_CHECK_THROW(v.reset_patch(-1), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(reset_prerelease) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.reset_prerelease("alpha.1");

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 3);
	BOOST_CHECK(v2.prerelease() == "alpha.1");
	BOOST_CHECK(v2.build() == "");

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(reset_build) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");
	auto v2 = v.reset_build("b123");

	// Check if setter works as expected
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 3);
	BOOST_CHECK(v2.prerelease() == "pre.rel.0");
	BOOST_CHECK(v2.build() == "b123");

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(inc_major) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");

	// Check default increment
	auto v2 = v.inc_major();
	BOOST_CHECK(v2.major() == 2);
	BOOST_CHECK(v2.minor() == 0);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check non-default increment
	auto v2 = v.inc_major(3);
	BOOST_CHECK(v2.major() == 4);
	BOOST_CHECK(v2.minor() == 0);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check negative increment
	v2 = v.inc_major(-1);
	BOOST_CHECK(v2.major() == 0);
	BOOST_CHECK(v2.minor() == 0);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check too negative increment
	BOOST_CHECK_THROW(v.inc_major(-2), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(inc_minor) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");

	// Check default increment
	auto v2 = v.inc_minor();
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 3);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check non-default increment
	auto v2 = v.inc_minor(3);
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 5);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check negative increment
	v2 = v.inc_minor(-1);
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 1);
	BOOST_CHECK(v2.patch() == 0);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check too negative increment
	BOOST_CHECK_THROW(v.inc_minor(-3), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}

BOOST_AUTO_TEST_CASE(inc_patch) {
	Semver200_version v("1.2.3-pre.rel.0+build.no.321");

	// Check default increment
	auto v2 = v.inc_patch();
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 4);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check default increment
	auto v2 = v.inc_patch(3);
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 6);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check negative increment
	v2 = v.inc_patch(-1);
	BOOST_CHECK(v2.major() == 1);
	BOOST_CHECK(v2.minor() == 2);
	BOOST_CHECK(v2.patch() == 2);
	BOOST_CHECK(v2.prerelease() == "");
	BOOST_CHECK(v2.build() == "");

	// Check too negative increment
	BOOST_CHECK_THROW(v.inc_patch(-4), version::Modification_error);

	// Check source version is unaffected
	CHECK_SRC
}
