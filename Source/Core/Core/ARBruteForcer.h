// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

namespace ARBruteForcer
{

// take photo
extern int ch_take_screenshot;
// current code
extern int ch_current_position;
// move on to next code?
extern bool ch_next_code;
// start searching
extern bool ch_begin_search;
extern bool ch_first_search;
// number of windows messages without saving a screenshot
extern int ch_cycles_without_snapshot;
// To Do: Is this actually needed?
extern bool ch_last_search;
// emulator is in action replay culling code brute-forcing mode
extern bool ch_bruteforce;
extern bool ch_dont_save_settings;

extern std::vector<std::string> ch_map;
extern std::string ch_title_id;
extern std::string ch_code;

void ARBruteForceDriver();
void SetupScreenshotAndWriteCSV(volatile bool* s_bScreenshot, std::string* s_sScreenshotName);
void ParseMapFile(std::string unique_id);
void IncrementPositionTxt();
void SaveLastPosition(int position);
int LoadLastPosition();
void PostProcessCSVFile();
void FindModeOfCSV(std::string filename, std::string* thing1_mode, std::string* thing2_mode);
void StripModesFromCSV(std::string infilename, std::string outfilename, std::string thing1_mode, std::string thing2_mode);

}  // namespace
