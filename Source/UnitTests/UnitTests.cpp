// Copyright (C) 2003-2009 Dolphin Project.

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

using namespace std;

int fail_count = 0;

#define EXPECT_TRUE(a) \
	if (!a) { \
		cout << "FAIL (" __FUNCTION__ "): " << #a << " is false" << endl; \
		cout << "Value: " << a << endl << "Expected: true" << endl; \
		fail_count++; \
	}

#define EXPECT_FALSE(a) \
	if (a) { \
		cout << "FAIL (" __FUNCTION__ "): " << #a << " is true" << endl; \
		cout << "Value: " << a << endl << "Expected: false" << endl; \
		fail_count++; \
	}

#define EXPECT_EQ(a, b) \
	if ((a) != (b)) { \
		cout << "FAIL (" __FUNCTION__ "): " << #a << " is not equal to " << #b << endl; \
		cout << "Actual: " << a << endl << "Expected: " << b << endl; \
		fail_count++; \
	}

void CoreTests()
{
}

void MathTests()
{
	// Tests that our fp classifier is correct.
	EXPECT_EQ(MathUtil::ClassifyFP(1.0), MathUtil::PPC_FPCLASS_PN);
	EXPECT_EQ(MathUtil::ClassifyFP(-1.0), 0x8);
	EXPECT_EQ(MathUtil::ClassifyFP(1235223.0), 0x4);
	EXPECT_EQ(MathUtil::ClassifyFP(-126323521.0), 0x8);
	EXPECT_EQ(MathUtil::ClassifyFP(1.0E-308), 0x14);
	EXPECT_EQ(MathUtil::ClassifyFP(-1.0E-308), 0x18);
	EXPECT_EQ(MathUtil::ClassifyFP(0.0), 0x2);
	EXPECT_EQ(MathUtil::ClassifyFP(-0.0), 0x12);
	EXPECT_EQ(MathUtil::ClassifyFP(HUGE_VAL), 0x5);  // weird #define for infinity
	EXPECT_EQ(MathUtil::ClassifyFP(-HUGE_VAL), 0x9);
	EXPECT_EQ(MathUtil::ClassifyFP(sqrt(-1.0)), 0x11);  // SNAN

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
	EXPECT_EQ(StripNewline(" abc \n"), " abc ");
	EXPECT_EQ(StripNewline(" abc \n "), " abc \n ");
	EXPECT_EQ(StripQuotes("\"abc\""), "abc");
	EXPECT_EQ(StripQuotes("\"abc\" "), "\"abc\" ");
}

int main(int argc, _TCHAR* argv[])
{
	CoreTests();
	MathTests();
	StringTests();
	if (fail_count == 0)
	{
		printf("All tests passed.\n");
	}
	return 0;
}


// Pretend that we are a host so we can link to core.... urgh.
//==============================================================
void Host_UpdateMainFrame(){}
void Host_UpdateDisasmDialog(){}
void Host_UpdateLogDisplay(){}
void Host_UpdateMemoryView(){}
void Host_NotifyMapLoaded(){}
void Host_UpdateBreakPointView(){}
void Host_SetDebugMode(bool enable){}

void Host_SetWaitCursor(bool enable){}

void Host_UpdateStatusBar(const char* _pText, int Filed = 0){}
#ifdef SETUP_TIMER_WAITING
void Host_UpdateGUI(){}
#endif

void Host_SysMessage(const char *fmt, ...){}
void Host_SetWiiMoteConnectionState(int _State){}

void Host_UpdateLeds(int bits){}
void Host_UpdateSpeakerStatus(int index, int bits){}
void Host_UpdateStatus(){}

int CSIDevice_GCController::GetNetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	return 0;
}
