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

#include <cmath>
#include <iostream>

#include "StringUtil.h"
#include "MathUtil.h"
#include "PowerPC/PowerPC.h"
#include "HW/SI_DeviceGCController.h"

void AudioJitTests();

using namespace std;
int fail_count = 0;

#define EXPECT_TRUE(a) \
	if (!a) { \
		cout << "FAIL (" << __FUNCTION__ << "): " << #a << " is false" << endl; \
		cout << "Value: " << a << endl << "Expected: true" << endl; \
		fail_count++; \
	}

#define EXPECT_FALSE(a) \
	if (a) { \
		cout << "FAIL (" << __FUNCTION__ << "): " << #a << " is true" << endl; \
		cout << "Value: " << a << endl << "Expected: false" << endl; \
		fail_count++; \
	}

#define EXPECT_EQ(a, b) \
	if ((a) != (b)) { \
		cout << "FAIL (" << __FUNCTION__ << "): " << #a << " is not equal to " << #b << endl; \
		cout << "Actual: " << a << endl << "Expected: " << b << endl; \
		fail_count++; \
	}

void CoreTests()
{
}

void MathTests()
{
	// Tests that our fp classifier is correct.
	EXPECT_EQ(MathUtil::ClassifyDouble(1.0),        MathUtil::PPC_FPCLASS_PN);
	EXPECT_EQ(MathUtil::ClassifyDouble(-1.0),       MathUtil::PPC_FPCLASS_NN);
	EXPECT_EQ(MathUtil::ClassifyDouble(1235223.0),  MathUtil::PPC_FPCLASS_PN);
	EXPECT_EQ(MathUtil::ClassifyDouble(-1263221.0), MathUtil::PPC_FPCLASS_NN);
	EXPECT_EQ(MathUtil::ClassifyDouble(1.0E-308),   MathUtil::PPC_FPCLASS_PD);
	EXPECT_EQ(MathUtil::ClassifyDouble(-1.0E-308),  MathUtil::PPC_FPCLASS_ND);
	EXPECT_EQ(MathUtil::ClassifyDouble(0.0),        MathUtil::PPC_FPCLASS_PZ);
	EXPECT_EQ(MathUtil::ClassifyDouble(-0.0),       MathUtil::PPC_FPCLASS_NZ);
	EXPECT_EQ(MathUtil::ClassifyDouble(HUGE_VAL),   MathUtil::PPC_FPCLASS_PINF);  // weird #define for infinity
	EXPECT_EQ(MathUtil::ClassifyDouble(-HUGE_VAL),  MathUtil::PPC_FPCLASS_NINF);
	EXPECT_EQ(MathUtil::ClassifyDouble(sqrt(-1.0)), MathUtil::PPC_FPCLASS_QNAN);

	// Float version
	EXPECT_EQ(MathUtil::ClassifyFloat(1.0f),        MathUtil::PPC_FPCLASS_PN);
	EXPECT_EQ(MathUtil::ClassifyFloat(-1.0f),       MathUtil::PPC_FPCLASS_NN);
	EXPECT_EQ(MathUtil::ClassifyFloat(1235223.0f),  MathUtil::PPC_FPCLASS_PN);
	EXPECT_EQ(MathUtil::ClassifyFloat(-1263221.0f), MathUtil::PPC_FPCLASS_NN);
	EXPECT_EQ(MathUtil::ClassifyFloat(1.0E-43f),    MathUtil::PPC_FPCLASS_PD);
	EXPECT_EQ(MathUtil::ClassifyFloat(-1.0E-43f),   MathUtil::PPC_FPCLASS_ND);
	EXPECT_EQ(MathUtil::ClassifyFloat(0.0f),        MathUtil::PPC_FPCLASS_PZ);
	EXPECT_EQ(MathUtil::ClassifyFloat(-0.0f),       MathUtil::PPC_FPCLASS_NZ);
	EXPECT_EQ(MathUtil::ClassifyFloat((float)HUGE_VAL),  MathUtil::PPC_FPCLASS_PINF);  // weird #define for infinity
	EXPECT_EQ(MathUtil::ClassifyFloat((float)-HUGE_VAL), MathUtil::PPC_FPCLASS_NINF);
	EXPECT_EQ(MathUtil::ClassifyFloat(sqrtf(-1.0f)),     MathUtil::PPC_FPCLASS_QNAN);

	EXPECT_FALSE(MathUtil::IsNAN(1.0));
	EXPECT_TRUE(MathUtil::IsNAN(sqrt(-1.0)));
	EXPECT_FALSE(MathUtil::IsSNAN(sqrt(-1.0)));

	// EXPECT_TRUE(MathUtil::IsQNAN(sqrt(-1.0)));  // Hmm...
	EXPECT_EQ(pow2(2.0), 4.0);
	EXPECT_EQ(pow2(-2.0), 4.0);
}

void StringTests()
{
	EXPECT_EQ(StripSpaces(" abc   "), "abc");

	EXPECT_EQ(StripQuotes("\"abc\""), "abc");
	EXPECT_EQ(StripQuotes("\"abc\" "), "\"abc\" ");

	EXPECT_EQ(TabsToSpaces(4, "a\tb"), "a    b");
	EXPECT_EQ(ThousandSeparate(1234567, 15), "      1,234,567");

	int i = 7;
	EXPECT_EQ(false, TryParse("FF", &i));
	EXPECT_EQ(7, i);
	EXPECT_EQ(true, TryParse("22", &i));
	EXPECT_EQ(22, i);

	std::vector<std::string> strs;
	SplitString("blah,foo,bar", ',', strs);

	EXPECT_EQ(3, strs.size());
	EXPECT_EQ("bar", strs[2]);

	std::string path, fname, ext;
	SplitPath("C:/some/path/test.jpg", &path, &fname, &ext);
	EXPECT_EQ("C:/some/path/", path);
	EXPECT_EQ("test", fname);
	EXPECT_EQ(".jpg", ext);

	SplitPath("C:/so.me/path/", &path, &fname, &ext);
	EXPECT_EQ("C:/so.me/path/", path);
	EXPECT_EQ("", fname);
	EXPECT_EQ("", ext);

	SplitPath("test.file.jpg", &path, &fname, &ext);
	EXPECT_EQ("", path);
	EXPECT_EQ("test.file", fname);
	EXPECT_EQ(".jpg", ext);
}


int main(int argc, char* argv[])
{
	AudioJitTests();

	CoreTests();
	MathTests();
	StringTests();
	if (fail_count == 0)
	{
		printf("All tests passed.\n");
	}
	return 0;
}
