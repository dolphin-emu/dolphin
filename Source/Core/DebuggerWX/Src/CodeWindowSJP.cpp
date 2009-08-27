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

/////////////////////////////
// What does SJP stand for???

//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
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
#include "../../DolphinWX/Src/Globals.h"
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
///////////////////////////////////



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
wxWindow * CCodeWindow::GetNootebookPage(wxString Name)
{
	if (!m_NB0 || !m_NB1) return NULL;

	for(u32 i = 0; i <= m_NB0->GetPageCount(); i++)
	{
		if (m_NB0->GetPageText(i).IsSameAs(Name)) return m_NB0->GetPage(i);
	}	
	for(u32 i = 0; i <= m_NB1->GetPageCount(); i++)
	{
		if (m_NB1->GetPageText(i).IsSameAs(Name)) return m_NB1->GetPage(i);
	}
	return NULL;
}
#ifdef _WIN32
wxWindow * CCodeWindow::GetWxWindow(wxString Name)
{
	HWND hWnd = ::FindWindow(NULL, Name.c_str());
	if (hWnd)
	{
		wxWindow * Win = new wxWindow();
		Win->SetHWND((WXHWND)hWnd);
		Win->AdoptAttributesFromHWND();
		return Win;
	}
	else if (GetParent()->GetParent()->FindWindowByName(Name))
	{
		return GetParent()->GetParent()->FindWindowByName(Name);
	}
	else if (GetParent()->GetParent()->FindWindowByLabel(Name))
	{
		return GetParent()->GetParent()->FindWindowByLabel(Name);
	}
	else if (GetNootebookPage(Name))
	{
		return GetNootebookPage(Name);
	}
	else
		return NULL;
}
#endif
void CCodeWindow::OpenPages()
{	
	if (bRegisterWindow)	OnToggleRegisterWindow(true, m_NB0);
	if (bBreakpointWindow)	OnToggleBreakPointWindow(true, m_NB1);
	if (bMemoryWindow)		OnToggleMemoryWindow(true, m_NB0);
	if (bJitWindow)			OnToggleJitWindow(true, m_NB0);
	if (bSoundWindow)		OnToggleSoundWindow(true, m_NB1);
	if (bVideoWindow)		OnToggleVideoWindow(true, m_NB1);
}
void CCodeWindow::OnToggleWindow(wxCommandEvent& event)
{
	DoToggleWindow(event.GetId(), GetMenuBar()->IsChecked(event.GetId()));
}
void CCodeWindow::DoToggleWindow(int Id, bool Show)
{
	switch (Id)
	{
		case IDM_REGISTERWINDOW: OnToggleRegisterWindow(Show, m_NB0); break;
		case IDM_BREAKPOINTWINDOW: OnToggleBreakPointWindow(Show, m_NB1); break;
		case IDM_MEMORYWINDOW: OnToggleMemoryWindow(Show, m_NB0); break;
		case IDM_JITWINDOW: OnToggleJitWindow(Show, m_NB0); break;
		case IDM_SOUNDWINDOW: OnToggleSoundWindow(Show, m_NB1); break;
		case IDM_VIDEOWINDOW: OnToggleVideoWindow(Show, m_NB1); break;
	}	
}
void CCodeWindow::OnToggleRegisterWindow(bool Show, wxAuiNotebook * _NB)
{
	if (Show)
	{
		if (m_RegisterWindow && _NB->GetPageIndex(m_RegisterWindow) != wxNOT_FOUND) return;
		if (!m_RegisterWindow) m_RegisterWindow = new CRegisterWindow(GetParent()->GetParent());		
		_NB->AddPage(m_RegisterWindow, wxT("Registers"), true, aNormalFile );
	}
	else // hide
	{
		// If m_dialog is NULL, then possibly the system
		// didn't report the checked menu item status correctly.
		// It should be true just after the menu item was selected,
		// if there was no modeless dialog yet.
		wxASSERT(m_RegisterWindow != NULL);
		//if (m_RegisterWindow) m_RegisterWindow->Hide();
		if (m_RegisterWindow)
		{
			if (m_NB0->GetPageIndex(m_RegisterWindow) != wxNOT_FOUND)
				m_NB0->RemovePage(m_NB0->GetPageIndex(m_RegisterWindow));
			else
				m_NB1->RemovePage(m_NB1->GetPageIndex(m_RegisterWindow));
			m_RegisterWindow->Hide();
		}
	}
}


void CCodeWindow::OnToggleBreakPointWindow(bool Show, wxAuiNotebook * _NB)
{
	if (Show)
	{
		if (m_BreakpointWindow && _NB->GetPageIndex(m_BreakpointWindow) != wxNOT_FOUND) return;
		if (!m_BreakpointWindow) m_BreakpointWindow = new CBreakPointWindow(this, GetParent()->GetParent());
		_NB->AddPage(m_BreakpointWindow, wxT("Breakpoints"), true, aNormalFile );
	}
	else // hide
	{
		// If m_dialog is NULL, then possibly the system
		// didn't report the checked menu item status correctly.
		// It should be true just after the menu item was selected,
		// if there was no modeless dialog yet.
		wxASSERT(m_BreakpointWindow != NULL);

		if (m_BreakpointWindow)
		{
			if (m_NB0->GetPageIndex(m_BreakpointWindow) != wxNOT_FOUND)
				m_NB0->RemovePage(m_NB0->GetPageIndex(m_BreakpointWindow));
			else
				m_NB1->RemovePage(m_NB1->GetPageIndex(m_BreakpointWindow));
			m_BreakpointWindow->Hide();
		}
	}
}

void CCodeWindow::OnToggleJitWindow(bool Show, wxAuiNotebook * _NB)
{
	if (Show)
	{
		if (m_JitWindow && _NB->GetPageIndex(m_JitWindow) != wxNOT_FOUND) return;
		if (!m_JitWindow) m_JitWindow = new CJitWindow(GetParent()->GetParent());
		_NB->AddPage(m_JitWindow, wxT("JIT"), true, aNormalFile );
	}
	else // hide
	{
		// If m_dialog is NULL, then possibly the system
		// didn't report the checked menu item status correctly.
		// It should be true just after the menu item was selected,
		// if there was no modeless dialog yet.
		wxASSERT(m_JitWindow != NULL);

		if (m_JitWindow)
		{
			if (m_NB0->GetPageIndex(m_JitWindow) != wxNOT_FOUND)
				m_NB0->RemovePage(m_NB0->GetPageIndex(m_JitWindow));
			else
				m_NB1->RemovePage(m_NB1->GetPageIndex(m_JitWindow));
			m_JitWindow->Hide();
		}
	}
}


void CCodeWindow::OnToggleMemoryWindow(bool Show, wxAuiNotebook * _NB)
{
	if (Show)
	{
		if (m_MemoryWindow && _NB->GetPageIndex(m_MemoryWindow) != wxNOT_FOUND) return;
		if (!m_MemoryWindow) m_MemoryWindow = new CMemoryWindow(GetParent()->GetParent());
		_NB->AddPage(m_MemoryWindow, wxT("Memory"), true, aNormalFile );
	}
	else // hide
	{
		// If m_dialog is NULL, then possibly the system
		// didn't report the checked menu item status correctly.
		// It should be true just after the menu item was selected,
		// if there was no modeless dialog yet.
		wxASSERT(m_MemoryWindow != NULL);

		if (m_MemoryWindow)
		{
			if (m_NB0->GetPageIndex(m_MemoryWindow) != wxNOT_FOUND)
				m_NB0->RemovePage(m_NB0->GetPageIndex(m_MemoryWindow));
			else
				m_NB1->RemovePage(m_NB1->GetPageIndex(m_MemoryWindow));
			m_MemoryWindow->Hide();
		}
	}
}

//Toggle Sound Debugging Window
void CCodeWindow::OnToggleSoundWindow(bool Show, wxAuiNotebook * _NB)
{
	//ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();

	if (Show)
	{
		#ifdef _WIN32
		wxWindow *Win = GetWxWindow(wxT("Sound"));
		if (Win && _NB->GetPageIndex(Win) != wxNOT_FOUND) return;

		{
		#endif				
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("OpenDebug\n").c_str());
			CPluginManager::GetInstance().OpenDebug(
				GetParent()->GetParent()->GetHandle(),
				//GetHandle(),
				SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str(),
				PLUGIN_TYPE_DSP, true // DSP, show
				);
		#ifdef _WIN32
		}

		Win = GetWxWindow(wxT("Sound"));
		if (Win)
		{			
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("AddPage\n").c_str());
			_NB->AddPage(Win, wxT("Sound"), true, aNormalFile );
		}
		#endif
	}
	else // hide
	{
		#ifdef _WIN32
		wxWindow *Win = GetWxWindow(wxT("Sound"));
		if (Win)
		{
			if (m_NB0->GetPageIndex(Win) != wxNOT_FOUND)
				m_NB0->RemovePage(m_NB0->GetPageIndex(Win));
			else
				m_NB1->RemovePage(m_NB1->GetPageIndex(Win));
		}
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
void CCodeWindow::OnToggleVideoWindow(bool Show, wxAuiNotebook * _NB)
{
	//GetMenuBar()->Check(event.GetId(), false); // Turn off

	if (Show)
	{
		#ifdef _WIN32
		wxWindow *Win = GetWxWindow(wxT("Video"));
		if (Win && _NB->GetPageIndex(Win) != wxNOT_FOUND) return;

		{
		#endif	
			// Show and/or create the window
			CPluginManager::GetInstance().OpenDebug(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str(),
			PLUGIN_TYPE_VIDEO, true // Video, show
			);
		#ifdef _WIN32
		}

		Win = GetWxWindow(wxT("Video"));
		if (Win) _NB->AddPage(Win, wxT("Video"), true, aNormalFile );
		#endif
	}
	else // hide
	{
		#ifdef _WIN32
		wxWindow *Win = GetWxWindow(wxT("Video"));
		if (Win)
		{
			if (m_NB0->GetPageIndex(Win) != wxNOT_FOUND)
				m_NB0->RemovePage(m_NB0->GetPageIndex(Win));
			else
				m_NB1->RemovePage(m_NB1->GetPageIndex(Win));
		}
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