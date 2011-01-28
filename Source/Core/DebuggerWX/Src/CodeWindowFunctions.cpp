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

#include "Common.h"
#include "CommonPaths.h"

#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <wx/mstream.h>
#include <wx/tipwin.h>
#include <wx/fontdlg.h>

#include "../../DolphinWX/Src/WxUtils.h"

#include "Host.h"

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
	ini.Get("General", "DebuggerFont", &fontDesc);
	if (!fontDesc.empty())
		DebuggerFont.SetNativeFontInfoUserDesc(wxString::FromAscii(fontDesc.c_str()));

	// Boot to pause or not
	ini.Get("General", "AutomaticStart", &bAutomaticStart, false);
	ini.Get("General", "BootToPause", &bBootToPause, true);

	const char* SettingName[] = {
		"Log",
		"Console",
		"Registers",
		"Breakpoints",
		"Memory",
		"JIT",
		"Sound",
		"Video",
		"Code"
	};

	// Decide what windows to show
	for (int i = 0; i <= IDM_VIDEOWINDOW - IDM_LOGWINDOW; i++)
		ini.Get("ShowOnStart", SettingName[i], &bShowOnStart[i], false);

	// Get notebook affiliation
	std::string _Section = "P - " +
		((Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives[Parent->ActivePerspective].Name : "Perspective 1");

	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		ini.Get(_Section.c_str(), SettingName[i], &iNbAffiliation[i], 0);

	// Get floating setting
	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		ini.Get("Float", SettingName[i], &Parent->bFloatWindow[i], false);
}

void CCodeWindow::Save()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	ini.Set("General", "DebuggerFont",
		   	std::string(DebuggerFont.GetNativeFontInfoUserDesc().mb_str()));

	// Boot to pause or not
	ini.Set("General", "AutomaticStart", GetMenuBar()->IsChecked(IDM_AUTOMATICSTART));
	ini.Set("General", "BootToPause", GetMenuBar()->IsChecked(IDM_BOOTTOPAUSE));

	const char* SettingName[] = {
		"Log",
		"Console",
		"Registers",
		"Breakpoints",
		"Memory",
		"JIT", 
		"Sound",
		"Video",
		"Code"
	};

	// Save windows settings
	for (int i = IDM_LOGWINDOW; i <= IDM_VIDEOWINDOW; i++)
		ini.Set("ShowOnStart", SettingName[i - IDM_LOGWINDOW], GetMenuBar()->IsChecked(i));

	// Save notebook affiliations
	std::string _Section = "P - " + Parent->Perspectives[Parent->ActivePerspective].Name;
	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		ini.Set(_Section.c_str(), SettingName[i], iNbAffiliation[i]);

	// Save floating setting
	for (int i = IDM_LOGWINDOW_PARENT; i <= IDM_CODEWINDOW_PARENT; i++)
		ini.Set("Float", SettingName[i - IDM_LOGWINDOW_PARENT], !!FindWindowById(i));

	ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

// Symbols, JIT, Profiler
// ----------------
void CCodeWindow::CreateMenuSymbols(wxMenuBar *pMenuBar)
{
	wxMenu *pSymbolsMenu = new wxMenu;
	pSymbolsMenu->Append(IDM_CLEARSYMBOLS, _("&Clear symbols"));
	// pSymbolsMenu->Append(IDM_CLEANSYMBOLS, _("&Clean symbols (zz)"));
	pSymbolsMenu->Append(IDM_SCANFUNCTIONS, _("&Generate symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_LOADMAPFILE, _("&Load symbol map"));
	pSymbolsMenu->Append(IDM_SAVEMAPFILE, _("&Save symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_SAVEMAPFILEWITHCODES, _("Save code"),
		wxString::FromAscii("Save the entire disassembled code. This may take a several seconds"
		" and may require between 50 and 100 MB of hard drive space. It will only save code"
		" that are in the first 4 MB of memory, if you are debugging a game that load .rel"
		" files with code to memory you may want to increase that to perhaps 8 MB, you can do"
		" that from SymbolDB::SaveMap().")
		);

	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_CREATESIGNATUREFILE, _("&Create signature file..."));
	pSymbolsMenu->Append(IDM_USESIGNATUREFILE, _("&Use signature file..."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_PATCHHLEFUNCTIONS, _("&Patch HLE functions"));
	pSymbolsMenu->Append(IDM_RENAME_SYMBOLS, _("&Rename symbols from file..."));
	pMenuBar->Append(pSymbolsMenu, _("&Symbols"));

	wxMenu *pProfilerMenu = new wxMenu;
	pProfilerMenu->Append(IDM_PROFILEBLOCKS, _("&Profile blocks"), wxEmptyString, wxITEM_CHECK);
	pProfilerMenu->AppendSeparator();
	pProfilerMenu->Append(IDM_WRITEPROFILE, _("&Write to profile.txt, show"));
	pMenuBar->Append(pProfilerMenu, _("&Profiler"));
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
		if (jit != NULL) jit->ClearCache();
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
		if(!AskYesNo("Do you want to clear the list of symbol names?")) return;
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
				_("Apply signature file"), wxEmptyString,
				wxEmptyString, wxEmptyString,
				_T("Dolphin Symbol Rename File (*.sym)|*.sym"),
				wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);
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
	data.SetInitialFont(DebuggerFont);

	wxFontDialog dialog(this, data);
	if ( dialog.ShowModal() == wxID_OK )
		DebuggerFont = dialog.GetFontData().GetChosenFont();
}

// Toogle windows

void CCodeWindow::OpenPages()
{
	ToggleCodeWindow(true);
	if (bShowOnStart[0])
		Parent->ToggleLogWindow(true);
	if (bShowOnStart[IDM_CONSOLEWINDOW - IDM_LOGWINDOW])
		Parent->ToggleConsole(true);
	if (bShowOnStart[IDM_REGISTERWINDOW - IDM_LOGWINDOW])
		ToggleRegisterWindow(true);
	if (bShowOnStart[IDM_BREAKPOINTWINDOW - IDM_LOGWINDOW])
		ToggleBreakPointWindow(true);
	if (bShowOnStart[IDM_MEMORYWINDOW - IDM_LOGWINDOW])
		ToggleMemoryWindow(true);
	if (bShowOnStart[IDM_JITWINDOW - IDM_LOGWINDOW])
		ToggleJitWindow(true);
	if (bShowOnStart[IDM_SOUNDWINDOW - IDM_LOGWINDOW])
		ToggleDLLWindow(IDM_SOUNDWINDOW, true);
	if (bShowOnStart[IDM_VIDEOWINDOW - IDM_LOGWINDOW])
		ToggleDLLWindow(IDM_VIDEOWINDOW, true);
}

void CCodeWindow::ToggleCodeWindow(bool bShow)
{
	if (bShow)
		Parent->DoAddPage(this,
			   	iNbAffiliation[IDM_CODEWINDOW - IDM_LOGWINDOW],
			   	Parent->bFloatWindow[IDM_CODEWINDOW - IDM_LOGWINDOW]);
	else // Hide
		Parent->DoRemovePage(this);
}

void CCodeWindow::ToggleRegisterWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_REGISTERWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_RegisterWindow)
		   	m_RegisterWindow = new CRegisterWindow(Parent, IDM_REGISTERWINDOW);
		Parent->DoAddPage(m_RegisterWindow,
			   	iNbAffiliation[IDM_REGISTERWINDOW - IDM_LOGWINDOW],
			   	Parent->bFloatWindow[IDM_REGISTERWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_RegisterWindow, false);
		m_RegisterWindow = NULL;
	}
}

void CCodeWindow::ToggleBreakPointWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_BREAKPOINTWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_BreakpointWindow)
		   	m_BreakpointWindow = new CBreakPointWindow(this, Parent, IDM_BREAKPOINTWINDOW);
		Parent->DoAddPage(m_BreakpointWindow,
			   	iNbAffiliation[IDM_BREAKPOINTWINDOW - IDM_LOGWINDOW],
			   	Parent->bFloatWindow[IDM_BREAKPOINTWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_BreakpointWindow, false);
		m_BreakpointWindow = NULL;
	}
}

void CCodeWindow::ToggleMemoryWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_MEMORYWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_MemoryWindow)
		   	m_MemoryWindow = new CMemoryWindow(Parent, IDM_MEMORYWINDOW);
		Parent->DoAddPage(m_MemoryWindow,
			   	iNbAffiliation[IDM_MEMORYWINDOW - IDM_LOGWINDOW],
			   	Parent->bFloatWindow[IDM_MEMORYWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_MemoryWindow, false);
		m_MemoryWindow = NULL;
	}
}

void CCodeWindow::ToggleJitWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_JITWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_JitWindow)
		   	m_JitWindow = new CJitWindow(Parent, IDM_JITWINDOW);
		Parent->DoAddPage(m_JitWindow,
			   	iNbAffiliation[IDM_JITWINDOW - IDM_LOGWINDOW],
			   	Parent->bFloatWindow[IDM_JITWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_JitWindow, false);
		m_JitWindow = NULL;
	}
}


void CCodeWindow::ToggleSoundWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_SOUNDWINDOW)->Check(bShow);
	if (bShow)
	{
		/* TODO: Resurrect DSP debugger window.
		if (!m_JitWindow)
		   	m_JitWindow = new CJitWindow(Parent, IDM_SOUNDWINDOW);
		Parent->DoAddPage(m_JitWindow,
			   	iNbAffiliation[IDM_SOUNDWINDOW - IDM_LOGWINDOW],
			   	Parent->bFloatWindow[IDM_SOUNDWINDOW - IDM_LOGWINDOW]);
		*/
	}
	else // Close
	{
		//Parent->DoRemovePage(m_JitWindow, false);
		// m_JitWindow = NULL;
	}
}

// Notice: This windows docking will produce several wx debugging messages for plugin
// windows when ::GetWindowRect and ::DestroyWindow fails in wxApp::CleanUp() for the
// plugin.

// Toggle Sound Debugging Window
void CCodeWindow::ToggleDLLWindow(int Id, bool bShow)
{
	std::string DLLName;
	int PluginType;
	wxPanel *Win;

	switch(Id)
	{
		case IDM_VIDEOWINDOW:
			DLLName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str();
			PluginType = PLUGIN_TYPE_VIDEO;
			break;
		default:
			PanicAlert("CCodeWindow::ToggleDLLWindow called with invalid Id");
			return;
	}

	if (bShow)
	{
		// Show window
		Win = (wxPanel *)CPluginManager::GetInstance().OpenDebug(Parent,
				DLLName.c_str(), (PLUGIN_TYPE)PluginType, bShow);

		if (Win)
		{
			Win->Show();
			Win->SetId(Id);
			Parent->DoAddPage(Win,
				   	iNbAffiliation[Id - IDM_LOGWINDOW],
				   	Parent->bFloatWindow[Id - IDM_LOGWINDOW]);
		}
	}
	else
	{
		Win = (wxPanel *)FindWindowById(Id);
		if (Win)
		{
			Parent->DoRemovePage(Win, false);
			Win->Destroy();
		}
	}
	GetMenuBar()->FindItem(Id)->Check(bShow && !!Win);
}
