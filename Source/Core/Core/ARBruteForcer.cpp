// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// AR Code Brute Forcer - Created by Penkamaster, reworked by cegli.

// -----------------------------------------------------------------------------------------
// Function of the AR Code Bruter Forcer:
// This system is designed to go through every function in a game and force it to return
// a specified value. It then takes a screenshot and updates a csv file that states how
// many primitives and draw calls were done. This information can be used to find functions
// that disable culling, rendering, or control the in game camera.
// This is especially important in VR, where each game should have its "Disable Culling"
// function found.
// -----------------------------------------------------------------------------------------

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <string>
#include <unordered_map>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/State.h"
#include "VideoCommon/Statistics.h"

namespace ARBruteForcer
{
// count down to take a screenshot
int ch_take_screenshot;
int ch_current_position;
// Move on to the next code
bool ch_next_code;
bool ch_begin_search;
bool ch_first_search;
bool ch_begun = false;
// Number of windows messages without saving a screenshot
int ch_cycles_without_snapshot;
// To Do: Is this actually needed?
bool ch_last_search;
bool ch_bruteforce;
bool ch_dont_save_settings;

bool ch_screenshot_all = false;

int original_prim_count;


std::vector<std::string> ch_map;
std::string ch_title_id;
std::string ch_code;

void ARBruteForceDriver()
{
  ch_cycles_without_snapshot++;
  // if begining searching, start from the most recently saved position
  if (ch_begin_search)
  {
    NOTICE_LOG(VR, "begin search");
    ch_begin_search = false;
    ch_next_code = false;
    ch_current_position = LoadLastPosition();
    ch_cycles_without_snapshot = 0;
    ERROR_LOG(VR, "ch_current_position = %d, ch_map.size = %d", ch_current_position, (int)ch_map.size());
    if (ch_current_position >= (int)ch_map.size() && ch_bruteforce)
    {
      ch_first_search = false;
      ch_bruteforce = false;
      ch_begun = false;

      NOTICE_LOG(VR, "Finished bruteforcing when starting");
      PostProcessCSVFile();

      SuccessAlert(
          "Finished brute forcing! To start again, delete position.txt in the screenshots folder.");
    }
    else
    {
      original_prim_count = SConfig::GetInstance().m_OriginalPrimitiveCount;
      ch_screenshot_all = SConfig::GetInstance().m_BruteforceScreenshotAll;
      ch_first_search = true;
      ch_begun = true;
      State::Load(1);
      // not sure why this is needed
      State::Load(1);
      ERROR_LOG(VR, "Loaded first state, prim_count = %d", original_prim_count);
    }
  }
  // if we should move on to the next code then do so, and save where we are up to
  // if we have received 30 windows messages without saving a screenshot, then this code is probably
  // bad
  // so skip to the next one
  else if (ch_begun && (ch_next_code ||
           (ch_current_position > 0 && ch_cycles_without_snapshot > 30 && ch_last_search) ||
           (ch_current_position >= 0 && ch_cycles_without_snapshot > 100)))
  {
    WARN_LOG(VR, "Next code %d (position = %d, cycles = %d, last = %d)", ch_next_code, ch_current_position, ch_cycles_without_snapshot, ch_last_search);

    ch_next_code = false;
    ch_first_search = false;
    ch_current_position++;
    SaveLastPosition(ch_current_position);
    ch_cycles_without_snapshot = 0;
    if (ch_current_position >= (int)ch_map.size())
    {
      ch_bruteforce = 0;
      NOTICE_LOG(VR, "Finished bruteforcing");
      PostProcessCSVFile();

      SuccessAlert(
          "Finished brute forcing! To start again, delete position.txt in the screenshots folder.");
    }
    else
    {
      State::Load(1);
      WARN_LOG(VR, "Loaded next state");
    }
  }
  else if (ch_begun)
  {
    WARN_LOG(VR, "message");
  }
}

void SetupScreenshotAndWriteCSV(Renderer *render)
{
  WARN_LOG(VR, "screenshot = %d, ch_current_position = %d, ", ch_take_screenshot, ch_current_position);
  std::string addr;
  if (ch_current_position >= 0)
    addr = ch_map[ch_current_position];
  std::string s_sAux = std::to_string(ch_current_position) + "," + addr +
                       "," + ch_code + "," + std::to_string(stats.thisFrame.numPrims) + "," +
                       std::to_string(stats.thisFrame.numDrawCalls) + "," +
                       std::to_string(ch_take_screenshot);
  std::ofstream myfile;
  myfile.open(File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/bruteforce.csv",
              std::ios_base::app);
  myfile << s_sAux << "\n";
  myfile.close();
  if (ch_take_screenshot == 1)
  {
    int prims = stats.thisFrame.numPrims;
    if (ch_current_position < 0)
    {
      original_prim_count = prims;
      SConfig::GetInstance().m_OriginalPrimitiveCount = original_prim_count;
      SConfig::GetInstance().m_BruteforceScreenshotAll = ch_screenshot_all;
      SConfig::GetInstance().SaveSettings();
      NOTICE_LOG(VR, "Saved setting, prim_count = %d", prims);
    }
    if (ch_current_position < 0 || prims != original_prim_count || ch_screenshot_all)
    {
      std::string filename;
      if (ch_current_position < 0)
        filename = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/" + StringFromFormat("original %d.png", prims);
      else if (prims == 0)
        filename = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/" + StringFromFormat("blank %s %s.png", addr, ch_code);
      else if (prims > original_prim_count)
        filename = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/" + StringFromFormat("show %d %s %s.png", prims, addr, ch_code);
      else
        filename = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/" + StringFromFormat("hide %d %s %s.png", prims, addr, ch_code);
      //filename = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/" +
      //  std::to_string(ch_current_position) + "_" + addr +
      //  "_" + ch_code + ".png";
      render->SaveScreenshot(filename, false);
      WARN_LOG(VR, "Requested screenshot");
    }
    else
    {
      WARN_LOG(VR, "No screenshot");
    }
    ch_cycles_without_snapshot = 0;
    ch_last_search = true;
    ch_next_code = true;
  }

  ch_take_screenshot--;
}

// Load the start address of each function from the .map file into a vector.
void ParseMapFile(std::string unique_id)
{
  NOTICE_LOG(VR, "ParseMapFile");
  std::string userPath = File::GetUserPath(D_MAPS_IDX);
  std::string userPathScreens = File::GetUserPath(D_SCREENSHOTS_IDX);

  std::string line;
  std::ifstream myfile(userPath + unique_id + ".map");
  std::string gameScrenShotsPath = userPathScreens + unique_id;
#ifdef _WIN32
  mkdir(gameScrenShotsPath.c_str());
#else
  mkdir(gameScrenShotsPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

  ch_title_id = unique_id;
  if (myfile.is_open())
  {
    while (getline(myfile, line))
    {
      std::string::size_type loc = line.find("80");
      if (loc != std::string::npos && line.find("__start") == std::string::npos &&
          line.find("OS") == std::string::npos)
      {
        line = line.substr(loc + 2, 6);
        // Double check it's hex and the correct size.
        if (line.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos &&
            line.size() == 6)
          ch_map.push_back("04" + line);
      }
    }
    myfile.close();
  }
}

void IncrementPositionTxt()
{
  WARN_LOG(VR, "IncrementPositionTxt");
  ch_current_position = LoadLastPosition();
  SaveLastPosition(++ch_current_position);
  WARN_LOG(VR, "IncrementedPositionTxt");
}

// save last position
void SaveLastPosition(int position)
{
  std::ofstream myfile(File::GetUserPath(D_SCREENSHOTS_IDX) + "position.txt");
  if (myfile.is_open())
  {
    std::string Result;
    std::ostringstream convert;  // stream used for the conversion
    convert << position;  // insert the textual representation of 'Number' in the characters in the
                          // stream
    myfile << convert.str() + "\n";
    myfile.close();
  }
}
// load last position
int LoadLastPosition()
{
  std::string line;
  std::ifstream myfile(File::GetUserPath(D_SCREENSHOTS_IDX) + "position.txt");
  std::string aux;

  if (myfile.is_open())
  {
    while (getline(myfile, line))
    {
      aux = line;
    }
    myfile.close();
  }

  std::istringstream iss(aux.c_str());
  iss.imbue(std::locale("C"));
  int tmp = -1;
  if (iss >> tmp)
    return tmp;
  else
    return -1;
}

// Create a new processed.csv file that only contains the functions that changed how many objects
// were rendered.
void PostProcessCSVFile()
{
  std::string most_common_num_prims;
  std::string most_common_num_draw_calls;

  std::string path_to_original_csv =
      File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/bruteforce.csv";
  std::string path_to_processed_csv =
      File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/processed.csv";

  FindModeOfCSV(path_to_original_csv, &most_common_num_prims, &most_common_num_draw_calls);
  StripModesFromCSV(path_to_original_csv, path_to_processed_csv, most_common_num_prims,
                    most_common_num_draw_calls);
}

// Find the most common amount of primitives and draw calls rendered.
void FindModeOfCSV(std::string filename, std::string* most_common_num_prims,
                   std::string* most_common_num_draw_calls)
{
  std::unordered_map<std::string, int> num_prims_bins;
  std::unordered_map<std::string, int> num_draw_calls_bins;

  std::ifstream file(filename);
  int current_largest_bin_prims = 0;
  int current_largest_bin_draw_calls = 0;

  std::string position, address, bruteforce_code, num_prims, num_draw_calls, frame;

  while (getline(file, position, ','))
  {
    getline(file, address, ',');
    getline(file, bruteforce_code, ',');
    getline(file, num_prims, ',');
    getline(file, num_draw_calls, ',');
    getline(file, frame);

    // Create bin and initialize to -1 if number has not been seen before.
    // Otherwise, does nothing.
    num_prims_bins.emplace(num_prims, -1);
    num_draw_calls_bins.emplace(num_draw_calls, -1);

    // Increment bin for number parsed
    num_prims_bins[num_prims]++;
    num_draw_calls_bins[num_draw_calls]++;
  }

  for (std::pair<const std::string, int> i : num_prims_bins)
  {
    if (i.second > current_largest_bin_prims)
    {
      current_largest_bin_prims = i.second;
      *most_common_num_prims = i.first;
    }
  }

  for (std::pair<const std::string, int> i : num_draw_calls_bins)
  {
    if (i.second > current_largest_bin_draw_calls)
    {
      current_largest_bin_draw_calls = i.second;
      *most_common_num_draw_calls = i.first;
    }
  }

  file.close();
}

// Remove the rows that contain the most common amount of objects. These are unlikely to be
// interesting to us.
void StripModesFromCSV(std::string infilename, std::string outfilename,
                       std::string most_common_num_prims, std::string most_common_num_draw_calls)
{
  std::ifstream infile(infilename);
  std::ofstream ofile(outfilename);

  std::string position, address, bruteforce_code, num_prims, num_draw_calls, frame;

  while (getline(infile, position, ','))
  {
    getline(infile, address, ',');
    getline(infile, bruteforce_code, ',');
    getline(infile, num_prims, ',');
    getline(infile, num_draw_calls, ',');
    getline(infile, frame);

    if (frame == "1")
    {
      if (!(num_prims == most_common_num_prims && num_draw_calls == most_common_num_draw_calls))
      {
        ofile << position << "," << address << "," << bruteforce_code << "," << num_prims << ","
              << num_draw_calls << "," << frame << std::endl;
      }
    }
  }

  infile.close();
  ofile.close();
}

}  // namespace ARBruteForcer
