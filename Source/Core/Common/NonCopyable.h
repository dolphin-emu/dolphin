// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable
{
protected:
	constexpr NonCopyable() = default;
	~NonCopyable() = default;

private:
	NonCopyable(NonCopyable&) = delete;
	NonCopyable& operator=(NonCopyable&) = delete;
};
