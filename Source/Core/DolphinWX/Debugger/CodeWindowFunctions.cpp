// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <istream>
#include <string>
#include <utility>
#include <vector>
#include <wx/filedlg.h>
#include <wx/font.h>
#include <wx/fontdata.h>
#include <wx/fontdlg.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/mimetype.h>
#include <wx/textdlg.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/SymbolDB.h"

#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/Boot/Boot.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/JitInterface.h"
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
	for (int i = 0; i <= IDM_VIDEO_WINDOW - IDM_LOG_WINDOW; i++)
		ini.GetOrCreateSection("ShowOnStart")->Get(SettingName[i], &bShowOnStart[i], false);

	// Get notebook affiliation
	std::string section = "P - " +
		((Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives[Parent->ActivePerspective].Name : "Perspective 1");

	for (int i = 0; i <= IDM_CODE_WINDOW - IDM_LOG_WINDOW; i++)
		ini.GetOrCreateSection(section)->Get(SettingName[i], &iNbAffiliation[i], 0);

	// Get floating setting
	for (int i = 0; i <= IDM_CODE_WINDOW - IDM_LOG_WINDOW; i++)
		ini.GetOrCreateSection("Float")->Get(SettingName[i], &Parent->bFloatWindow[i], false);
}

void CCodeWindow::Save()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	IniFile::Section* general = ini.GetOrCreateSection("General");
	general->Set("DebuggerFont", WxStrToStr(DebuggerFont.GetNativeFontInfoUserDesc()));
	general->Set("AutomaticStart", GetMenuBar()->IsChecked(IDM_AUTOMATIC_START));
	general->Set("BootToPause", GetMenuBar()->IsChecked(IDM_BOOT_TO_PAUSE));

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
	for (int i = IDM_LOG_WINDOW; i <= IDM_VIDEO_WINDOW; i++)
		ini.GetOrCreateSection("ShowOnStart")->Set(SettingName[i - IDM_LOG_WINDOW], GetMenuBar()->IsChecked(i));

	// Save notebook affiliations
	std::string section = "P - " + Parent->Perspectives[Parent->ActivePerspective].Name;
	for (int i = 0; i <= IDM_CODE_WINDOW - IDM_LOG_WINDOW; i++)
		ini.GetOrCreateSection(section)->Set(SettingName[i], iNbAffiliation[i]);

	// Save floating setting
	for (int i = IDM_LOG_WINDOW_PARENT; i <= IDM_CODE_WINDOW_PARENT; i++)
		ini.GetOrCreateSection("Float")->Set(SettingName[i - IDM_LOG_WINDOW_PARENT], !!FindWindowById(i));

	ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

// Symbols, JIT, Profiler
// ----------------
void CCodeWindow::CreateMenuSymbols(wxMenuBar *pMenuBar)
{
	wxMenu *pSymbolsMenu = new wxMenu;
	pSymbolsMenu->Append(IDM_CLEAR_SYMBOLS, _("&Clear symbols"),
		_("Remove names from all functions and variables."));
	pSymbolsMenu->Append(IDM_SCAN_FUNCTIONS, _("&Generate symbol map"),
		_("Recognise standard functions from sys\\totaldb.dsy, and use generic zz_ names for other functions."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_LOAD_MAP_FILE, _("&Load symbol map"),
		_("Try to load this game's function names automatically - but doesn't check .map files stored on the disc image yet."));
	pSymbolsMenu->Append(IDM_SAVEMAPFILE, _("&Save symbol map"),
		_("Save the function names for each address to a .map file in your user settings map folder, named after the title id."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_LOAD_MAP_FILE_AS, _("Load &other map file..."),
		_("Load any .map file containing the function names and addresses for this game."));
	pSymbolsMenu->Append(IDM_LOAD_BAD_MAP_FILE, _("Load &bad map file..."),
		_("Try to load a .map file that might be from a slightly different version."));
	pSymbolsMenu->Append(IDM_SAVE_MAP_FILE_AS, _("Save symbol map &as..."),
		_("Save the function names and addresses for this game as a .map file. If you want to open it in IDA pro, use the .idc script."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_SAVE_MAP_FILE_WITH_CODES, _("Save code"),
		_("Save the entire disassembled code. This may take a several seconds"
		" and may require between 50 and 100 MB of hard drive space. It will only save code"
		" that are in the first 4 MB of memory, if you are debugging a game that load .rel"
		" files with code to memory you may want to increase that to perhaps 8 MB, you can do"
		" that from SymbolDB::SaveMap().")
		);

	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_CREATE_SIGNATURE_FILE, _("&Create signature file..."),
		_("Create a .dsy file that can be used to recognise these same functions in other games."));
	pSymbolsMenu->Append(IDM_APPEND_SIGNATURE_FILE, _("Append to &existing signature file..."),
		_("Add any named functions missing from a .dsy file, so it can also recognise these additional functions in other games."));
	pSymbolsMenu->Append(IDM_COMBINE_SIGNATURE_FILES, _("Combine two signature files..."),
		_("Make a new .dsy file which can recognise more functions, by combining two existing files. The first input file has priority."));
	pSymbolsMenu->Append(IDM_USE_SIGNATURE_FILE, _("Apply signat&ure file..."),
		_("Must use Generate symbol map first! Recognise names of any standard library functions used in multiple games, by loading them from a .dsy file."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_PATCH_HLE_FUNCTIONS, _("&Patch HLE functions"));
	pSymbolsMenu->Append(IDM_RENAME_SYMBOLS, _("&Rename symbols from file..."));
	pMenuBar->Append(pSymbolsMenu, _("&Symbols"));

	wxMenu *pProfilerMenu = new wxMenu;
	pProfilerMenu->Append(IDM_PROFILE_BLOCKS, _("&Profile blocks"), wxEmptyString, wxITEM_CHECK);
	pProfilerMenu->AppendSeparator();
	pProfilerMenu->Append(IDM_WRITE_PROFILE, _("&Write to profile.txt, show"));
	pMenuBar->Append(pProfilerMenu, _("&Profiler"));
}

void CCodeWindow::OnProfilerMenu(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_PROFILE_BLOCKS:
		Core::SetState(Core::CORE_PAUSE);
		if (jit != nullptr)
			jit->ClearCache();
		Profiler::g_ProfileBlocks = GetMenuBar()->IsChecked(IDM_PROFILE_BLOCKS);
		Core::SetState(Core::CORE_RUN);
		break;
	case IDM_WRITE_PROFILE:
		if (Core::GetState() == Core::CORE_RUN)
			Core::SetState(Core::CORE_PAUSE);

		if (Core::GetState() == Core::CORE_PAUSE && PowerPC::GetMode() == PowerPC::MODE_JIT)
		{
			if (jit != nullptr)
			{
				std::string filename = File::GetUserPath(D_DUMP_IDX) + "Debug/profiler.txt";
				File::CreateFullPath(filename);

				ProfileStats prof_stats;
				JitInterface::GetProfileResults(&prof_stats);
				JitInterface::WriteProfileResults(prof_stats, filename);

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

	std::string existing_map_file, writable_map_file, title_id_str;
	bool map_exists = CBoot::FindMapFile(&existing_map_file,
	                                     &writable_map_file,
	                                     &title_id_str);
	switch (event.GetId())
	{
	case IDM_CLEAR_SYMBOLS:
		if (!AskYesNoT("Do you want to clear the list of symbol names?")) return;
		g_symbolDB.Clear();
		Host_NotifyMapLoaded();
		break;
	case IDM_SCAN_FUNCTIONS:
		{
		PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
		SignatureDB db;
		if (db.Load(File::GetSysDirectory() + TOTALDB))
		{
			db.Apply(&g_symbolDB);
			Parent->StatusBarMessage("Generated symbol names from '%s'", TOTALDB);
			db.List();
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
	case IDM_LOAD_MAP_FILE:
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
	case IDM_LOAD_MAP_FILE_AS:
		{
			const wxString path = wxFileSelector(
				_("Load map file"), File::GetUserPath(D_MAPS_IDX),
				title_id_str + ".map", ".map",
				"Dolphin Map File (*.map)|*.map|All files (*.*)|*.*",
				wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);

			if (!path.IsEmpty())
			{
				g_symbolDB.LoadMap(WxStrToStr(path));
				Parent->StatusBarMessage("Loaded symbols from '%s'", WxStrToStr(path).c_str());
			}
			HLE::PatchFunctions();
			NotifyMapLoaded();
		}
		break;
	case IDM_LOAD_BAD_MAP_FILE:
		{
			const wxString path = wxFileSelector(
				_("Load bad map file"), File::GetUserPath(D_MAPS_IDX),
				title_id_str + ".map", ".map",
				"Dolphin Map File (*.map)|*.map|All files (*.*)|*.*",
				wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);

			if (!path.IsEmpty())
			{
				g_symbolDB.LoadMap(WxStrToStr(path), true);
				Parent->StatusBarMessage("Loaded symbols from '%s'", WxStrToStr(path).c_str());
			}
			HLE::PatchFunctions();
			NotifyMapLoaded();
		}
		break;
	case IDM_SAVEMAPFILE:
		g_symbolDB.SaveMap(writable_map_file);
		break;
	case IDM_SAVE_MAP_FILE_AS:
		{
			const wxString path = wxFileSelector(
				_("Save map file as"), File::GetUserPath(D_MAPS_IDX),
				title_id_str + ".map", ".map",
				"Dolphin Map File (*.map)|*.map|All files (*.*)|*.*",
				wxFD_SAVE | wxFD_OVERWRITE_PROMPT, this);

			if (!path.IsEmpty())
				g_symbolDB.SaveMap(WxStrToStr(path));
		}
		break;
	case IDM_SAVE_MAP_FILE_WITH_CODES:
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

	case IDM_CREATE_SIGNATURE_FILE:
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
					_("Save signature as"), File::GetSysDirectory(), wxEmptyString, wxEmptyString,
					"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_SAVE | wxFD_OVERWRITE_PROMPT,
					this);
				if (!path.IsEmpty())
				{
					SignatureDB db;
					db.Initialize(&g_symbolDB, prefix);
					db.Save(WxStrToStr(path));
					db.List();
				}
			}
		}
		break;
	case IDM_APPEND_SIGNATURE_FILE:
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
					_("Append signature to"), File::GetSysDirectory(), wxEmptyString, wxEmptyString,
					"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_SAVE,
					this);
				if (!path.IsEmpty())
				{
					SignatureDB db;
					db.Initialize(&g_symbolDB, prefix);
					db.List();
					db.Load(WxStrToStr(path));
					db.Save(WxStrToStr(path));
					db.List();
				}
			}
		}
		break;
	case IDM_USE_SIGNATURE_FILE:
		{
			wxString path = wxFileSelector(
				_("Apply signature file"), File::GetSysDirectory(), wxEmptyString, wxEmptyString,
				"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_OPEN | wxFD_FILE_MUST_EXIST,
				this);
			if (!path.IsEmpty())
			{
				SignatureDB db;
				db.Load(WxStrToStr(path));
				db.Apply(&g_symbolDB);
				db.List();
				NotifyMapLoaded();
			}
		}
		break;
	case IDM_COMBINE_SIGNATURE_FILES:
		{
			wxString path1 = wxFileSelector(
				_("Choose priority input file"), File::GetSysDirectory(), wxEmptyString, wxEmptyString,
				"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_OPEN | wxFD_FILE_MUST_EXIST,
				this);
			if (!path1.IsEmpty())
			{
				SignatureDB db;
				wxString path2 = wxFileSelector(
					_("Choose secondary input file"), File::GetSysDirectory(), wxEmptyString, wxEmptyString,
					"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_OPEN | wxFD_FILE_MUST_EXIST,
					this);
				if (!path2.IsEmpty())
				{
					db.Load(WxStrToStr(path2));
					db.Load(WxStrToStr(path1));

					path2 = wxFileSelector(
						_("Save combined output file as"), File::GetSysDirectory(), wxEmptyString, ".dsy",
						"Dolphin Signature File (*.dsy)|*.dsy;", wxFD_SAVE | wxFD_OVERWRITE_PROMPT,
						this);
					db.Save(WxStrToStr(path2));
					db.List();
				}
			}
		}
		break;
	case IDM_PATCH_HLE_FUNCTIONS:
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
	if (bShowOnStart[IDM_LOG_CONFIG_WINDOW - IDM_LOG_WINDOW])
		Parent->ToggleLogConfigWindow(true);
	if (bShowOnStart[IDM_REGISTER_WINDOW - IDM_LOG_WINDOW])
		ToggleRegisterWindow(true);
	if (bShowOnStart[IDM_WATCH_WINDOW - IDM_LOG_WINDOW])
		ToggleWatchWindow(true);
	if (bShowOnStart[IDM_BREAKPOINT_WINDOW - IDM_LOG_WINDOW])
		ToggleBreakPointWindow(true);
	if (bShowOnStart[IDM_MEMORY_WINDOW - IDM_LOG_WINDOW])
		ToggleMemoryWindow(true);
	if (bShowOnStart[IDM_JIT_WINDOW - IDM_LOG_WINDOW])
		ToggleJitWindow(true);
	if (bShowOnStart[IDM_SOUND_WINDOW - IDM_LOG_WINDOW])
		ToggleSoundWindow(true);
	if (bShowOnStart[IDM_VIDEO_WINDOW - IDM_LOG_WINDOW])
		ToggleVideoWindow(true);
}

void CCodeWindow::ToggleCodeWindow(bool bShow)
{
	if (bShow)
		Parent->DoAddPage(this,
		        iNbAffiliation[IDM_CODE_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_CODE_WINDOW - IDM_LOG_WINDOW]);
	else // Hide
		Parent->DoRemovePage(this);
}

void CCodeWindow::ToggleRegisterWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_REGISTER_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_RegisterWindow)
			m_RegisterWindow = new CRegisterWindow(Parent, IDM_REGISTER_WINDOW);
		Parent->DoAddPage(m_RegisterWindow,
		        iNbAffiliation[IDM_REGISTER_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_REGISTER_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_RegisterWindow, false);
		m_RegisterWindow = nullptr;
	}
}

void CCodeWindow::ToggleWatchWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_WATCH_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_WatchWindow)
			m_WatchWindow = new CWatchWindow(Parent, IDM_WATCH_WINDOW);
		Parent->DoAddPage(m_WatchWindow,
		        iNbAffiliation[IDM_WATCH_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_WATCH_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_WatchWindow, false);
		m_WatchWindow = nullptr;
	}
}

void CCodeWindow::ToggleBreakPointWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_BREAKPOINT_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_BreakpointWindow)
			m_BreakpointWindow = new CBreakPointWindow(this, Parent, IDM_BREAKPOINT_WINDOW);
		Parent->DoAddPage(m_BreakpointWindow,
		        iNbAffiliation[IDM_BREAKPOINT_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_BREAKPOINT_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_BreakpointWindow, false);
		m_BreakpointWindow = nullptr;
	}
}

void CCodeWindow::ToggleMemoryWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_MEMORY_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_MemoryWindow)
			m_MemoryWindow = new CMemoryWindow(Parent, IDM_MEMORY_WINDOW);
		Parent->DoAddPage(m_MemoryWindow,
		        iNbAffiliation[IDM_MEMORY_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_MEMORY_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_MemoryWindow, false);
		m_MemoryWindow = nullptr;
	}
}

void CCodeWindow::ToggleJitWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_JIT_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_JitWindow)
			m_JitWindow = new CJitWindow(Parent, IDM_JIT_WINDOW);
		Parent->DoAddPage(m_JitWindow,
		        iNbAffiliation[IDM_JIT_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_JIT_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_JitWindow, false);
		m_JitWindow = nullptr;
	}
}


void CCodeWindow::ToggleSoundWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_SOUND_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_SoundWindow)
			m_SoundWindow = new DSPDebuggerLLE(Parent, IDM_SOUND_WINDOW);
		Parent->DoAddPage(m_SoundWindow,
		       iNbAffiliation[IDM_SOUND_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_SOUND_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_SoundWindow, false);
		m_SoundWindow = nullptr;
	}
}

void CCodeWindow::ToggleVideoWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_VIDEO_WINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_VideoWindow)
			m_VideoWindow = new GFXDebuggerPanel(Parent, IDM_VIDEO_WINDOW);
		Parent->DoAddPage(m_VideoWindow,
		        iNbAffiliation[IDM_VIDEO_WINDOW - IDM_LOG_WINDOW],
		        Parent->bFloatWindow[IDM_VIDEO_WINDOW - IDM_LOG_WINDOW]);
	}
	else // Close
	{
		Parent->DoRemovePage(m_VideoWindow, false);
		m_VideoWindow = nullptr;
	}
}
