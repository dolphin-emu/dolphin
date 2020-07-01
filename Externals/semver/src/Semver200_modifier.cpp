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

#include <climits>
#include "semver200.h"

namespace version {

	Version_data Semver200_modifier::set_major(const Version_data& s, const int m) const {
		// if (m < 0) throw Modification_error("major version cannot be less than 0");
		return Version_data{ m, s.minor, s.patch, s.prerelease_ids, s.build_ids };
	}

	Version_data Semver200_modifier::set_minor(const Version_data& s, const int m) const {
		// if (m < 0) throw Modification_error("minor version cannot be less than 0");
		return Version_data{ s.major, m, s.patch, s.prerelease_ids, s.build_ids };
	}

	Version_data Semver200_modifier::set_patch(const Version_data& s, const int p) const {
		// if (p < 0) throw Modification_error("patch version cannot be less than 0");
		return Version_data{ s.major, s.minor, p, s.prerelease_ids, s.build_ids };
	}

	Version_data Semver200_modifier::set_prerelease(const Version_data& s, const Prerelease_identifiers& pr) const {
		return Version_data{ s.major, s.minor, s.patch, pr, s.build_ids };
	}

	Version_data Semver200_modifier::set_build(const Version_data& s, const Build_identifiers& b) const {
		return Version_data{ s.major, s.minor, s.patch, s.prerelease_ids, b };
	}

	Version_data Semver200_modifier::reset_major(const Version_data&, const int m) const {
		// if (m < 0) throw Modification_error("major version cannot be less than 0");
		return Version_data{ m, 0, 0, Prerelease_identifiers{}, Build_identifiers{} };
	}

	Version_data Semver200_modifier::reset_minor(const Version_data& s, const int m) const {
		// if (m < 0) throw Modification_error("minor version cannot be less than 0");
		return Version_data{ s.major, m, 0, Prerelease_identifiers{}, Build_identifiers{} };
	}

	Version_data Semver200_modifier::reset_patch(const Version_data& s, const int p) const {
		// if (p < 0) throw Modification_error("patch version cannot be less than 0");
		return Version_data{ s.major, s.minor, p, Prerelease_identifiers{}, Build_identifiers{} };
	}

	Version_data Semver200_modifier::reset_prerelease(const Version_data& s, const Prerelease_identifiers& pr) const {
		return Version_data{ s.major, s.minor, s.patch, pr, Build_identifiers{} };
	}

	Version_data Semver200_modifier::reset_build(const Version_data& s, const Build_identifiers& b) const {
		return Version_data{ s.major, s.minor, s.patch, s.prerelease_ids, b };
	}
}