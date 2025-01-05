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

#pragma once

#include "version.h"

namespace version {

	/// Parse string into Version_data structure according to semantic versioning 2.0.0 rules.
	struct Semver200_parser {
		Version_data parse(const std::string&) const;
	};

	/// Compare Version_data to another using semantic versioning 2.0.0 rules.
	struct Semver200_comparator {
		int compare(const Version_data&, const Version_data&) const;
	};

	/// Implementation of various version modification methods.
	/**
	All methods are non-destructive, i.e. they return a new object with modified properties;
	original object is never modified.
	*/
	struct Semver200_modifier {
		/// Set major version to specified value leaving all other components unchanged..
		Version_data set_major(const Version_data&, const int) const;

		/// Set minor version to specified value leaving all other components unchanged.
		Version_data set_minor(const Version_data&, const int) const;

		/// Set patch version to specified value leaving all other components unchanged.
		Version_data set_patch(const Version_data&, const int) const;

		/// Set pre-release version to specified value leaving all other components unchanged.
		Version_data set_prerelease(const Version_data&, const Prerelease_identifiers&) const;

		/// Set build version to specified value leaving all other components unchanged.
		Version_data set_build(const Version_data&, const Build_identifiers&) const;

		/// Set major version to specified value resetting all lower-priority components to zero/empty values.
		Version_data reset_major(const Version_data&, const int) const;

		/// Set minor version to specified value resetting all lower-priority components to zero/empty values.
		Version_data reset_minor(const Version_data&, const int) const;

		/// Set patch version to specified value resetting all lower-priority components to zero/empty values.
		Version_data reset_patch(const Version_data&, const int) const;

		/// Set pre-release version to specified value resetting all lower-priority components to zero/empty values.
		Version_data reset_prerelease(const Version_data&, const Prerelease_identifiers&) const;

		/// Set build version to specified value.
		Version_data reset_build(const Version_data&, const Build_identifiers&) const;
	};

	/// Concrete version class that binds all semver 2.0.0 functionality together.
	class Semver200_version : public Basic_version<Semver200_parser, Semver200_comparator, Semver200_modifier> {
	public:
		Semver200_version()
			: Basic_version{ Semver200_parser(), Semver200_comparator(), Semver200_modifier() } {}

		Semver200_version(const std::string& v)
			: Basic_version{ v, Semver200_parser(), Semver200_comparator(), Semver200_modifier() } {}
	};

}