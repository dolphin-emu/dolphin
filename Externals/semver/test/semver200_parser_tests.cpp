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

#define BOOST_TEST_MODULE semver200_parser_tests

#include "semver200_parser_util.h"

using namespace version;
using namespace std;

// normal must have major, minor and patch version
BOOST_AUTO_TEST_CASE(parse_normal_ids) {
	CHECK_NORMALS("0.0.0", 0, 0, 0);
	CHECK_PARSE_ERROR("1");
	CHECK_PARSE_ERROR("1.1");
}

// normal versions must be non negative integers
BOOST_AUTO_TEST_CASE(parse_normal_positive_ints) {
	CHECK_NORMALS("1.2.3", 1, 2, 3);
	CHECK_NORMALS("65535.65534.65533", 65535, 65534, 65533);
	CHECK_PARSE_ERROR("-1.0.0");
	CHECK_PARSE_ERROR("1.-1.0");
	CHECK_PARSE_ERROR("1.1.-1");
	CHECK_PARSE_ERROR("a.0.0");
	CHECK_PARSE_ERROR("1.a.0");
	CHECK_PARSE_ERROR("1.0.a");
}

// normal versions must not have leading 0
BOOST_AUTO_TEST_CASE(parse_normal_leading_0) {
	CHECK_PARSE_ERROR("01.0.0");
	CHECK_PARSE_ERROR("1.01.0");
	CHECK_PARSE_ERROR("1.0.01");
}

// prerel contains one or more dot-separated ids with distinct numeric and mixed ids
BOOST_AUTO_TEST_CASE(parse_prerel_ids_types) {
	CHECK_PREREL("1.2.3-test", 1, 2, 3, Prerelease_identifiers({ {"test", A} }));
	CHECK_PREREL("1.2.3-321", 1, 2, 3, Prerelease_identifiers({ {"321", N} }));
	CHECK_PREREL("1.2.3-test.1", 1, 2, 3, Prerelease_identifiers({ {"test", A},{"1",N} }));
	CHECK_PREREL("1.2.3-1.test", 1, 2, 3, Prerelease_identifiers({ {"1", N},{"test", A} }));
	CHECK_PREREL("1.2.3-test.123456", 1, 2, 3, Prerelease_identifiers({ {"test", A},{"123456", N} }));
	CHECK_PREREL("1.2.3-123456.test", 1, 2, 3, Prerelease_identifiers({ {"123456", N},{"test", A} }));
	CHECK_PREREL("1.2.3-1.a.22.bb.333.ccc.4444.dddd.55555.fffff", 1, 2, 3, Prerelease_identifiers({ {"1", N},{"a", A},
	{"22", N},{"bb", A},{"333", N },{"ccc", A} ,{"4444", N},{"dddd", A} ,{"55555", N},{"fffff", A} }));
}

// prerel ids contain only alphanumerics and hyphen
BOOST_AUTO_TEST_CASE(parse_prerel_legal_chars) {
	CHECK_PREREL("1.2.3-test-1-2-3-CAP", 1, 2, 3, Prerelease_identifiers({ { "test-1-2-3-CAP", A } }));
	CHECK_PARSE_ERROR("1.2.3-test#1");
	CHECK_PARSE_ERROR("1.2.3-test.©2015");
	CHECK_PARSE_ERROR("1.2.3-ћирилица-1");
}

// prerel ids may not be empty
BOOST_AUTO_TEST_CASE(parse_prerel_empty_ids) {
	CHECK_PARSE_ERROR("1.2.3-");
	CHECK_PARSE_ERROR("1.2.3-test.");
	CHECK_PARSE_ERROR("1.2.3-test..");
	CHECK_PARSE_ERROR("1.2.3-test..1");
}

// prerel numeric ids must not have leading 0
BOOST_AUTO_TEST_CASE(parse_prerel_num_ids_no_leading_0) {
	CHECK_PARSE_ERROR("1.2.3-01");
	CHECK_PARSE_ERROR("1.2.3-test.0023");
	CHECK_PREREL("1.2.3-test.01a", 1, 2, 3, Prerelease_identifiers({ { "test", A },{ "01a", A } }));
	CHECK_PREREL("1.2.3-test.01-s", 1, 2, 3, Prerelease_identifiers({ { "test", A },{ "01-s", A } }));
}

// build contains one or more dot separated ids
BOOST_AUTO_TEST_CASE(parse_build_ids) {
	CHECK_BUILD("1.2.3+test", 1, 2, 3, Build_identifiers({ "test" }));
	CHECK_BUILD("1.2.3+321", 1, 2, 3, Build_identifiers({ "321" }));
	CHECK_BUILD("1.2.3+test.1", 1, 2, 3, Build_identifiers({ "test","1" }));
	CHECK_BUILD("1.2.3+1.test", 1, 2, 3, Build_identifiers({ "1","test" }));
	CHECK_BUILD("1.2.3+test.123456", 1, 2, 3, Build_identifiers({ "test","123456" }));
	CHECK_BUILD("1.2.3+123456.test", 1, 2, 3, Build_identifiers({ "123456","test" }));
	CHECK_BUILD("1.2.3+1.a.22.bb.333.ccc.4444.dddd.55555.fffff", 1, 2, 3, Build_identifiers({ "1","a","22","bb",
		"333","ccc","4444","dddd","55555","fffff" }));
}

// build ids contain only alphanumerics and hyphen
BOOST_AUTO_TEST_CASE(parse_build_legal_chars) {
	CHECK_BUILD("1.2.3+test-1-2-3-CAP", 1, 2, 3, Build_identifiers({ "test-1-2-3-CAP" }));
	CHECK_PARSE_ERROR("1.2.3+test#1");
	CHECK_PARSE_ERROR("1.2.3+test.©2015");
	CHECK_PARSE_ERROR("1.2.3+ћирилица-1");
}

// build ids may not be empty
BOOST_AUTO_TEST_CASE(parse_build_empty_ids) {
	CHECK_PARSE_ERROR("1.2.3+");
	CHECK_PARSE_ERROR("1.2.3+test.");
	CHECK_PARSE_ERROR("1.2.3+test..");
	CHECK_PARSE_ERROR("1.2.3+test..1");
}

// optional prerel must come after patch and build after prerel
BOOST_AUTO_TEST_CASE(parse_prerel_build_order) {
	CHECK_PREREL_BUILD("1.2.3-r4+b5", 1, 2, 3, Prerelease_identifiers({ {"r4",A} }), Build_identifiers({ "b5" }));
	CHECK_PREREL_BUILD("1.2.3+b4-r5", 1, 2, 3, no_rel_ids, Build_identifiers({ "b4-r5" }));
}

// check some corner cases
BOOST_AUTO_TEST_CASE(parse_corner_cases) {
	CHECK_PARSE_ERROR("1.2.3-r4.+b5");
	CHECK_PARSE_ERROR("1.2.3-r4+b5.");

	CHECK_PREREL_BUILD("1.2.3-alpha+build.314", 1, 2, 3, Prerelease_identifiers({ {"alpha", A} }),
		Build_identifiers({ "build","314" }));

}
