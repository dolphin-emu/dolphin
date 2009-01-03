// Copyright (C) 2003-2008 Dolphin Project.

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


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

#include "PowerPC/PowerPc.h" // Core

#include "../../../../Source/Core/DiscIO/Src/FileSystemGCWii.h" // This file has #include "Filesystem.h"
#include "../../../../Source/Core/DiscIO/Src/VolumeCreator.h"

#include "../../Player/Src/PlayerExport.h" // Local player
#include "../../Common/Src/Console.h" // Local common
//////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯
namespace MusicMod
{

struct MyFilesStructure
{
		std::string path;
		int offset; // Is int enough, how high does offset go?
};

std::vector <MyFilesStructure> MyFiles;
void StructSort (std::vector <MyFilesStructure> &MyFiles);


// Playback
std::string currentfile;
std::string unique_gameid;

std::string MusicPath;

DiscIO::CFileSystemGCWii* my_pFileSystem;

extern bool bShowConsole;
int WritingFile = false;
bool dllloaded = false;
std::string CurrentPlayFile;
//////////////////////////////////



// ---------------------------------------------------------------------------------------
// A function to sort the filelist table after offset
void StructSort (std::vector <MyFilesStructure> &MyFiles)
{
	MyFilesStructure temp;

	//wprintf("StructSort > Begin\n");

     for(int i = 0; i < MyFiles.size() - 1; i++)
     {
          for (int j = i + 1; j < MyFiles.size(); j++)
          {
               //if (printerStock[ i ].brand > printerStock[ j ].brand)  //comparing brands 
				if (MyFiles[ i ].offset > MyFiles[ j ].offset)  //comparing cost 
               {
                     temp = MyFiles[ i ]; // Swapping entire struct
                     MyFiles[ i ] = MyFiles[ j ];
                     MyFiles[ j ] = temp;
               }
          }
     }


	for (long i=1; i<(long)MyFiles.size(); ++i)
	{
		//cout << printerStock.at(i).brand.c_str() << printerStock.at(i).cost << "\n";
		std::cout << i << " " << MyFiles[i].path.c_str() << "#" << MyFiles[i].offset << "\n";
	}

	//wprintf("StructSort > Done\n");
}



// =======================================================================================
/* This will load Plainamp.dll. It's the original Plainamp.exe with removed GUI and some small
   modifications. */
void Main(std::string FileName)
{
	//
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(FileName.c_str());
	
	//
	my_pFileSystem = new DiscIO::CFileSystemGCWii(pVolume);

	/* We have to sort the files according to offset so that out scan in Blob.cpp works.
	   Because StructSort() only works for MyFilesStructure I copy the offset and filenames
	   to a new vetor first. */	
	MyFiles.resize(my_pFileSystem->m_FileInfoVector.size()); // Set size
	for (size_t i = 0; i < my_pFileSystem->m_FileInfoVector.size(); i++)
	{
		MyFiles.at(i).offset = my_pFileSystem->m_FileInfoVector.at(i).m_Offset;
		MyFiles.at(i).path = my_pFileSystem->m_FileInfoVector.at(i).m_FullPath;
	}		

	StructSort(MyFiles);

	// These things below will not need to be updated after a new game is started
	if (dllloaded) return;

	Player_Main(bShowConsole); // Call the DLL for the first time
	//play_file("c:\\demo36_02.ast");
	wprintf("DLL loaded\n");
	dllloaded = true; // Do this once


	// ---------------------------------------------------------------------------------------
	// Make Music directory
	LPSECURITY_ATTRIBUTES attr;
	attr = NULL;
	MusicPath = "Music\\";
	wprintf("Created a Music directory\n");
	CreateDirectory(MusicPath.c_str(), attr);
	// ----------------------------------------------------------------------------------------
}


// =======================================================================================
// Supported music files
// ---------------------------------------------------------------------------------------
bool CheckFileEnding(std::string FileName)
{
	if (  
		   (FileName.find(".adp") != std::string::npos) // 1080 Avalanche, Crash Bandicoot etc
		|| (FileName.find(".afc") != std::string::npos) // Zelda WW
		|| (FileName.find(".ast") != std::string::npos) // Zelda TP, Mario Kart
		|| (FileName.find(".dsp") != std::string::npos) // Metroid Prime
		|| (FileName.find(".hps") != std::string::npos) // SSB Melee
		)
	return true;
}
// =======================================================================================



// =======================================================================================
//
void CheckFile(u64 offset, u64 size)
{
	// =======================================================================================
	/* Only do this test:
			1. After boot, not on ISO scan
			2. Not if offset = 0.
			3. Not when WritingFile. We will lead to calls back to here (Read) and it will mess
			   upp the scanning */
	if(PowerPC::state == PowerPC::CPUState::CPU_RUNNING && offset != 0 && !WritingFile)
	{

	
		//////////////////////////////////////////////////////////////////////////////////////////
		/* Get the filename. Here we go through all files until we come to the file that has
		   the matching offset. Before MyFiles has data this loop will go nowhere. We have to
		   specify (MyFiles.size() - 1) because we will be reading MyFiles.at(i + 1).
		   MyFiles.size() is the amount of files on the disc, and the last file is 
		   MyFiles.at(MyFiles.size() - 1) */
		// ---------------------------------------------------------------------------------------
		for (int i = 0; i < (int)(MyFiles.size() - 1); ++i)
		{
			// ---------------------------------------------------------------------------------------
			/* If we assume that myoffset is the begginning of every file this works. 
			   Suppose there are three files
					1 is 0 to 149
					2 is 150 to 170
					3 is 171 to 200
			   If the offset is 160 the game is reading file number two, for example
					myoffset = 150: (myoffset >= offset) == false
					myoffset = 171: (myoffset >= offset) == true, break

			  However if the offset is 195 the game is reading the last file and we will get
					myoffset = 171: (myoffset >= offset) == false
			  we therefore need to add the condition (offset > MyFiles[MyFiles.size() - 1].offset)
			  to. */
			if (MyFiles[i + 1].offset >= offset || offset > MyFiles[MyFiles.size() - 1].offset)
			{
				// Now we know that the game is reading from MyFiles[i].path

				// Break if we found the same file again
				if (currentfile == MyFiles[i].path) break;
	
				//wprintf(">>>> (%i)Current read %s <%u = %ux%i> <block %u>\n", i, CurrentFiles[i].path.c_str(), offset, CurrentFiles[i].offset, size);

				if (CheckFileEnding(MyFiles[i].path.c_str()))
				{
					//u64 myoffset2 = (u32)offset;
					wprintf("\n >>> (%i/%i) Match %s <%u = %ux%i> <block %u>\n\n", i,
						MyFiles.size(), MyFiles[i].path.c_str(), offset, MyFiles[i].offset, size);
					//wprintf(">>>> [i + 1].offset %i", MyFiles[i + 1].offset);
					currentfile = MyFiles[i].path; // save the found file

					// ---------------------------------------------------------------------------------------
					// We will now save the file to the PC hard drive
					// ---------------------------------------------------------------------------------------
					// Get the filename
					std::size_t pointer = MyFiles[i].path.find_last_of("\\");
					std::string fragment = MyFiles[i].path.substr (0, (pointer-0));
					int compare = MyFiles[i].path.length() - fragment.length(); // get the length of the filename
					fragment = MyFiles[i].path.substr ((pointer+1), compare); // now we have the filename
					
					// ---------------------------------------------------------------------------------------
					// Create the file path
					std::string FilePath = (MusicPath + fragment);
					// ---------------------------------------------------------------------------------------
					WritingFile = true; // Avoid detecting the file we are writing
					wprintf("Writing <%s> to <%s>\n", MyFiles[i].path.c_str(), FilePath.c_str());
					my_pFileSystem->ExportFile(MyFiles[i].path.c_str(), FilePath.c_str());
					WritingFile = false;
					
					// ---------------------------------------------------------------------------------------
					// Play the file we found
					if(dllloaded)
					{
						Player_Play((char*)FilePath.c_str()); // retype it from const char* to char*
					} else {
						wprintf("Warning > Music DLL is not loaded");
					}

					// ---------------------------------------------------------------------------------------
					// Remove the last file, if any
					if(CurrentPlayFile.length() > 0)
					{
						if(!remove(CurrentPlayFile.c_str()))
						{
							wprintf("The program failed to remove <%s>\n", CurrentPlayFile.c_str());
						} else {
							wprintf("The program removed <%s>\n", CurrentPlayFile.c_str());
						}
					}

					// ---------------------------------------------------------------------------------------
					// Save the current playing file
					CurrentPlayFile = FilePath; // Save the filename so we can remove it later
					break;
					// ---------------------------------------------------------------------------------------

				}

				// ---------------------------------------------------------------------------------------
				// Tell about the files we ignored
				wprintf("(%i/%i) Ignored %s <%u = %ux%i> <block %u>\n", i, MyFiles.size(), MyFiles[i].path.c_str(), offset, MyFiles[i].offset, size);
				currentfile = MyFiles[i].path;
				break;
				// ---------------------------------------------------------------------------------------
			}
		}

		//wprintf("Blob.cpp:Read <%i> <%i> <%i> <%i>\n", offset, size, out_ptr);
	} // This ends the entire filescan
	// =======================================================================================



}

} // end of namespace