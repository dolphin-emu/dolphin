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

#include "Core/ARBruteForcer.h"
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
// Number of windows messages without saving a screenshot
int ch_cycles_without_snapshot;
// To Do: Is this actually needed?
bool ch_last_search;
bool ch_bruteforce;

std::vector<std::string> ch_map;
std::string ch_title_id;
std::string ch_code;

void ARBruteForceDriver()
{
	ch_cycles_without_snapshot++;
	// if begining searching, start from the most recently saved position
	if (ch_begin_search)
	{
		ch_begin_search = false;
		ch_next_code = false;
		ch_current_position = ch_load_last_position();
		if (ch_current_position >= ch_map.size())
		{
			ch_first_search = false;
			ch_bruteforce = 0;

			PostProcessCSVFile();

			SuccessAlert("Finished brute forcing! To start again, delete position.txt in the screenshots folder.");
		}
		else
		{
			ch_first_search = true;
			State::Load(1);
		}
	}
	// if we should move on to the next code then do so, and save where we are up to
	// if we have received 30 windows messages without saving a screenshot, then this code is probably bad
	// so skip to the next one
	else if (ch_next_code || (ch_current_position && ch_cycles_without_snapshot > 30 && ch_last_search))
	{
		ch_next_code = false;
		ch_first_search = false;
		ch_current_position++;
		ch_save_last_position(ch_current_position);
		ch_cycles_without_snapshot = 0;
		if (ch_current_position >= ch_map.size())
		{
			SuccessAlert("Finished brute forcing! To start again, delete position.txt in the screenshots folder.");
			ch_bruteforce = 0;

			PostProcessCSVFile();
		}
		else
		{
			State::Load(1);
		}
	}
}

void SetupScreenshotAndWriteCSV(volatile bool* s_bScreenshot, std::string* s_sScreenshotName)
{
	std::string s_sAux = std::to_string(ch_current_position) + "," + ch_map[ch_current_position] +
		"," + ch_code + "," + std::to_string(stats.thisFrame.numPrims) + "," + std::to_string(stats.thisFrame.numDrawCalls) + "," + std::to_string(ch_take_screenshot);
	std::ofstream myfile;
	myfile.open(File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/bruteforce.csv", std::ios_base::app);
	myfile << s_sAux << "\n";
	myfile.close();
	if (ch_take_screenshot == 1){
		*s_bScreenshot = true;
		*s_sScreenshotName = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/" + std::to_string(ch_current_position) + "_" + ch_map[ch_current_position] + "_" + ch_code + ".png";
		ch_cycles_without_snapshot = 0;
		ch_last_search = true;
		ch_next_code = true;
	}

	ch_take_screenshot--;
}

// Load the start address of each function from the .map file into a vector.
void ParseMapFile(std::string unique_id)
{
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
			if (loc != std::string::npos && line.find("__start") == std::string::npos)
			{
				line = line.substr(loc + 2, 6);
				// Double check it's hex and the correct size.
				if (line.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos && line.size() == 6)
					ch_map.push_back("04" + line);
			}
		}
		myfile.close();
	}
}

void IncrementPositionTxt()
{
	//if (ch_begin_search)
	//	PanicAlert("Entered with ch_begin_search = 1");

	std::ifstream myfile_in(File::GetUserPath(D_SCREENSHOTS_IDX) + "position.txt");

	if (myfile_in.is_open())
	{
		std::string aux;
		std::string line;

		while (getline(myfile_in, line))
		{
			aux = line;
		}

		ch_current_position = atoi(aux.c_str());
		myfile_in.close();
	}

	std::ofstream myfile_out(File::GetUserPath(D_SCREENSHOTS_IDX) + "position.txt");
	if (myfile_out.is_open())
	{
		std::string Result;
		std::ostringstream convert;   // stream used for the conversion
		convert << ++ch_current_position;      // insert the textual representation of 'Number' in the characters in the stream
		myfile_out << convert.str() + "\n";
		myfile_out.close();
	}
}

// save last position
void ch_save_last_position(int position)
{
	std::ofstream myfile(File::GetUserPath(D_SCREENSHOTS_IDX) + "position.txt");
	if (myfile.is_open())
	{
		std::string Result;
		std::ostringstream convert;   // stream used for the conversion
		convert << position;      // insert the textual representation of 'Number' in the characters in the stream
		myfile << convert.str() + "\n";
		myfile.close();
	}
}
// load last position
int ch_load_last_position()
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
	return (atoi(aux.c_str()));
}

// Create a new processed.csv file that only contains the functions that changed how many objects were rendered.
void PostProcessCSVFile()
{
	std::string mode1;
	std::string mode2;

	std::string path_to_original_csv = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/bruteforce.csv";
	std::string path_to_processed_csv = File::GetUserPath(D_SCREENSHOTS_IDX) + ch_title_id + "/processed.csv";

	FindModeOfCSV(path_to_original_csv, &mode1, &mode2);
	StripModesFromCSV(path_to_original_csv, path_to_processed_csv, mode1, mode2);
}

// Find the most common amount of primitives and draw calls rendered.
void FindModeOfCSV(std::string filename, std::string* thing1_mode, std::string* thing2_mode)
{
	std::unordered_map<std::string, int> things1_bins;
	std::unordered_map<std::string, int> things2_bins;

	std::ifstream file(filename);
	int max_things1 = 0;
	int max_things2 = 0;
	std::string maxthing1;
	std::string maxthing2;

	std::string value1, value2, value3, things1, things2, frame;

	while (getline(file, value1, ','))
	{
		getline(file, value2, ',');
		getline(file, value3, ',');
		getline(file, things1, ',');
		getline(file, things2, ',');
		getline(file, frame);

		things1_bins.emplace(things1, -1);
		things2_bins.emplace(things2, -1);

		things1_bins[things1]++;
		things2_bins[things2]++;
	}

	for (auto i : things1_bins)
	{
		if (i.second > max_things1)
		{
			max_things1 = i.second;
			maxthing1 = i.first;
		}
	}

	for (auto i : things2_bins)
	{
		if (i.second > max_things2)
		{
			max_things2 = i.second;
			maxthing2 = i.first;
		}
	}

	file.close();

	*thing1_mode = maxthing1;
	*thing2_mode = maxthing2;
}

// Remove the rows that contain the most common amount of objects. These are unlikely to be interesting to us.
void StripModesFromCSV(std::string infilename, std::string outfilename, std::string thing1_mode, std::string thing2_mode)
{
	std::ifstream infile(infilename);
	std::ofstream ofile(outfilename);

	int mode1 = atoi(thing1_mode.c_str());
	int mode2 = atoi(thing2_mode.c_str());
	std::string value1, value2, value3, things1, things2, frame;

	while (getline(infile, value1, ','))
	{
		getline(infile, value2, ',');
		getline(infile, value3, ',');
		getline(infile, things1, ',');
		getline(infile, things2, ',');
		getline(infile, frame);

		if (atoi(frame.c_str()) == 1)
		{
			if (!((atoi(things1.c_str()) == mode1) && (atoi(things2.c_str()) == mode2)))
			{
				ofile << value1 << "," << value2 << "," << value3 << "," << things1 << "," << things2 << "," << frame << std::endl;
			}
		}

	}

	infile.close();
	ofile.close();
}

} //namespace ARBruteForcer
