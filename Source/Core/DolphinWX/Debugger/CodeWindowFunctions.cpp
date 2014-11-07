// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <istream>
#include <string>
#include <utility>
#include <vector>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/font.h>
#include <wx/fontdata.h>
#include <wx/fontdlg.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/mimetype.h>
#include <wx/string.h>
#include <wx/textdlg.h>
#include <wx/translation.h>
#include <wx/utils.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/SymbolDB.h"

#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/Boot/Boot.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/SignatureDB.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerPanel.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/DSPDebugWindow.h"
#include "DolphinWX/Debugger/JitWindow.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Debugger/RegisterWindow.h"
#include "DolphinWX/Debugger/WatchWindow.h"


// Save and load settings
// -----------------------------
void CCodeWindow::Load()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	// The font to override DebuggerFont with
	std::string fontDesc;

	IniFile::Section* general = ini.GetOrCreateSection("General");
	general->Get("DebuggerFont", &fontDesc);
	general->Get("AutomaticStart", &bAutomaticStart, false);
	general->Get("BootToPause", &bBootToPause, true);

	if (!fontDesc.empty())
		DebuggerFont.SetNativeFontInfoUserDesc(StrToWxStr(fontDesc));

	const char* SettingName[] = {
		"Log",
		"LogConfig",
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
		ini.GetOrCreateSection("ShowOnStart")->Get(SettingName[i], &bShowOnStart[i], false);

	// Get notebook affiliation
	std::string section = "P - " +
		((Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives[Parent->ActivePerspective].Name : "Perspective 1");

	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		ini.GetOrCreateSection(section)->Get(SettingName[i], &iNbAffiliation[i], 0);

	// Get floating setting
	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		ini.GetOrCreateSection("Float")->Get(SettingName[i], &Parent->bFloatWindow[i], false);
}

void CCodeWindow::Save()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	IniFile::Section* general = ini.GetOrCreateSection("General");
	general->Set("DebuggerFont", WxStrToStr(DebuggerFont.GetNativeFontInfoUserDesc()));
	general->Set("AutomaticStart", GetMenuBar()->IsChecked(IDM_AUTOMATICSTART));
	general->Set("BootToPause", GetMenuBar()->IsChecked(IDM_BOOTTOPAUSE));

	const char* SettingName[] = {
		"Log",
		"LogConfig",
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
		ini.GetOrCreateSection("ShowOnStart")->Set(SettingName[i - IDM_LOGWINDOW], GetMenuBar()->IsChecked(i));

	// Save notebook affiliations
	std::string section = "P - " + Parent->Perspectives[Parent->ActivePerspective].Name;
	for (int i = 0; i <= IDM_CODEWINDOW - IDM_LOGWINDOW; i++)
		ini.GetOrCreateSection(section)->Set(SettingName[i], iNbAffiliation[i]);

	// Save floating setting
	for (int i = IDM_LOGWINDOW_PARENT; i <= IDM_CODEWINDOW_PARENT; i++)
		ini.GetOrCreateSection("Float")->Set(SettingName[i - IDM_LOGWINDOW_PARENT], !!FindWindowById(i));

	ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

// Symbols, JIT, Profiler
// ----------------
void CCodeWindow::CreateMenuSymbols(wxMenuBar *pMenuBar)
{
	wxMenu *pSymbolsMenu = new wxMenu;
	pSymbolsMenu->Append(IDM_CLEARSYMBOLS, _("&Clear symbols"));
	pSymbolsMenu->Append(IDM_SCANFUNCTIONS, _("&Generate symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_LOADMAPFILE, _("&Load symbol map"));
	pSymbolsMenu->Append(IDM_SAVEMAPFILE, _("&Save symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_SAVEMAPFILEWITHCODES, _("Save code"),
		_("Save the entire disassembled code. This may take a several seconds"
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
	switch (event.GetId())
	{
	case IDM_PROFILEBLOCKS:
		Core::SetState(Core::CORE_PAUSE);
		if (jit != nullptr)
			jit->ClearCache();
		Profiler::g_ProfileBlocks = GetMenuBar()->IsChecked(IDM_PROFILEBLOCKS);
		Core::SetState(Core::CORE_RUN);
		break;
	case IDM_WRITEPROFILE:
		if (Core::GetState() == Core::CORE_RUN)
			Core::SetState(Core::CORE_PAUSE);

		if (Core::GetState() == Core::CORE_PAUSE && PowerPC::GetMode() == PowerPC::MODE_JIT)
		{
			if (jit != nullptr)
			{
				std::string filename = File::GetUserPath(D_DUMP_IDX) + "Debug/profiler.txt";
				File::CreateFullPath(filename);
				Profiler::WriteProfileResults(filename);

				wxFileType* filetype = nullptr;
				if (!(filetype = wxTheMimeTypesManager->GetFileTypeFromExtension("txt")))
				{
					// From extension failed, trying with MIME type now
					if (!(filetype = wxTheMimeTypesManager->GetFileTypeFromMimeType("text/plain")))
						// MIME type failed, aborting mission
						break;
				}
				wxString OpenCommand;
				OpenCommand = filetype->GetOpenCommand(StrToWxStr(filename));
				if (!OpenCommand.IsEmpty())
					wxExecute(OpenCommand, wxEXEC_SYNC);
			}
		}
		break;
	}
}

void CCodeWindow::OnSymbolsMenu(wxCommandEvent& event)
{
	Parent->ClearStatusBar();

	if (!Core::IsRunning()) return;

	std::string existing_map_file, writable_map_file;
	bool map_exists = CBoot::FindMapFile(&existing_map_file,
	                                     &writable_map_file);
	switch (event.GetId())
	{
	case IDM_CLEARSYMBOLS:
		if (!AskYesNoT("Do you want to clear the list of symbol names?")) return;
		g_symbolDB.Clear();
		Host_NotifyMapLoaded();
		break;
	case IDM_SCANFUNCTIONS:
		{
		PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
		SignatureDB db;
		if (db.Load(File::GetSysDirectory() + TOTALDB))
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
		if (!map_exists)
		{
			g_symbolDB.Clear();
			PPCAnalyst::FindFunctions(0x81300000, 0x81800000, &g_symbolDB);
			SignatureDB db;
			if (db.Load(File::GetSysDirectory() + TOTALDB))
				db.Apply(&g_symbolDB);
			Parent->StatusBarMessage("'%s' not found, scanning for common functions instead", writable_map_file.c_str());
		}
		else
		{
			g_symbolDB.LoadMap(existing_map_file);
			Parent->StatusBarMessage("Loaded symbols from '%s'", existing_map_file.c_str());
		}
		HLE::PatchFunctions();
		NotifyMapLoaded();
		break;
	case IDM_SAVEMAPFILE:
		g_symbolDB.SaveMap(writable_map_file);
		break;
	case IDM_SAVEMAPFILEWITHCODES:
		g_symbolDB.SaveMap(writable_map_file, true);
		break;

	case IDM_RENAME_SYMBOLS:
		{
			const wxString path = wxFileSelector(
				_("Apply signature file"), wxEmptyString,
				wxEmptyString, wxEmptyString,
				"Dolphin Symbol Rename File (*.sym)|*.sym",
				wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);

			if (!path.IsEmpty())
			{
				std::ifstream f;
				OpenFStream(f, WxStrToStr(path), std::ios_base::in);

				std::string line;
				while (std::getline(f, line))
				{
					if (line.length() < 12)
						continue;

					u32 address, type;
					std::string name;

					std::istringstream ss(line);
					ss >> std::hex >> address >> std::dec >> type >> name;

					Symbol *symbol = g_symbolDB.GetSymbolFromAddr(address);
					if (symbol)
						symbol->name = line.substr(12);
				}

				Host_NotifyMapLoaded();
			}
		}
		break;

	case IDM_CREATESIGNATUREFILE:
		{
			wxTextEntryDialog input_prefix(
				this,
				_("Only export symbols with prefix:\n(Blank for all symbols)"),
				wxGetTextFromUserPromptStr,
				wxEmptyString);

			if (input_prefix.ShowModal() == wxID_OK)
			{
				std::string prefix(WxStrToStr(input_prefix.GetValue()));

				wxString path = wxFileSelector(
					_("Save signature as"), wxEmptyString, wxEmptyString, wxEmptyString,
					"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_SAVE,
					this);
				if (!path.IsEmpty())
				{
					SignatureDB db;
					db.Initialize(&g_symbolDB, prefix);
					db.Save(WxStrToStr(path));
				}
			}
		}
		break;
	case IDM_USESIGNATUREFILE:
		{
			wxString path = wxFileSelector(
				_("Apply signature file"), wxEmptyString, wxEmptyString, wxEmptyString,
				"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_OPEN | wxFD_FILE_MUST_EXIST,
				this);
			if (!path.IsEmpty())
			{
				SignatureDB db;
				db.Load(WxStrToStr(path));
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
	symbols->Freeze(); // HyperIris: wx style fast filling
	symbols->Clear();
	for (const auto& symbol : g_symbolDB.Symbols())
	{
		int idx = symbols->Append(StrToWxStr(symbol.second.name));
		symbols->SetClientData(idx, (void*)&symbol.second);
	}
	symbols->Thaw();
	//symbols->Show(true);
	Update();
}

void CCodeWindow::OnSymbolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	if (index >= 0)
	{
		Symbol* pSymbol = static_cast<Symbol *>(symbols->GetClientData(index));
		if (pSymbol != nullptr)
		{
			if (pSymbol->type == Symbol::SYMBOL_DATA)
			{
				if (m_MemoryWindow)// && m_MemoryWindow->IsVisible())
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

// Toggle windows

void CCodeWindow::OpenPages()
{
	ToggleCodeWindow(true);
	if (bShowOnStart[0])
		Parent->ToggleLogWindow(true);
	if (bShowOnStart[IDM_LOGCONFIGWINDOW - IDM_LOGWINDOW])
		Parent->ToggleLogConfigWindow(true);
	if (bShowOnStart[IDM_REGISTERWINDOW - IDM_LOGWINDOW])
		ToggleRegisterWindow(true);
	if (bShowOnStart[IDM_WATCHWINDOW - IDM_LOGWINDOW])
		ToggleWatchWindow(true);
	if (bShowOnStart[IDM_BREAKPOINTWINDOW - IDM_LOGWINDOW])
		ToggleBreakPointWindow(true);
	if (bShowOnStart[IDM_MEMORYWINDOW - IDM_LOGWINDOW])
		ToggleMemoryWindow(true);
	if (bShowOnStart[IDM_JITWINDOW - IDM_LOGWINDOW])
		ToggleJitWindow(true);
	if (bShowOnStart[IDM_SOUNDWINDOW - IDM_LOGWINDOW])
		ToggleSoundWindow(true);
	if (bShowOnStart[IDM_VIDEOWINDOW - IDM_LOGWINDOW])
		ToggleVideoWindow(true);
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
		m_RegisterWindow = nullptr;
	}
}

void CCodeWindow::ToggleWatchWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_WATCHWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_WatchWindow)
			m_WatchWindow = new CWatchWindow(Parent, IDM_WATCHWINDOW);
		Parent->DoAddPage(m_WatchWindow,
		        iNbAffiliation[IDM_WATCHWINDOW - IDM_LOGWINDOW],
		        Parent->bFloatWindow[IDM_WATCHWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_WatchWindow, false);
		m_WatchWindow = nullptr;
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
		m_BreakpointWindow = nullptr;
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
		m_MemoryWindow = nullptr;
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
		m_JitWindow = nullptr;
	}
}


void CCodeWindow::ToggleSoundWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_SOUNDWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_SoundWindow)
			m_SoundWindow = new DSPDebuggerLLE(Parent, IDM_SOUNDWINDOW);
		Parent->DoAddPage(m_SoundWindow,
		       iNbAffiliation[IDM_SOUNDWINDOW - IDM_LOGWINDOW],
		        Parent->bFloatWindow[IDM_SOUNDWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_SoundWindow, false);
		m_SoundWindow = nullptr;
	}
}

void CCodeWindow::ToggleVideoWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_VIDEOWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_VideoWindow)
			m_VideoWindow = new GFXDebuggerPanel(Parent, IDM_VIDEOWINDOW);
		Parent->DoAddPage(m_VideoWindow,
		        iNbAffiliation[IDM_VIDEOWINDOW - IDM_LOGWINDOW],
		        Parent->bFloatWindow[IDM_VIDEOWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_VideoWindow, false);
		m_VideoWindow = nullptr;
	}
}
