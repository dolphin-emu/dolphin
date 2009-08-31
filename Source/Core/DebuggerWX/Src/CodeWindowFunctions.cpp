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



//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "Common.h"

#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <wx/mstream.h>
#include <wx/tipwin.h>
#include <wx/fontdlg.h>

// ugly that this lib included code from the main
#include "../../DolphinWX/Src/WxUtils.h"

#include "Host.h"

#include "Debugger.h"
#include "DebuggerUIUtil.h"

#include "RegisterWindow.h"
#include "BreakpointWindow.h"
#include "MemoryWindow.h"
#include "JitWindow.h"
#include "FileUtil.h"

#include "CodeWindow.h"
#include "CodeView.h"

#include "Core.h"
#include "HLE/HLE.h"
#include "Boot/Boot.h"
#include "LogManager.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "Debugger/PPCDebugInterface.h"
#include "Debugger/Debugger_SymbolMap.h"
#include "PowerPC/PPCAnalyst.h"
#include "PowerPC/Profiler.h"
#include "PowerPC/PPCSymbolDB.h"
#include "PowerPC/SignatureDB.h"
#include "PowerPC/PPCTables.h"
#include "PowerPC/Jit64/Jit.h"
#include "PowerPC/JitCommon/JitCache.h" // for ClearCache()

#include "PluginManager.h"
#include "ConfigManager.h"


extern "C"  // Bitmaps
{
	#include "../resources/toolbar_play.c"
	#include "../resources/toolbar_pause.c"
	#include "../resources/toolbar_add_memorycheck.c"
	#include "../resources/toolbar_delete.c"
	#include "../resources/toolbar_add_breakpoint.c"
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Symbols, JIT, Profiler
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CCodeWindow::CreateSymbolsMenu()
{
	wxMenu *pSymbolsMenu = new wxMenu;
	pSymbolsMenu->Append(IDM_CLEARSYMBOLS, _T("&Clear symbols"));
	// pSymbolsMenu->Append(IDM_CLEANSYMBOLS, _T("&Clean symbols (zz)"));
	pSymbolsMenu->Append(IDM_SCANFUNCTIONS, _T("&Generate symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_LOADMAPFILE, _T("&Load symbol map"));
	pSymbolsMenu->Append(IDM_SAVEMAPFILE, _T("&Save symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_SAVEMAPFILEWITHCODES, _T("Save code"),
		wxString::FromAscii("Save the entire disassembled code. This may take a several seconds"
		" and may require between 50 and 100 MB of hard drive space. It will only save code"
		" that are in the first 4 MB of memory, if you are debugging a game that load .rel"
		" files with code to memory you may want to increase that to perhaps 8 MB, you can do"
		" that from SymbolDB::SaveMap().")
		);

	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_CREATESIGNATUREFILE, _T("&Create signature file..."));
	pSymbolsMenu->Append(IDM_USESIGNATUREFILE, _T("&Use signature file..."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_PATCHHLEFUNCTIONS, _T("&Patch HLE functions"));
    pSymbolsMenu->Append(IDM_RENAME_SYMBOLS, _T("&Rename symbols from file..."));
	pMenuBar->Append(pSymbolsMenu, _T("&Symbols"));

	wxMenu *pJitMenu = new wxMenu;
	pJitMenu->Append(IDM_CLEARCODECACHE, _T("&Clear code cache"));
	pJitMenu->Append(IDM_LOGINSTRUCTIONS, _T("&Log JIT instruction coverage"));
	pJitMenu->Append(IDM_SEARCHINSTRUCTION, _T("&Search for an op"));
	pMenuBar->Append(pJitMenu, _T("&JIT"));

	wxMenu *pProfilerMenu = new wxMenu;
	pProfilerMenu->Append(IDM_PROFILEBLOCKS, _T("&Profile blocks"), wxEmptyString, wxITEM_CHECK);
	pProfilerMenu->AppendSeparator();
	pProfilerMenu->Append(IDM_WRITEPROFILE, _T("&Write to profile.txt, show"));
	pMenuBar->Append(pProfilerMenu, _T("&Profiler"));
}


void CCodeWindow::OnProfilerMenu(wxCommandEvent& event)
{
	if (Core::GetState() == Core::CORE_RUN) {
		event.Skip();
		return;
	}
	switch (event.GetId())
	{
	case IDM_PROFILEBLOCKS:
		jit.ClearCache();
		Profiler::g_ProfileBlocks = GetMenuBar()->IsChecked(IDM_PROFILEBLOCKS);
		break;
	case IDM_WRITEPROFILE:
		Profiler::WriteProfileResults("profiler.txt");
		WxUtils::Launch("profiler.txt");
		break;
	}
}

void CCodeWindow::OnSymbolsMenu(wxCommandEvent& event)
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
	{
		// TODO: disable menu items instead :P
		return;
	}
	std::string mapfile = CBoot::GenerateMapFilename();
	switch (event.GetId())
	{
	case IDM_CLEARSYMBOLS:
		g_symbolDB.Clear();
		Host_NotifyMapLoaded();
		break;
	case IDM_CLEANSYMBOLS:
		g_symbolDB.Clear("zz");
		Host_NotifyMapLoaded();
		break;
	case IDM_SCANFUNCTIONS:
		{
		PPCAnalyst::FindFunctions(0x80000000, 0x80400000, &g_symbolDB);
		SignatureDB db;
		if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
			db.Apply(&g_symbolDB);

		// HLE::PatchFunctions();
		NotifyMapLoaded();
		break;
		}
	case IDM_LOADMAPFILE:
		if (!File::Exists(mapfile.c_str()))
		{
			g_symbolDB.Clear();
			PPCAnalyst::FindFunctions(0x81300000, 0x81800000, &g_symbolDB);
			SignatureDB db;
			if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
				db.Apply(&g_symbolDB);
		} else {
			g_symbolDB.LoadMap(mapfile.c_str());
		}
        HLE::PatchFunctions();
		NotifyMapLoaded();
		break;
	case IDM_SAVEMAPFILE:
		g_symbolDB.SaveMap(mapfile.c_str());
		break;
	case IDM_SAVEMAPFILEWITHCODES:
		g_symbolDB.SaveMap(mapfile.c_str(), true);
		break;

    case IDM_RENAME_SYMBOLS:
        {
            wxString path = wxFileSelector(
                _T("Apply signature file"), wxEmptyString, wxEmptyString, wxEmptyString,
                _T("Dolphin Symbole Rename File (*.sym)|*.sym;"), wxFD_OPEN | wxFD_FILE_MUST_EXIST,
                this);
            if (path) 
            {
                FILE *f = fopen(path.mb_str(), "r");
                if (!f)
                    return;

                bool started = false;
                while (!feof(f))
                {
                    char line[512];
                    fgets(line, 511, f);
                    if (strlen(line) < 4)
                        continue;

                    u32 address, type;
                    char name[512];
                    sscanf(line, "%08x %02i %s", &address, &type, name);

                    Symbol *symbol = g_symbolDB.GetSymbolFromAddr(address);
                    if (symbol) {
                        symbol->name = line+12;
                    }
                }
                fclose(f);
                Host_NotifyMapLoaded();
            }
        }
        break;

	case IDM_CREATESIGNATUREFILE:
		{
		wxTextEntryDialog input_prefix(this, wxString::FromAscii("Only export symbols with prefix:"), wxGetTextFromUserPromptStr, _T("."));
		if (input_prefix.ShowModal() == wxID_OK) {
			std::string prefix(input_prefix.GetValue().mb_str());

			wxString path = wxFileSelector(
					_T("Save signature as"), wxEmptyString, wxEmptyString, wxEmptyString,
					_T("Dolphin Signature File (*.dsy)|*.dsy;"), wxFD_SAVE,
					this);
			if (path) {
				SignatureDB db;
				db.Initialize(&g_symbolDB, prefix.c_str());
				std::string filename(path.mb_str());		// PPCAnalyst::SaveSignatureDB(
				db.Save(path.mb_str());
			}
		}
		}
		break;
	case IDM_USESIGNATUREFILE:
		{
		wxString path = wxFileSelector(
				_T("Apply signature file"), wxEmptyString, wxEmptyString, wxEmptyString,
				_T("Dolphin Signature File (*.dsy)|*.dsy;"), wxFD_OPEN | wxFD_FILE_MUST_EXIST,
				this);
		if (path) {
			SignatureDB db;
			db.Load(path.mb_str());
			db.Apply(&g_symbolDB);
		}
		}
		NotifyMapLoaded();
		break;
	case IDM_PATCHHLEFUNCTIONS:
		HLE::PatchFunctions();
		Update();
		break;
	}
}


void CCodeWindow::NotifyMapLoaded()
{
	g_symbolDB.FillInCallers();
	//symbols->Show(false); // hide it for faster filling
	symbols->Freeze();	// HyperIris: wx style fast filling
	symbols->Clear();
	for (PPCSymbolDB::XFuncMap::iterator iter = g_symbolDB.GetIterator(); iter != g_symbolDB.End(); iter++)
	{
		int idx = symbols->Append(wxString::FromAscii(iter->second.name.c_str()));
		symbols->SetClientData(idx, (void*)&iter->second);
	}
	symbols->Thaw();
	//symbols->Show(true);
	Update();
}


void CCodeWindow::OnSymbolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	if (index >= 0) {
		Symbol* pSymbol = static_cast<Symbol *>(symbols->GetClientData(index));
		if (pSymbol != NULL)
		{
			if(pSymbol->type == Symbol::SYMBOL_DATA)
			{
				if(m_MemoryWindow && m_MemoryWindow->IsVisible())
					m_MemoryWindow->JumpToAddress(pSymbol->address);
			}
			else
			{
				JumpToAddress(pSymbol->address);
			}
		}
	}
}

void CCodeWindow::OnSymbolListContextMenu(wxContextMenuEvent& event)
{
}


// Change the global DebuggerFont
void CCodeWindow::OnChangeFont(wxCommandEvent& event)
{
	wxFontData data;
	data.SetInitialFont(GetFont());

	wxFontDialog dialog(this, data);
	if ( dialog.ShowModal() == wxID_OK )
		DebuggerFont = dialog.GetFontData().GetChosenFont();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toogle windows
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void CCodeWindow::OpenPages()
{	
							Parent->DoToggleWindow(IDM_CODEWINDOW, true);
	if (bRegisterWindow)	Parent->DoToggleWindow(IDM_REGISTERWINDOW, true);
	if (bBreakpointWindow)	Parent->DoToggleWindow(IDM_BREAKPOINTWINDOW, true);
	if (bMemoryWindow)		Parent->DoToggleWindow(IDM_MEMORYWINDOW, true);
	if (bJitWindow)			Parent->DoToggleWindow(IDM_JITWINDOW, true);
	if (bSoundWindow)		Parent->DoToggleWindow(IDM_SOUNDWINDOW, true);
	if (bVideoWindow)		Parent->DoToggleWindow(IDM_VIDEOWINDOW, true);
}
void CCodeWindow::OnToggleWindow(wxCommandEvent& event)
{
	Parent->DoToggleWindow(event.GetId(), GetMenuBar()->IsChecked(event.GetId()));
}
void CCodeWindow::OnToggleCodeWindow(bool Show, int i)
{
	if (Show)
	{
		Parent->DoAddPage(this, i, "Code");
	}
	else // hide
		Parent->DoRemovePage (this);
}
void CCodeWindow::OnToggleRegisterWindow(bool Show, int i)
{
	if (Show)
	{
		if (!m_RegisterWindow) m_RegisterWindow = new CRegisterWindow(Parent);
		Parent->DoAddPage(m_RegisterWindow, i, "Registers");
	}
	else // hide
		Parent->DoRemovePage (m_RegisterWindow);
}

void CCodeWindow::OnToggleBreakPointWindow(bool Show, int i)
{
	if (Show)
	{
		if (!m_BreakpointWindow) m_BreakpointWindow = new CBreakPointWindow(this, Parent);
		Parent->DoAddPage(m_BreakpointWindow, i, "Breakpoints");
	}
	else // hide
		Parent->DoRemovePage(m_BreakpointWindow);
}

void CCodeWindow::OnToggleJitWindow(bool Show, int i)
{
	if (Show)
	{
		if (!m_JitWindow) m_JitWindow = new CJitWindow(Parent);
		Parent->DoAddPage(m_JitWindow, i, "JIT");
	}
	else // hide
		Parent->DoRemovePage(m_JitWindow);
}


void CCodeWindow::OnToggleMemoryWindow(bool Show, int i)
{
	if (Show)
	{
		if (!m_MemoryWindow) m_MemoryWindow = new CMemoryWindow(Parent);
		Parent->DoAddPage(m_MemoryWindow, i, "Memory");
	}
	else // hide
		Parent->DoRemovePage(m_MemoryWindow);
}

//Toggle Sound Debugging Window
void CCodeWindow::OnToggleSoundWindow(bool Show, int i)
{
	//ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();

	if (Show)
	{
		if (Parent->GetNotebookCount() == 0) return;
		if (i < 0 || i > Parent->GetNotebookCount()-1) i = 0;
		#ifdef _WIN32
		wxWindow *Win = Parent->GetWxWindow(wxT("Sound"));
		if (Win && Parent->GetNotebook(i)->GetPageIndex(Win) != wxNOT_FOUND) return;

		{
		#endif				
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("OpenDebug\n").c_str());
			CPluginManager::GetInstance().OpenDebug(
				Parent->GetHandle(),
				//GetHandle(),
				SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str(),
				PLUGIN_TYPE_DSP, true // DSP, show
				);
		#ifdef _WIN32
		}

		Win = Parent->GetWxWindow(wxT("Sound"));
		if (Win)
		{			
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("AddPage\n").c_str());
			Parent->GetNotebook(i)->AddPage(Win, wxT("Sound"), true, Parent->aNormalFile );
		}
		#endif
	}
	else // hide
	{
		#ifdef _WIN32
		wxWindow *Win = Parent->GetWxWindow(wxT("Sound"));
		Parent->DoRemovePage(Win, false);
		#endif
		// Close the sound dll that has an open debugger
		CPluginManager::GetInstance().OpenDebug(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str(),
			PLUGIN_TYPE_DSP, false // DSP, hide
			);
	}
}

// Toggle Video Debugging Window
void CCodeWindow::OnToggleVideoWindow(bool Show, int i)
{
	//GetMenuBar()->Check(event.GetId(), false); // Turn off

	if (Show)
	{
		if (Parent->GetNotebookCount() == 0) return;
		if (i < 0 || i > Parent->GetNotebookCount()-1) i = 0;
		#ifdef _WIN32
		wxWindow *Win = Parent->GetWxWindow(wxT("Video"));
		if (Win && Parent->GetNotebook(i)->GetPageIndex(Win) != wxNOT_FOUND) return;

		{
		#endif	
			// Show and/or create the window
			CPluginManager::GetInstance().OpenDebug(
			Parent->GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str(),
			PLUGIN_TYPE_VIDEO, true // Video, show
			);
		#ifdef _WIN32
		}

		Win = Parent->GetWxWindow(wxT("Video"));
		if (Win) Parent->GetNotebook(i)->AddPage(Win, wxT("Video"), true, Parent->aNormalFile );
		#endif
	}
	else // hide
	{
		#ifdef _WIN32
		wxWindow *Win = Parent->GetWxWindow(wxT("Video"));
		Parent->DoRemovePage (Win, false);
		#endif
		// Close the video dll that has an open debugger
		CPluginManager::GetInstance().OpenDebug(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str(),
			PLUGIN_TYPE_VIDEO, false // Video, hide
			);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////