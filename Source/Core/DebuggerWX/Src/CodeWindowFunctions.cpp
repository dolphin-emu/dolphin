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




// Include
// --------------
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
#include "PowerPC/JitCommon/JitBase.h"
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



// Save and load settings
// -----------------------------
void CCodeWindow::Load()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	// The font to override DebuggerFont with
	std::string fontDesc;
	Section& showonstart = ini["ShowOnStart"];
	showonstart.Get("DebuggerFont", &fontDesc);
	if (!fontDesc.empty())
		DebuggerFont.SetNativeFontInfoUserDesc(wxString::FromAscii(fontDesc.c_str()));

	// Decide what windows to use
	// This stuff really doesn't belong in CodeWindow anymore, does it? It should be
	// in Frame.cpp somewhere, even though it's debugger stuff.
	showonstart.Get("Code", &bCodeWindow, true);
	showonstart.Get("Registers", &bRegisterWindow, false);
	showonstart.Get("Breakpoints", &bBreakpointWindow, false);
	showonstart.Get("Memory", &bMemoryWindow, false);
	showonstart.Get("JIT", &bJitWindow, false);
	showonstart.Get("Sound", &bSoundWindow, false);
	showonstart.Get("Video", &bVideoWindow, false);
	// Get notebook affiliation
	Section& section = ini[StringFromFormat("P - %s",
		(Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives.at(Parent->ActivePerspective).Name.c_str() : "")];
	section.Get("Log", &iLogWindow, 1);
	section.Get("Console", &iConsoleWindow, 1);
	section.Get("Code", &iCodeWindow, 1);
	section.Get("Registers", &iRegisterWindow, 1);
	section.Get("Breakpoints", &iBreakpointWindow, 0);
	section.Get("Memory", &iMemoryWindow, 1);
	section.Get("JIT", &iJitWindow, 1);
	section.Get("Sound", &iSoundWindow, 0);
	section.Get("Video", &iVideoWindow, 0);
	// Get floating setting
	Section& flt = ini["Float"];
	flt.Get("Log", &Parent->bFloatLogWindow, false);
	flt.Get("Console", &Parent->bFloatConsoleWindow, false);
	flt.Get("Code", &bFloatCodeWindow, false);
	flt.Get("Registers", &bFloatRegisterWindow, false);
	flt.Get("Breakpoints", &bFloatBreakpointWindow, false);
	flt.Get("Memory", &bFloatMemoryWindow, false);
	flt.Get("JIT", &bFloatJitWindow, false);
	flt.Get("Sound", &bFloatSoundWindow, false);
	flt.Get("Video", &bFloatVideoWindow, false);

	// Boot to pause or not
	showonstart.Get("AutomaticStart", &bAutomaticStart, false);
	showonstart.Get("BootToPause", &bBootToPause, true);
}
void CCodeWindow::Save()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	Section& showonstart = ini["ShowOnStart"];
	showonstart.Set("DebuggerFont", std::string(DebuggerFont.GetNativeFontInfoUserDesc().mb_str()));

	// Boot to pause or not
	showonstart.Set("AutomaticStart", GetMenuBar()->IsChecked(IDM_AUTOMATICSTART));
	showonstart.Set("BootToPause", GetMenuBar()->IsChecked(IDM_BOOTTOPAUSE));

	// Save windows settings	
	//showonstart.Set("Code", GetMenuBar()->IsChecked(IDM_CODEWINDOW));
	showonstart.Set("Registers", GetMenuBar()->IsChecked(IDM_REGISTERWINDOW));
	showonstart.Set("Breakpoints", GetMenuBar()->IsChecked(IDM_BREAKPOINTWINDOW));
	showonstart.Set("Memory", GetMenuBar()->IsChecked(IDM_MEMORYWINDOW));
	showonstart.Set("JIT", GetMenuBar()->IsChecked(IDM_JITWINDOW));
	showonstart.Set("Sound", GetMenuBar()->IsChecked(IDM_SOUNDWINDOW));
	showonstart.Set("Video", GetMenuBar()->IsChecked(IDM_VIDEOWINDOW));		
	Section& section = ini[StringFromFormat("P - %s",
		(Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives.at(Parent->ActivePerspective).Name.c_str() : "")];
	section.Set("Log", iLogWindow);
	section.Set("Console", iConsoleWindow);
	section.Set("Code", iCodeWindow);
	section.Set("Registers", iRegisterWindow);
	section.Set("Breakpoints", iBreakpointWindow);
	section.Set("Memory", iMemoryWindow);
	section.Set("JIT", iJitWindow);
	section.Set("Sound", iSoundWindow);
	section.Set("Video", iVideoWindow);
	// Save floating setting
	Section& flt = ini["Float"];
	flt.Set("Log", !!FindWindowById(IDM_LOGWINDOW_PARENT));
	flt.Set("Console", !!FindWindowById(IDM_CONSOLEWINDOW_PARENT));
	flt.Set("Code", !!FindWindowById(IDM_CODEWINDOW_PARENT));
	flt.Set("Registers", !!FindWindowById(IDM_REGISTERWINDOW_PARENT));
	flt.Set("Breakpoints", !!FindWindowById(IDM_BREAKPOINTWINDOW_PARENT));
	flt.Set("Memory", !!FindWindowById(IDM_MEMORYWINDOW_PARENT));
	flt.Set("JIT", !!FindWindowById(IDM_JITWINDOW_PARENT));
	flt.Set("Sound", !!FindWindowById(IDM_SOUNDWINDOW_PARENT));
	flt.Set("Video", !!FindWindowById(IDM_VIDEOWINDOW_PARENT));	

	ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}


// Symbols, JIT, Profiler
// ----------------
void CCodeWindow::CreateMenuSymbols()
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
		jit->ClearCache();
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
	Parent->ClearStatusBar();

	if (Core::GetState() == Core::CORE_UNINITIALIZED) return;

	std::string mapfile = CBoot::GenerateMapFilename();
	switch (event.GetId())
	{
	case IDM_CLEARSYMBOLS:
		if(!AskYesNo("Do you want to clear the list of symbol names?", "Confirm", wxYES_NO)) return;
		g_symbolDB.Clear();
		Host_NotifyMapLoaded();
		break;
	case IDM_CLEANSYMBOLS:
		g_symbolDB.Clear("zz");
		Host_NotifyMapLoaded();
		break;
	case IDM_SCANFUNCTIONS:
		{
		PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
		SignatureDB db;
		if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
		{
			db.Apply(&g_symbolDB);
			Parent->StatusBarMessage("Generated symbol names from '%s'", TOTALDB);			
		}
		else
		{
			Parent->StatusBarMessage("'%s' not found, no symbol names generated", TOTALDB);
		}
		// HLE::PatchFunctions();
		// Update GUI
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
			Parent->StatusBarMessage("'%s' not found, scanning for common functions instead", mapfile.c_str());
		}
		else
		{
			g_symbolDB.LoadMap(mapfile.c_str());
			Parent->StatusBarMessage("Loaded symbols from '%s'", mapfile.c_str());
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
            if (! path.IsEmpty()) 
            {
                FILE *f = fopen(path.mb_str(), "r");
                if (!f)
                    return;

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
			wxTextEntryDialog input_prefix(
				this,
				wxString::FromAscii("Only export symbols with prefix:\n(Blank for all symbols)"),
				wxGetTextFromUserPromptStr,
				wxEmptyString);

			if (input_prefix.ShowModal() == wxID_OK)
			{
				std::string prefix(input_prefix.GetValue().mb_str());

				wxString path = wxFileSelector(
					_T("Save signature as"), wxEmptyString, wxEmptyString, wxEmptyString,
					_T("Dolphin Signature File (*.dsy)|*.dsy;"), wxFD_SAVE,
					this);
				if (!path.IsEmpty())
				{
					SignatureDB db;
					db.Initialize(&g_symbolDB, prefix.c_str());
					std::string filename(path.mb_str());
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
			if (!path.IsEmpty())
			{
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
	if (!codeview) return;

	g_symbolDB.FillInCallers();
	//symbols->Show(false); // hide it for faster filling
	symbols->Freeze();	// HyperIris: wx style fast filling
	symbols->Clear();
	for (PPCSymbolDB::XFuncMap::iterator iter = g_symbolDB.GetIterator(); iter != g_symbolDB.End(); ++iter)
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
				if(m_MemoryWindow)// && m_MemoryWindow->IsVisible())
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




// Toogle windows
// ----------------

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
	event.Skip();
	Parent->DoToggleWindow(event.GetId(), GetMenuBar()->IsChecked(event.GetId()));
}
void CCodeWindow::OnToggleCodeWindow(bool _Show, int i)
{
	if (_Show)
	{
		Parent->DoAddPage(this, i, wxT("Code"), bFloatCodeWindow);
	}
	else // hide
		Parent->DoRemovePage (this);
}
void CCodeWindow::OnToggleRegisterWindow(bool _Show, int i)
{
	if (_Show)
	{
		if (!m_RegisterWindow) m_RegisterWindow = new CRegisterWindow(Parent, IDM_REGISTERWINDOW);
		Parent->DoAddPage(m_RegisterWindow, i, wxT("Registers"), bFloatRegisterWindow);
	}
	else // hide
		Parent->DoRemovePage (m_RegisterWindow);
}

void CCodeWindow::OnToggleBreakPointWindow(bool _Show, int i)
{
	if (_Show)
	{
		if (!m_BreakpointWindow) m_BreakpointWindow = new CBreakPointWindow(this, Parent, IDM_BREAKPOINTWINDOW);
		Parent->DoAddPage(m_BreakpointWindow, i, wxT("Breakpoints"), bFloatBreakpointWindow);
	}
	else // hide
		Parent->DoRemovePage(m_BreakpointWindow);
}


void CCodeWindow::OnToggleMemoryWindow(bool _Show, int i)
{
	if (_Show)
	{
		if (!m_MemoryWindow) m_MemoryWindow = new CMemoryWindow(Parent, IDM_MEMORYWINDOW);
		Parent->DoAddPage(m_MemoryWindow, i, wxT("Memory"), bFloatMemoryWindow);
	}
	else // hide
		Parent->DoRemovePage(m_MemoryWindow);
}


void CCodeWindow::OnToggleJitWindow(bool _Show, int i)
{
	if (_Show)
	{
		if (!m_JitWindow) m_JitWindow = new CJitWindow(Parent, IDM_JITWINDOW);
		Parent->DoAddPage(m_JitWindow, i, wxT("JIT"), bFloatJitWindow);
	}
	else // hide
		Parent->DoRemovePage(m_JitWindow);
}


/*



Notice: This windows docking for plugin windows will produce several wx debugging messages when
::GetWindowRect and ::DestroyWindow fails in wxApp::CleanUp() for the plugin.



*/


// Toggle Sound Debugging Window
void CCodeWindow::OnToggleDLLWindow(int Id, bool _Show, int i)
{
	std::string DLLName;
	wxString Title;
	int PLUGINTYPE;

	switch(Id)
	{
		case IDM_SOUNDWINDOW:
			DLLName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str();
			PLUGINTYPE = PLUGIN_TYPE_DSP;
			Title = wxT("Sound");
			break;
		case IDM_VIDEOWINDOW:
			DLLName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str();
			PLUGINTYPE = PLUGIN_TYPE_VIDEO;
			Title = wxT("Video");
			break;
		default:
			PanicAlert("CCodeWindow::OnToggleDLLWindow");
			return;
	}

	if (_Show)
	{
		if (Parent->GetNotebookCount() == 0) return;
		if (i < 0 || i > Parent->GetNotebookCount()-1) i = 0;
		wxWindow *Win = Parent->GetWxWindow(Title);
		if (Win && Parent->GetNotebookFromId(i)->GetPageIndex(Win) != wxNOT_FOUND) return;

		// Show window
		CPluginManager::GetInstance().OpenDebug(Parent->GetHandle(), DLLName.c_str(), (PLUGIN_TYPE)PLUGINTYPE, _Show);
			
		Win = Parent->GetWxWindow(Title);
		if (Win)
		{		
			Win->SetName(Title);
			Win->Reparent(Parent);
			Win->SetId(IDM_SOUNDWINDOW);
			Parent->GetNotebookFromId(i)->AddPage(Win, Title, true, Parent->aNormalFile);
		}
		else
		{
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("OpenDebug: Win not found\n").c_str());
		}		
	}
	else
	{	
		wxWindow *Win = Parent->GetWxWindow(Title);
		if (Win)
		{			
			Parent->DoRemovePage(Win, false);
			//Win->Reparent(NULL);
			// Destroy
			CPluginManager::GetInstance().OpenDebug(Parent->GetHandle(), DLLName.c_str(), (PLUGIN_TYPE)PLUGINTYPE, _Show);
			//WARN_LOG(CONSOLE, "Sound removed from NB");
		}
		else
		{
			//WARN_LOG(CONSOLE, "Sound not found (Win %i)", FindWindowByName(wxT("Sound")));
		}
	}
	
}
