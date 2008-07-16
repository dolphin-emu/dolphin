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

#include <string>
#include <vector>

#include "Common.h"
#include "Globals.h"
#include "FileSearch.h"
#include "FileUtil.h"
#include "PluginManager.h"
#include "StringUtil.h"

CPluginManager CPluginManager::m_Instance;


CPluginManager::CPluginManager()
{}


CPluginManager::~CPluginManager()
{}


void
CPluginManager::ScanForPlugins(wxWindow* _wxWindow)
{
	m_PluginInfos.clear();

	CFileSearch::XStringVector Directories;
	Directories.push_back(_T("Plugins"));

	CFileSearch::XStringVector Extensions;
#ifdef _WIN32
	Extensions.push_back("*.dll");
#else
	Extensions.push_back("*.so");
#endif

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		wxProgressDialog dialog(_T("Scanning for Plugins"),
					_T("Scanning..."),
					(int)rFilenames.size(), // range
					_wxWindow, // parent
					wxPD_CAN_ABORT |
					wxPD_APP_MODAL |
					// wxPD_AUTO_HIDE | -- try this as well
					wxPD_ELAPSED_TIME |
					wxPD_ESTIMATED_TIME |
					wxPD_REMAINING_TIME |
					wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
					);
		dialog.CenterOnParent();

		for (size_t i = 0; i < rFilenames.size(); i++)
		{
			std::string orig_name = rFilenames[i];
			printf("Scanning %s\n", rFilenames[i].c_str());
			std::string FileName;

			if (!SplitPath(rFilenames[i], NULL, &FileName, NULL))
			{
				printf("Bad Path %s\n", rFilenames[i].c_str());
				return;
			}

			wxString msg;
			msg.Printf("Scanning %s", FileName.c_str());
			bool Cont = dialog.Update((int)i, msg);

			if (!Cont)
			{
				break;
			}

			printf("Examining %s\n", FileName.c_str());

			CPluginInfo PluginInfo(orig_name);
			printf("%s\n", orig_name.c_str());

			if (PluginInfo.IsValid())
			{
				m_PluginInfos.push_back(PluginInfo);
			}

			printf("Valid plugin\n");
		}
	}
}


void
CPluginManager::OpenAbout(void* _Parent, const std::string& _rFilename)
{
	if (Common::CPlugin::Load(_rFilename.c_str()))
	{
		Common::CPlugin::Config((HWND)_Parent);
		Common::CPlugin::Release();
	}
}


void
CPluginManager::OpenConfig(void* _Parent, const std::string& _rFilename)
{
	if (Common::CPlugin::Load(_rFilename.c_str()))
	{
		Common::CPlugin::Config((HWND)_Parent);
		Common::CPlugin::Release();
	}
}


CPluginInfo::CPluginInfo(const std::string& _rFileName)
	: m_FileName(_rFileName)
	, m_Valid(false)
{
	printf("Loading!\n");

	if (Common::CPlugin::Load(_rFileName.c_str()))
	{
		printf("Loaded!\n");

		if (Common::CPlugin::GetInfo(m_PluginInfo))
		{
			m_Valid = true;
		}
		else
		{
			PanicAlert("Could not get info about plugin %s", _rFileName.c_str());
		}

		Common::CPlugin::Release();
		printf("Unloaded!\n");
	}
	else
	{
		if (!File::Exists(_rFileName)) {
			PanicAlert("Could not load plugin %s - file does not exist", _rFileName.c_str());
		} else {
			PanicAlert("Failed to load plugin %s - unknown error.\n", _rFileName.c_str());
		}
	}
}


