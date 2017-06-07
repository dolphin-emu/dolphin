// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <wx/artprov.h>
#include <wx/aui/auibook.h>
#include <wx/aui/dockart.h>
#include <wx/aui/framemanager.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/thread.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/DSP/DSPCore.h"
#include "Core/HW/DSPLLE/DSPDebugInterface.h"
#include "Core/HW/DSPLLE/DSPSymbols.h"
#include "Core/Host.h"
#include "DolphinWX/AuiToolBar.h"
#include "DolphinWX/Debugger/CodeView.h"
#include "DolphinWX/Debugger/DSPDebugWindow.h"
#include "DolphinWX/Debugger/DSPRegisterView.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/MemoryView.h"
#include "DolphinWX/WxUtils.h"

static DSPDebuggerLLE* m_DebuggerFrame = nullptr;

DSPDebuggerLLE::DSPDebuggerLLE(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("DSP LLE Debugger")),
      m_CachedStepCounter(UINT64_MAX), m_toolbar_item_size(FromDIP(wxSize(16, 16)))
{
  Bind(wxEVT_MENU, &DSPDebuggerLLE::OnChangeState, this, ID_RUNTOOL, ID_SHOWPCTOOL);

  m_DebuggerFrame = this;

  // notify wxAUI which frame to use
  m_mgr.SetManagedWindow(this);
  m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

  m_Toolbar =
      new DolphinAuiToolBar(this, ID_TOOLBAR, wxDefaultPosition, wxDefaultSize, wxAUI_TB_HORZ_TEXT);
  m_Toolbar->AddTool(ID_RUNTOOL, _("Pause"),
                     wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_OTHER, m_toolbar_item_size));
  // i18n: Here, "Step" is a verb. This function is used for
  // going through code step by step.
  m_Toolbar->AddTool(ID_STEPTOOL, _("Step"),
                     wxArtProvider::GetBitmap(wxART_GO_DOWN, wxART_OTHER, m_toolbar_item_size));
  m_Toolbar->AddTool(
      // i18n: Here, PC is an acronym for program counter, not personal computer.
      ID_SHOWPCTOOL, _("Show PC"),
      wxArtProvider::GetBitmap(wxART_GO_TO_PARENT, wxART_OTHER, m_toolbar_item_size));
  m_Toolbar->AddSeparator();

  m_addr_txtctrl = new wxTextCtrl(m_Toolbar, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                  wxDefaultSize, wxTE_PROCESS_ENTER);
  m_addr_txtctrl->Bind(wxEVT_TEXT_ENTER, &DSPDebuggerLLE::OnAddrBoxChange, this);

  m_Toolbar->AddControl(m_addr_txtctrl);
  m_Toolbar->Realize();

  m_SymbolList = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(100, 80)),
                               0, nullptr, wxLB_SORT);
  m_SymbolList->Bind(wxEVT_LISTBOX, &DSPDebuggerLLE::OnSymbolListChange, this);

  m_MainNotebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE);

  wxPanel* code_panel = new wxPanel(m_MainNotebook, wxID_ANY);
  wxBoxSizer* code_sizer = new wxBoxSizer(wxVERTICAL);
  m_CodeView = new CCodeView(&debug_interface, &DSP::Symbols::g_dsp_symbol_db, code_panel);
  m_CodeView->SetPlain();
  code_sizer->Add(m_CodeView, 1, wxEXPAND);
  code_panel->SetSizer(code_sizer);
  m_MainNotebook->AddPage(code_panel, _("Disassembly"), true);

  wxPanel* mem_panel = new wxPanel(m_MainNotebook, wxID_ANY);
  wxBoxSizer* mem_sizer = new wxBoxSizer(wxVERTICAL);
  m_MemView = new CMemoryView(&debug_interface, mem_panel);
  mem_sizer->Add(m_MemView, 1, wxEXPAND);
  mem_panel->SetSizer(mem_sizer);
  m_MainNotebook->AddPage(mem_panel, _("Memory"));

  m_Regs = new DSPRegisterView(this);

  // add the panes to the manager
  m_mgr.AddPane(m_Toolbar,
                wxAuiPaneInfo().ToolbarPane().Top().LeftDockable(false).RightDockable(false));

  m_mgr.AddPane(m_SymbolList,
                wxAuiPaneInfo().Left().CloseButton(false).Caption(_("Symbols")).Dockable(true));

  m_mgr.AddPane(
      m_MainNotebook,
      wxAuiPaneInfo().Name("m_MainNotebook").Center().CloseButton(false).MaximizeButton(true));

  m_mgr.AddPane(m_Regs,
                wxAuiPaneInfo().Right().CloseButton(false).Caption(_("Registers")).Dockable(true));

  m_mgr.GetArtProvider()->SetFont(wxAUI_DOCKART_CAPTION_FONT, DebuggerFont);
  UpdateState();

  m_mgr.Update();
}

DSPDebuggerLLE::~DSPDebuggerLLE()
{
  m_mgr.UnInit();
  m_DebuggerFrame = nullptr;
}

void DSPDebuggerLLE::OnChangeState(wxCommandEvent& event)
{
  const DSP::State dsp_state = DSP::DSPCore_GetState();

  if (dsp_state == DSP::State::Stopped)
    return;

  switch (event.GetId())
  {
  case ID_RUNTOOL:
    if (dsp_state == DSP::State::Running)
      DSP::DSPCore_SetState(DSP::State::Stepping);
    else
      DSP::DSPCore_SetState(DSP::State::Running);
    break;

  case ID_STEPTOOL:
    if (dsp_state == DSP::State::Stepping)
    {
      DSP::DSPCore_Step();
      Repopulate();
    }
    break;

  case ID_SHOWPCTOOL:
    FocusOnPC();
    break;
  }

  UpdateState();
  m_mgr.Update();
}

void Host_RefreshDSPDebuggerWindow()
{
  // FIXME: This should use QueueEvent to post the update request to the UI thread.
  //   Need to check if this can safely be performed asynchronously or if it races.
  // FIXME: This probably belongs in Main.cpp with the other host functions.
  // NOTE: The DSP never tells us when it shuts down. It probably should.
  if (m_DebuggerFrame)
    m_DebuggerFrame->Repopulate();
}

void DSPDebuggerLLE::Repopulate()
{
  if (!wxIsMainThread())
    wxMutexGuiEnter();
  UpdateSymbolMap();
  UpdateDisAsmListView();
  UpdateRegisterFlags();
  UpdateState();
  if (!wxIsMainThread())
    wxMutexGuiLeave();
}

void DSPDebuggerLLE::FocusOnPC()
{
  JumpToAddress(DSP::g_dsp.pc);
}

void DSPDebuggerLLE::UpdateState()
{
  if (DSP::DSPCore_GetState() == DSP::State::Running)
  {
    m_Toolbar->SetToolLabel(ID_RUNTOOL, _("Pause"));
    m_Toolbar->SetToolBitmap(
        ID_RUNTOOL, wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_OTHER, m_toolbar_item_size));
    m_Toolbar->EnableTool(ID_STEPTOOL, false);
  }
  else
  {
    m_Toolbar->SetToolLabel(ID_RUNTOOL, _("Run"));
    m_Toolbar->SetToolBitmap(
        ID_RUNTOOL, wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_OTHER, m_toolbar_item_size));
    m_Toolbar->EnableTool(ID_STEPTOOL, true);
  }
  m_Toolbar->Realize();
}

void DSPDebuggerLLE::UpdateDisAsmListView()
{
  if (m_CachedStepCounter == DSP::g_dsp.step_counter)
    return;

  // show PC
  FocusOnPC();
  m_CachedStepCounter = DSP::g_dsp.step_counter;
  m_Regs->Repopulate();
}

void DSPDebuggerLLE::UpdateSymbolMap()
{
  if (DSP::g_dsp.dram == nullptr)
    return;

  m_SymbolList->Freeze();  // HyperIris: wx style fast filling
  m_SymbolList->Clear();
  for (const auto& symbol : DSP::Symbols::g_dsp_symbol_db.Symbols())
  {
    int idx = m_SymbolList->Append(StrToWxStr(symbol.second.name));
    m_SymbolList->SetClientData(idx, (void*)&symbol.second);
  }
  m_SymbolList->Thaw();
}

void DSPDebuggerLLE::OnSymbolListChange(wxCommandEvent& event)
{
  int index = m_SymbolList->GetSelection();
  if (index >= 0)
  {
    Symbol* pSymbol = static_cast<Symbol*>(m_SymbolList->GetClientData(index));
    if (pSymbol != nullptr)
    {
      if (pSymbol->type == Symbol::Type::Function)
      {
        JumpToAddress(pSymbol->address);
      }
    }
  }
}

void DSPDebuggerLLE::UpdateRegisterFlags()
{
}

void DSPDebuggerLLE::OnAddrBoxChange(wxCommandEvent& event)
{
  wxString txt = m_addr_txtctrl->GetValue();

  auto text = StripSpaces(WxStrToStr(txt));
  if (text.size())
  {
    u32 addr;
    sscanf(text.c_str(), "%04x", &addr);
    if (JumpToAddress(addr))
      m_addr_txtctrl->SetBackgroundColour(*wxWHITE);
    else
      m_addr_txtctrl->SetBackgroundColour(*wxRED);
  }
  event.Skip();
}

bool DSPDebuggerLLE::JumpToAddress(u16 addr)
{
  int page = m_MainNotebook->GetSelection();
  if (page == 0)
  {
    // Center on valid instruction in IRAM/IROM
    int new_line = DSP::Symbols::Addr2Line(addr);
    if (new_line >= 0)
    {
      m_CodeView->Center(new_line);
      return true;
    }
  }
  else if (page == 1)
  {
    // Center on any location in any valid ROM/RAM
    int seg = addr >> 12;
    if (seg == 0 || seg == 1 || seg == 8 || seg == 0xf)
    {
      m_MemView->Center(addr);
      return true;
    }
  }

  return false;
}
