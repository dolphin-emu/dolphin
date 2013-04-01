// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef UTILITY_FUNCS_H_
#define UTILITY_FUNCS_H_

#include <memory>

template <typename T>
std::shared_ptr<T> ToSharedPtr(std::unique_ptr<T> ptr)
{
	return std::move(ptr);
}

// Something they forgot to add to the stdlib that will probably be in C++1x
template <typename T>
std::unique_ptr<T> make_unique()
{
	return std::unique_ptr<T>(new T());
}

template <typename T, typename Arg>
std::unique_ptr<T> make_unique(Arg&& arg)
{
	return std::unique_ptr<T>(new T(std::forward<Arg>(arg)));
}

// It would be nice if msvc supported variadic templates..
template <typename T, typename Arg1, typename Arg2>
std::unique_ptr<T> make_unique(Arg1&& arg1, Arg2&& arg2)
{
	return std::unique_ptr<T>(new T(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2)));
}

template <typename T, typename Arg1, typename Arg2, typename Arg3>
std::unique_ptr<T> make_unique(Arg1&& arg1, Arg2&& arg2, Arg3&& arg3)
{
	return std::unique_ptr<T>(new T(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), std::forward<Arg3>(arg3)));
}

template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
std::unique_ptr<T> make_unique(Arg1&& arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4)
{
	return std::unique_ptr<T>(new T(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2),
		std::forward<Arg3>(arg3), std::forward<Arg4>(arg4)));
}

#endif
