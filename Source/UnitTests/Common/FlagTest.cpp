// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <thread>

#include "Common/Flag.h"

using Common::Flag;

TEST(Flag, Simple)
{
	Flag f;
	EXPECT_FALSE(f.IsSet());

	f.Set();
	EXPECT_TRUE(f.IsSet());

	f.Clear();
	EXPECT_FALSE(f.IsSet());

	f.Set(false);
	EXPECT_FALSE(f.IsSet());

	Flag f2(true);
	EXPECT_TRUE(f2.IsSet());
}

TEST(Flag, MultiThreaded)
{
	Flag f;
	int count = 0;
	const int ITERATIONS_COUNT = 100000;

	auto setter = [&f]() {
		for (int i = 0; i < ITERATIONS_COUNT; ++i)
		{
			while (f.IsSet());
			f.Set();
		}
	};

	auto clearer = [&f, &count]() {
		for (int i = 0; i < ITERATIONS_COUNT; ++i)
		{
			while (!f.IsSet());
			count++;
			f.Clear();
		}
	};

	std::thread setter_thread(setter);
	std::thread clearer_thread(clearer);

	setter_thread.join();
	clearer_thread.join();

	EXPECT_EQ(ITERATIONS_COUNT, count);
}
