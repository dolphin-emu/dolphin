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

#define BOOST_TEST_MODULE semver200_comparator_tests

#include <boost/test/unit_test.hpp>
#include "semver200.h"

using namespace version;

Semver200_comparator c;
Semver200_parser p;

inline int compare(const std::string& l, const std::string& r) {
	auto lv = p.parse(l);
	auto rv = p.parse(r);
	return c.compare(lv, rv);
}

#define GT(L, R) BOOST_CHECK(compare(L, R) >  0)
#define GE(L, R) BOOST_CHECK(compare(L, R) >= 0)
#define LT(L, R) BOOST_CHECK(compare(L, R) <  0)
#define LE(L, R) BOOST_CHECK(compare(L, R) <= 0)
#define EQ(L, R) BOOST_CHECK(compare(L, R) == 0)

// check normal precedence
BOOST_AUTO_TEST_CASE(compare_normal) {
	EQ("1.2.3", "1.2.3");

	GT("0.0.2", "0.0.1");
	GT("0.2.0", "0.0.3");
	GT("0.2.0", "0.1.3");
	GT("2.0.0", "0.0.1");
	GT("2.0.0", "0.3.1");
	GT("2.0.0", "1.3.1");
}

// normal and prerel precedence
BOOST_AUTO_TEST_CASE(compare_normal_prerel) {
	GT("1.0.0", "1.0.0-alpha");
	GT("1.0.0", "1.0.0-99");
	GT("1.0.0", "1.0.0-ZZ");
}

// same prerels
BOOST_AUTO_TEST_CASE(compare_equal_prerels) {
	EQ("1.0.0-alpha", "1.0.0-alpha");
	EQ("1.0.0-alpha.1", "1.0.0-alpha.1");
	EQ("1.0.0-1", "1.0.0-1");
}

// prerels precedence with numeric id
BOOST_AUTO_TEST_CASE(compare_numeric_prerels) {
	GT("1.0.0-1", "1.0.0-0");
	GT("1.0.0-10", "1.0.0-1");
	GT("1.0.0-alpha.3", "1.0.0-alpha.1");
}

// prerels precedence with alphanum id
BOOST_AUTO_TEST_CASE(compare_alphanum_prerels) {
	GT("1.0.0-1", "1.0.0-0");
	GT("1.0.0-Z", "1.0.0-A");
	GT("1.0.0-Z", "1.0.0-1");
	GT("1.0.0-alpha-3", "1.0.0-alpha-1");
	GT("1.0.0-alpha-3", "1.0.0-alpha-100");
}

// prerels precedence misc
BOOST_AUTO_TEST_CASE(compare_misc_prerels) {
	LT("1.0.0-alpha", "1.0.0-alpha.1");
	LT("1.0.0-alpha.1", "1.0.0-alpha.beta");
	LT("1.0.0-alpha.beta", "1.0.0-beta");
	LT("1.0.0-beta", "1.0.0-beta.2");
	LT("1.0.0-beta.2", "1.0.0-beta.11");
	LT("1.0.0-beta.11", "1.0.0-rc.1");
	LT("1.0.0-rc.1", "1.0.0");
}

// equal precedence based on build
BOOST_AUTO_TEST_CASE(compare_build) {
	EQ("1.0.0", "1.0.0+build.1.2.3");
	EQ("1.0.0+ZZZ", "1.0.0+build.1.2.3");
	EQ("1.0.0+100", "1.0.0+200");
}
