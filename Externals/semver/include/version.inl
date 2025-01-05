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

#include <sstream>
#include "version.h"

namespace version {

	namespace {

		/// Utility function to splice all vector elements to output stream, using designated separator 
		/// between elements and function object for getting values from vector elements.
		template<typename T, typename F>
		std::ostream& splice(std::ostream& os, const std::vector<T>& vec, const std::string& sep, F read) {
			if (!vec.empty()) {
				for (auto it = vec.cbegin(); it < vec.cend() - 1; ++it) {
					os << read(*it) << sep;
				}
				os << read(*vec.crbegin());
			}
			return os;
		}
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier>::Basic_version(Parser p, Comparator c, Modifier m)
		: parser_(p), comparator_(c), modifier_(m), ver_(parser_.parse("0.0.0")) {}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier>::Basic_version(const std::string& v, Parser p, Comparator c, Modifier m)
		: parser_(p), comparator_(c), modifier_(m), ver_(parser_.parse(v)) {}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier>::Basic_version(const Version_data& v, Parser p, Comparator c, Modifier m)
		: parser_(p), comparator_(c), modifier_(m), ver_(v) {}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier>::Basic_version(const Basic_version<Parser, Comparator, Modifier>&) = default;

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier>& Basic_version<Parser, Comparator, Modifier>::operator=(
		const Basic_version<Parser, Comparator, Modifier>&) = default;

	template<typename Parser, typename Comparator, typename Modifier>
	int Basic_version<Parser, Comparator, Modifier>::major_ver() const {
		return ver_.major;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	int Basic_version<Parser, Comparator, Modifier>::minor_ver() const {
		return ver_.minor;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	int Basic_version<Parser, Comparator, Modifier>::patch() const {
		return ver_.patch;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	const std::string Basic_version<Parser, Comparator, Modifier>::prerelease() const {
		std::stringstream ss;
		splice(ss, ver_.prerelease_ids, ".", [](const auto& id) { return id.first;});
		return ss.str();
	}

	template<typename Parser, typename Comparator, typename Modifier>
	const std::string Basic_version<Parser, Comparator, Modifier>::build() const {
		std::stringstream ss;
		splice(ss, ver_.build_ids, ".", [](const auto& id) { return id;});
		return ss.str();
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::set_major(const int m) const {
		return Basic_version<Parser, Comparator, Modifier>(modifier_.set_major(ver_, m), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::set_minor(const int m) const {
		return Basic_version<Parser, Comparator, Modifier>(modifier_.set_minor(ver_, m), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::set_patch(const int p) const {
		return Basic_version<Parser, Comparator, Modifier>(modifier_.set_patch(ver_, p), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::set_prerelease(const std::string& pr) const {
		auto vd = parser_.parse("0.0.0-" + pr);
		return Basic_version<Parser, Comparator, Modifier>(modifier_.set_prerelease(ver_, vd.prerelease_ids), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::set_build(const std::string& b) const {
		auto vd = parser_.parse("0.0.0+" + b);
		return Basic_version<Parser, Comparator, Modifier>(modifier_.set_build(ver_, vd.build_ids), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::reset_major(const int m) const {
		return Basic_version<Parser, Comparator, Modifier>(modifier_.reset_major(ver_, m), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::reset_minor(const int m) const {
		return Basic_version<Parser, Comparator, Modifier>(modifier_.reset_minor(ver_, m), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::reset_patch(const int p) const {
		return Basic_version<Parser, Comparator, Modifier>(modifier_.reset_patch(ver_, p), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::reset_prerelease(const std::string& pr) const {
		std::string ver = "0.0.0-" + pr;
		auto vd = parser_.parse(ver);
		return Basic_version<Parser, Comparator, Modifier>(modifier_.reset_prerelease(ver_, vd.prerelease_ids), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::reset_build(const std::string& b) const {
		std::string ver = "0.0.0+" + b;
		auto vd = parser_.parse(ver);
		return Basic_version<Parser, Comparator, Modifier>(modifier_.reset_build(ver_, vd.build_ids), parser_, comparator_, modifier_);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::inc_major(const int i) const {
		return reset_major(ver_.major + i);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::inc_minor(const int i) const {
		return reset_minor(ver_.minor + i);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	Basic_version<Parser, Comparator, Modifier> Basic_version<Parser, Comparator, Modifier>::inc_patch(const int i) const {
		return reset_patch(ver_.patch + i);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	bool operator<(const Basic_version<Parser, Comparator, Modifier>& l,
		const Basic_version<Parser, Comparator, Modifier>& r) {
		return l.comparator_.compare(l.ver_, r.ver_) == -1;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	bool operator==(const Basic_version<Parser, Comparator, Modifier>& l,
		const Basic_version<Parser, Comparator, Modifier>& r) {
		return l.comparator_.compare(l.ver_, r.ver_) == 0;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	std::ostream& operator<<(std::ostream& os,
		const Basic_version<Parser, Comparator, Modifier>& v) {
		os << v.ver_.major << "." << v.ver_.minor << "." << v.ver_.patch;
		std::string prl = v.prerelease();
		if (!prl.empty()) {
			os << "-" << prl;
		}
		std::string bld = v.build();
		if (!bld.empty()) {
			os << "+" << bld;
		}
		return os;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	inline bool operator!=(const Basic_version<Parser, Comparator, Modifier>& l,
		const Basic_version<Parser, Comparator, Modifier>& r) {
		return !(l == r);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	inline bool operator>(const Basic_version<Parser, Comparator, Modifier>& l,
		const Basic_version<Parser, Comparator, Modifier>& r) {
		return r < l;
	}

	template<typename Parser, typename Comparator, typename Modifier>
	inline bool operator>=(const Basic_version<Parser, Comparator, Modifier>& l,
		const Basic_version<Parser, Comparator, Modifier>& r) {
		return !(l < r);
	}

	template<typename Parser, typename Comparator, typename Modifier>
	inline bool operator<=(const Basic_version<Parser, Comparator, Modifier>& l,
		const Basic_version<Parser, Comparator, Modifier>& r) {
		return !(l > r);
	}
}
