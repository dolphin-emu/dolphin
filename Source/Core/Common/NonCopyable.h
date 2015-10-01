// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable
{
protected:
#if defined(_MSC_VER) && _MSC_VER <= 1800
	NonCopyable() {}
	NonCopyable(const NonCopyable&&) {}
	void operator=(const NonCopyable&&) {}
#else
	constexpr NonCopyable() = default;
#endif
	~NonCopyable() = default;

private:
	NonCopyable(NonCopyable&) = delete;
	NonCopyable& operator=(NonCopyable&) = delete;
};

