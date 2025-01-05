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

#include <algorithm>
#include <functional>
#include <map>
#include "semver200.h"

using namespace std;

namespace version {

	namespace {

		// Compare normal version identifiers.
		int compare_normal(const Version_data& l, const Version_data& r) {
			if (l.major > r.major) return 1;
			if (l.major < r.major) return -1;
			if (l.minor > r.minor) return 1;
			if (l.minor < r.minor) return -1;
			if (l.patch > r.patch) return 1;
			if (l.patch < r.patch) return -1;
			return 0;
		}

		// Compare alphanumeric prerelease identifiers.
		inline int cmp_alnum_prerel_ids(const string& l, const string& r) {
			auto cmp = l.compare(r);
			if (cmp == 0) {
				return cmp;
			} else {
				return cmp > 0 ? 1 : -1;
			}
		}

		// Compare numeric prerelease identifiers.
		inline int cmp_num_prerel_ids(const string& l, const string& r) {
			long long li = stoll(l);
			long long ri = stoll(r);
			if (li == ri) return 0;
			return li > ri ? 1 : -1;
		}

		using Prerel_type_pair = pair<Id_type, Id_type>;
		using Prerel_id_comparator = function<int(const string&, const string&)>;
		const map<Prerel_type_pair, Prerel_id_comparator> comparators = {
			{ { Id_type::alnum, Id_type::alnum }, cmp_alnum_prerel_ids },
			{ { Id_type::alnum, Id_type::num }, [](const string&, const string&) {return 1;} },
			{ { Id_type::num, Id_type::alnum }, [](const string&, const string&) {return -1;} },
			{ { Id_type::num, Id_type::num }, cmp_num_prerel_ids }
		};

		// Compare prerelease identifiers based on their types.
		inline int compare_prerel_identifiers(const Prerelease_identifier& l, const Prerelease_identifier& r) {
			auto cmp = comparators.at({ l.second, r.second });
			return cmp(l.first, r.first);
		}

		inline int cmp_rel_prerel(const Prerelease_identifiers& l, const Prerelease_identifiers& r) {
			if (l.empty() && !r.empty()) return 1;
			if (r.empty() && !l.empty()) return -1;
			return 0;
		}
	}

	int Semver200_comparator::compare(const Version_data& l, const Version_data& r) const {
		// Compare normal version components.
		int cmp = compare_normal(l, r);
		if (cmp != 0) return cmp;

		// Compare if one version is release and the other prerelease - release is always higher.
		cmp = cmp_rel_prerel(l.prerelease_ids, r.prerelease_ids);
		if (cmp != 0) return cmp;

		// Compare prerelease by looking at each identifier: numeric ones are compared as numbers,
		// alphanum as ASCII strings.
		auto shorter = min(l.prerelease_ids.size(), r.prerelease_ids.size());
		for (size_t i = 0; i < shorter; i++) {
			cmp = compare_prerel_identifiers(l.prerelease_ids[i], r.prerelease_ids[i]);
			if (cmp != 0) return cmp;
		}

		// Prerelease identifiers are the same, to the length of the shorter version string;
		// if they are the same length, then versions are equal, otherwise, longer one wins.
		if (l.prerelease_ids.size() == r.prerelease_ids.size()) return 0;
		return l.prerelease_ids.size() > r.prerelease_ids.size() ? 1 : -1;
	}

}
