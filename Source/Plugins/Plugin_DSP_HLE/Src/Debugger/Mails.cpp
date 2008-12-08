//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

// includes
#include <iostream>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <stdlib.h>
#endif

#include "Debugger.h"
#include "PBView.h"
#include "IniFile.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "FileSearch.h"
#include "../Logging/Console.h" // open and close console

extern std::vector<std::string> sMailLog, sMailTime;
extern CDebugger* m_frame;

// =======================================================================================
// Update mail window
// --------------
void CDebugger::DoUpdateMail()
{
	//wprintf("i  %i  %i\n", sFullMail.size(), sMailLog.size());

	if(sFullMail.size() > 0 && sMailLog.size() > 0)
	{
            m_log->SetValue(wxString::FromAscii(sFullMail.at(m_RadioBox[3]->GetSelection()).c_str()));
            m_log->SetDefaultStyle(wxTextAttr(*wxBLUE)); // doesn't work because of the current wx
            
            m_log1->SetValue(wxString::FromAscii(sMailLog.at(m_RadioBox[3]->GetSelection()).c_str()));
            m_log1->AppendText(wxT("\n\n"));
	}
}


void CDebugger::UpdateMail(wxNotebookEvent& event)
{
	DoUpdateMail();
	/* This may be called before m_frame is fully created through the
	   EVT_NOTEBOOK_PAGE_CHANGED, in that case it will crash because this
	   is accessing members of it */
	if(StoreMails && m_frame) ReadDir();
}

// Change mail from radio button change
void CDebugger::ChangeMail(wxCommandEvent& event)
{
	//wprintf("abc");
	DoUpdateMail();
	//if(StoreMails) ReadDir();
}
// ==============



// =======================================================================================
// Read out mails from dir
// --------------
void CDebugger::ReadDir()
{
	CFileSearch::XStringVector Directories;
	//Directories.push_back("Logs/Mail");
	Directories.push_back(FULL_MAIL_LOGS_DIR);	

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.log");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	//m_gc->Show(false);
	//m_gc->Append(wxT("SSBM ffffix"));
	//m_gc->Show(true);

	// Clear in case we already did this earlier
	all_all_files.clear();

	if (rFilenames.size() > 0 && m_gc && m_wii)
	{
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL); // place the filename in FileName

			//std::string FileName = StripSpaces(*FileName);
			std::vector<std::string> pieces;	
			SplitString(FileName, "_sep", pieces); // split string

			// Save all filenames heres
			if(pieces[2] == "0") all_all_files.push_back(pieces[0]);

			// Cut to size
			std::string cut;
			if(pieces[0].length() > 18)
				cut = pieces[0].substr(0, 18) + "...";	
			else
				cut = pieces[0];

			//wprintf("%s  %s  %s\n",  pieces[0].c_str(), pieces[1].c_str(),
			//	pieces[2].c_str(), pieces[3].c_str());

			if (NoDuplicate(pieces[0]) && pieces.size() >= 3)
			{	
				all_files.push_back(pieces[0]);
				if (pieces[3] == "GC")
				{
					gc_files.push_back(pieces[0]);
					m_gc->Append(wxString::FromAscii(cut.c_str()));			
				}
				else
				{
					wii_files.push_back(pieces[0]);
					m_wii->Append(wxString::FromAscii(cut.c_str()));
				}				
			}				
		}
	}
}



// =======================================================================================
// Check for duplicates and count files from all_all_files
// --------------
bool CDebugger::NoDuplicate(std::string FileName)
{
	for (u32 i = 0; i < all_files.size(); i++)
	{
		if(all_files.at(i) == FileName)
			return false;
	}
	return true;
}

// Count the number of files for each game
u32 CDebugger::CountFiles(std::string FileName)
{
	int match = 0;

	for (u32 i = 0; i < all_all_files.size(); i++)
	{
		//wprintf("CountFiles  %i  %s\n", i, all_all_files[i].c_str());
		if(all_all_files[i] == FileName)
		match++;
	}
	//wprintf("We found  %i  files for this game\n", match);
	return match;
}
// ==============


// =======================================================================================
// Read file from harddrive
// --------------
std::string CDebugger::Readfile_(std::string FileName)
{
	char c;     // declare a char variable
	FILE *file; // declare a FILE pointer
	std::string sz = "";

	if(File::Exists(FileName.c_str()))
		file = fopen(FileName.c_str(), "r"); // open a text file for reading
	else
		return "";

	if(file == NULL)
	{
		// file could not be opened
	}
	else
	{
		while(1) // looping through file
		{     
			c = fgetc(file);

			if(c != EOF)
				sz += c; // print the file one character at a time
			else
				break; // break when EOF is reached
		}
	
                fclose(file);
	}
        return sz;
}

// Read file
void CDebugger::Readfile(std::string FileName, bool GC)
{
	u32 n = CountFiles(FileName); // count how many mails we have
	u32 curr_n = 0;
	std::ifstream file;
	for (u32 i = 0; i < m_RadioBox[3]->GetCount(); i++)
	{
		if(m_RadioBox[3]->IsItemEnabled(i)) curr_n++;
		m_RadioBox[3]->Enable(i, false); // disable all
	}
	//wprintf("Disabled all: n %i\n", n);


	for (u32 i = 0; i < n; i++)
	{
		m_RadioBox[3]->Enable(i, true); // then anble the right ones
		//wprintf("m_RadioBox[3] enabled:  %i\n", i);

		std::string sz = "";
                std::ostringstream ci;
                ci << i;
		std::string f0 = FULL_MAIL_LOGS_DIR + FileName + "_sep" + ci.str() + "_sep" + "0_sep" + (GC ? "GC" : "Wii") +  "_sep.log";
		std::string f1 = FULL_MAIL_LOGS_DIR + FileName + "_sep" + ci.str() + "_sep" + "1_sep" + (GC ? "GC" : "Wii") +  "_sep.log";

		//wprintf("ifstream  %s  %s\n", f0.c_str(), f1.c_str());

		if(sFullMail.size() <= i) sFullMail.resize(sFullMail.size() + 1);
		if(sMailLog.size() <= i) sMailLog.resize(sMailLog.size() + 1);

		if(Readfile_(f0).length() > 0) sFullMail.at(i) = Readfile_(f0);
			else sFullMail.at(i) = "";
		if(Readfile_(f1).length() > 0) sMailLog.at(i) = Readfile_(f1);
			else sMailLog.at(i) = "";
	}
	if(n < curr_n) m_RadioBox[3]->Select(n - 1);
	//wprintf("Select: %i  | n %i  curr_n %i\n", n - 1, n, curr_n);
	DoUpdateMail();
}
// ==============


// =======================================================================================
// Read the file to the text window
// ---------------
void CDebugger::OnGameChange(wxCommandEvent& event)
{
    if(event.GetId() == 2006)
	{
            // Only allow one selected game at a time
            for (u32 i = 0; i < m_gc->GetCount(); ++i)
                if(i != (u32)event.GetInt()) m_gc->Check(i, false);
            for (u32 i = 0; i < m_wii->GetCount(); ++i)
                m_wii->Check(i, false);
            Readfile(gc_files[event.GetInt()], true);
	}
    else
	{
            for (u32 i = 0; i < m_gc->GetCount(); ++i)
                m_gc->Check(i, false);
            for (u32 i = 0; i < m_wii->GetCount(); ++i)
                if(i != (u32)event.GetInt()) m_wii->Check(i, false);
            Readfile(wii_files[event.GetInt()], false);
	}
}

// Settings
void CDebugger::MailSettings(wxCommandEvent& event)
{
	//for (int i = 0; i < all_all_files.size(); ++i)
		//wprintf("s: %s \n", all_all_files.at(i).c_str());

	ScanMails = m_gcwiiset->IsChecked(0);
	StoreMails = m_gcwiiset->IsChecked(1);
}



