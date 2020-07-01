/*
The MIT License (MIT)

Copyright (c) 2015 Marko Zivanovic

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

#define BOOST_TEST_MODULE semver200_version_tests

#include <boost/test/unit_test.hpp>
#include "semver200.h"

using namespace version;

using v = Semver200_version;

/// Check parsing logic by performing roundtrip - parse string to version object, then
/// generate string from that object and check if it's identical to source.
#define CHECK_RT(SRC) { \
std::stringstream ss; \
ss << v(SRC); \
BOOST_CHECK_EQUAL(ss.str(), SRC); \
}

BOOST_AUTO_TEST_CASE(test_relational_operators) {
	BOOST_CHECK(v("1.0.0-alpha") < v("1.0.0-alpha.1"));
	BOOST_CHECK(v("1.0.0-alpha.1") < v("1.0.0-alpha.beta"));
	BOOST_CHECK(v("1.0.0-alpha.beta") < v("1.0.0-beta"));
	BOOST_CHECK(v("1.0.0-beta") < v("1.0.0-beta.2"));
	BOOST_CHECK(v("1.0.0-beta.2") < v("1.0.0-beta.11"));
	BOOST_CHECK(v("1.0.0-beta.11") < v("1.0.0-rc.1"));
	BOOST_CHECK(v("1.0.0-rc.1") < v("1.0.0"));

	BOOST_CHECK(v("1.0.0+rc.1") == v("1.0.0+rc22"));

	BOOST_CHECK(v("1.0.0+rc.1") != v("1.0.0-rc22"));

	BOOST_CHECK(v("1.0.0") >= v("1.0.0"));
	BOOST_CHECK(v("1.0.0") >= v("0.0.9"));

	BOOST_CHECK(v("2.0.0") > v("1.9.9"));
}

BOOST_AUTO_TEST_CASE(test_ostream_output) {
	CHECK_RT("1.2.3");
	CHECK_RT("1.2.3-alpha");
	CHECK_RT("1.2.3-alpha.1.2.3");
	CHECK_RT("1.2.3+build.1.2.3");
	CHECK_RT("1.2.3-alpha+build.314");
	CHECK_RT("1.2.3-alpha.1+build.314");
	CHECK_RT("1.2.3-alpha.1.2.3+build.314");
}

BOOST_AUTO_TEST_CASE(test_accessors) {
	auto p = v("1.2.3-pre.rel.1+test.build.321");
	BOOST_CHECK_EQUAL(p.major(), 1);
	BOOST_CHECK_EQUAL(p.minor(), 2);
	BOOST_CHECK_EQUAL(p.patch(), 3);
	BOOST_CHECK_EQUAL(p.prerelease(), "pre.rel.1");
	BOOST_CHECK_EQUAL(p.build(), "test.build.321");
}