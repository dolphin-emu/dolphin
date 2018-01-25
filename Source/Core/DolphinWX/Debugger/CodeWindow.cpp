// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/CodeWindow.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

// clang-format off
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/thread.h>
#include <wx/aui/auibar.h>
#include <wx/aui/dockart.h>
// clang-format on

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeView.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/JitWindow.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Debugger/RegisterWindow.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/AuiToolBar.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"

CCodeWindow::CCodeWindow(CFrame* parent, wxWindowID id, const wxPoint& position, const wxSize& size,
                         long style, const wxString& name)
    : wxPanel(parent, id, position, size, style, name), Parent(parent)
{
  DebugInterface* di = &PowerPC::debug_interface;

  codeview = new CCodeView(di, &g_symbolDB, this, wxID_ANY);

  callstack = new wxListBox(this, wxID_ANY);
  callstack->Bind(wxEVT_LISTBOX, &CCodeWindow::OnCallstackListChange, this);

  symbols = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
  symbols->Bind(wxEVT_LISTBOX, &CCodeWindow::OnSymbolListChange, this);

  calls = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
  calls->Bind(wxEVT_LISTBOX, &CCodeWindow::OnCallsListChange, this);

  callers = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
  callers->Bind(wxEVT_LISTBOX, &CCodeWindow::OnCallersListChange, this);

  m_aui_toolbar = new DolphinAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxAUI_TB_HORIZONTAL | wxAUI_TB_PLAIN_BACKGROUND);

  wxSearchCtrl* const address_searchctrl = new wxSearchCtrl(m_aui_toolbar, IDM_ADDRBOX);
  address_searchctrl->Bind(wxEVT_TEXT, &CCodeWindow::OnAddrBoxChange, this);
  address_searchctrl->SetDescriptiveText(_("Search Address"));
  m_symbol_filter_ctrl = new wxSearchCtrl(m_aui_toolbar, wxID_ANY);
  m_symbol_filter_ctrl->Bind(wxEVT_TEXT, &CCodeWindow::OnSymbolFilterText, this);
  m_symbol_filter_ctrl->SetDescriptiveText(_("Filter Symbols"));
  m_symbol_filter_ctrl->SetToolTip(_("Filter the symbol list by name. This is case-sensitive."));

  m_aui_toolbar->AddControl(address_searchctrl);
  m_aui_toolbar->AddControl(m_symbol_filter_ctrl);
  m_aui_toolbar->Realize();

  m_aui_manager.SetManagedWindow(this);
  m_aui_manager.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);
  m_aui_manager.AddPane(m_aui_toolbar, wxAuiPaneInfo().ToolbarPane().Top().Floatable(false));
  m_aui_manager.AddPane(callstack, wxAuiPaneInfo()
                                       .MinSize(FromDIP(wxSize(150, 100)))
                                       .Left()
                                       .CloseButton(false)
                                       .Floatable(false)
                                       .Caption(_("Callstack")));
  m_aui_manager.AddPane(symbols, wxAuiPaneInfo()
                                     .MinSize(FromDIP(wxSize(150, 100)))
                                     .Left()
                                     .CloseButton(false)
                                     .Floatable(false)
                                     .Caption(_("Symbols")));
  m_aui_manager.AddPane(calls, wxAuiPaneInfo()
                                   .MinSize(FromDIP(wxSize(150, 100)))
                                   .Left()
                                   .CloseButton(false)
                                   .Floatable(false)
                                   .Caption(_("Function calls")));
  m_aui_manager.AddPane(callers, wxAuiPaneInfo()
                                     .MinSize(FromDIP(wxSize(150, 100)))
                                     .Left()
                                     .CloseButton(false)
                                     .Floatable(false)
                                     .Caption(_("Function callers")));
  m_aui_manager.AddPane(codeview, wxAuiPaneInfo().CenterPane().CloseButton(false).Floatable(false));
  m_aui_manager.Update();

  // Menu
  Bind(wxEVT_MENU, &CCodeWindow::OnCPUMode, this, IDM_INTERPRETER, IDM_JIT_SR_OFF);
  Bind(wxEVT_MENU, &CCodeWindow::OnChangeFont, this, IDM_FONT_PICKER);
  Bind(wxEVT_MENU, &CCodeWindow::OnJitMenu, this, IDM_CLEAR_CODE_CACHE, IDM_SEARCH_INSTRUCTION);
  Bind(wxEVT_MENU, &CCodeWindow::OnSymbolsMenu, this, IDM_CLEAR_SYMBOLS, IDM_PATCH_HLE_FUNCTIONS);
  Bind(wxEVT_MENU, &CCodeWindow::OnProfilerMenu, this, IDM_PROFILE_BLOCKS, IDM_WRITE_PROFILE);
  Bind(wxEVT_MENU, &CCodeWindow::OnBootToPauseSelected, this, IDM_BOOT_TO_PAUSE);
  Bind(wxEVT_MENU, &CCodeWindow::OnAutomaticStartSelected, this, IDM_AUTOMATIC_START);

  // Toolbar
  Bind(wxEVT_MENU, &CCodeWindow::OnCodeStep, this, IDM_STEP, IDM_GOTOPC);

  // Other
  Bind(wxEVT_HOST_COMMAND, &CCodeWindow::OnHostMessage, this);
}

CCodeWindow::~CCodeWindow()
{
  m_aui_manager.UnInit();
}

wxMenuBar* CCodeWindow::GetParentMenuBar()
{
  return Parent->GetMenuBar();
}

// ----------
// Events

void CCodeWindow::OnHostMessage(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_NOTIFY_MAP_LOADED:
    NotifyMapLoaded();
    if (HasPanel<CBreakPointWindow>())
      GetPanel<CBreakPointWindow>()->NotifyUpdate();
    break;

  case IDM_UPDATE_DISASM_DIALOG:
    codeview->Center(PC);
    Repopulate(false);
    if (HasPanel<CRegisterWindow>())
      GetPanel<CRegisterWindow>()->NotifyUpdate();
    if (HasPanel<CWatchWindow>())
      GetPanel<CWatchWindow>()->NotifyUpdate();
    if (HasPanel<CMemoryWindow>())
      GetPanel<CMemoryWindow>()->Refresh();
    break;

  case IDM_UPDATE_BREAKPOINTS:
    Repopulate();
    if (HasPanel<CBreakPointWindow>())
      GetPanel<CBreakPointWindow>()->NotifyUpdate();
    if (HasPanel<CMemoryWindow>())
      GetPanel<CMemoryWindow>()->Refresh();
    break;

  case IDM_UPDATE_JIT_PANE:
    RequirePanel<CJitWindow>()->ViewAddr(codeview->GetSelection());
    break;
  }
}

// The Play, Stop, Step, Skip, Go to PC and Show PC buttons go here
void CCodeWindow::OnCodeStep(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_STEP:
    SingleStep();
    break;

  case IDM_STEPOVER:
    StepOver();
    break;

  case IDM_STEPOUT:
    StepOut();
    break;

  case IDM_TOGGLE_BREAKPOINT:
    ToggleBreakpoint();
    break;

  case IDM_SKIP:
    PC += 4;
    codeview->Center(PC);
    Repopulate(false);
    break;

  case IDM_SETPC:
    PC = codeview->GetSelection();
    codeview->Center(PC);
    Repopulate(false);
    break;

  case IDM_GOTOPC:
    JumpToAddress(PC);
    break;
  }

  // Update all toolbars in the aui manager
  Parent->UpdateGUI();
}

bool CCodeWindow::JumpToAddress(u32 address)
{
  // Jump to anywhere in memory
  if (address <= 0xFFFFFFFF)
  {
    codeview->Center(address);
    UpdateLists();

    return true;
  }

  return false;
}

void CCodeWindow::OnCodeViewChange(wxCommandEvent& event)
{
  UpdateLists();
}

void CCodeWindow::OnAddrBoxChange(wxCommandEvent& event)
{
  wxSearchCtrl* pAddrCtrl = (wxSearchCtrl*)m_aui_toolbar->FindControl(IDM_ADDRBOX);

  // Trim leading and trailing whitespace.
  wxString txt = pAddrCtrl->GetValue().Trim().Trim(false);

  bool success = false;
  unsigned long addr;
  if (txt.ToULong(&addr, 16))
  {
    if (JumpToAddress(addr))
      success = true;
  }

  if (success)
    pAddrCtrl->SetBackgroundColour(wxNullColour);
  else if (!txt.empty())
    pAddrCtrl->SetBackgroundColour(*wxRED);

  pAddrCtrl->Refresh();

  event.Skip();
}

void CCodeWindow::OnSymbolFilterText(wxCommandEvent&)
{
  ReloadSymbolListBox();
}

void CCodeWindow::OnCallstackListChange(wxCommandEvent& event)
{
  int index = callstack->GetSelection();
  if (index >= 0)
  {
    u32 address = (u32)(u64)(callstack->GetClientData(index));
    if (address)
      JumpToAddress(address);
  }
}

void CCodeWindow::OnCallersListChange(wxCommandEvent& event)
{
  int index = callers->GetSelection();
  if (index >= 0)
  {
    u32 address = (u32)(u64)(callers->GetClientData(index));
    if (address)
      JumpToAddress(address);
  }
}

void CCodeWindow::OnCallsListChange(wxCommandEvent& event)
{
  int index = calls->GetSelection();
  if (index >= 0)
  {
    u32 address = (u32)(u64)(calls->GetClientData(index));
    if (address)
      JumpToAddress(address);
  }
}

void CCodeWindow::SingleStep()
{
  if (CPU::IsStepping())
  {
    PowerPC::CoreMode old_mode = PowerPC::GetMode();
    PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
    PowerPC::breakpoints.ClearAllTemporary();
    CPU::StepOpcode(&sync_event);
    sync_event.WaitFor(std::chrono::milliseconds(20));
    PowerPC::SetMode(old_mode);
    Core::DisplayMessage(_("Step successful!").ToStdString(), 2000);
    // Will get a IDM_UPDATE_DISASM_DIALOG. Don't update the GUI here.
  }
}

void CCodeWindow::StepOver()
{
  if (CPU::IsStepping())
  {
    UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
    if (inst.LK)
    {
      PowerPC::breakpoints.ClearAllTemporary();
      PowerPC::breakpoints.Add(PC + 4, true);
      CPU::EnableStepping(false);
      Core::DisplayMessage(_("Step over in progress...").ToStdString(), 2000);
    }
    else
    {
      SingleStep();
    }
  }
}

// Returns true on a rfi, blr or on a bclr that evaluates to true.
static bool WillInstructionReturn(UGeckoInstruction inst)
{
  // Is a rfi instruction
  if (inst.hex == 0x4C000064u)
    return true;
  bool counter = (inst.BO_2 >> 2 & 1) != 0 || (CTR != 0) != ((inst.BO_2 >> 1 & 1) != 0);
  bool condition = inst.BO_2 >> 4 != 0 || GetCRBit(inst.BI_2) == (inst.BO_2 >> 3 & 1);
  // bool isBclr = inst.OPCD_7 == 0b010011 && (inst.hex >> 1 & 0b10000) != 0;
  bool isBclr = inst.OPCD_7 == 0x13 && ((inst.hex >> 1) & 0x10) != 0;
  return isBclr && counter && condition && !inst.LK_3;
}

void CCodeWindow::StepOut()
{
  if (CPU::IsStepping())
  {
    CPU::PauseAndLock(true, false);
    PowerPC::breakpoints.ClearAllTemporary();

    // Keep stepping until the next return instruction or timeout after five seconds
    using clock = std::chrono::steady_clock;
    clock::time_point timeout = clock::now() + std::chrono::seconds(5);
    PowerPC::CoreMode old_mode = PowerPC::GetMode();
    PowerPC::SetMode(PowerPC::CoreMode::Interpreter);

    // Loop until either the current instruction is a return instruction with no Link flag
    // or a breakpoint is detected so it can step at the breakpoint. If the PC is currently
    // on a breakpoint, skip it.
    UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
    do
    {
      if (WillInstructionReturn(inst))
      {
        PowerPC::SingleStep();
        break;
      }

      if (inst.LK)
      {
        // Step over branches
        u32 next_pc = PC + 4;
        do
        {
          PowerPC::SingleStep();
        } while (PC != next_pc && clock::now() < timeout &&
                 !PowerPC::breakpoints.IsAddressBreakPoint(PC));
      }
      else
      {
        PowerPC::SingleStep();
      }

      inst = PowerPC::HostRead_Instruction(PC);
    } while (clock::now() < timeout && !PowerPC::breakpoints.IsAddressBreakPoint(PC));

    PowerPC::SetMode(old_mode);
    CPU::PauseAndLock(false, false);

    wxCommandEvent ev(wxEVT_HOST_COMMAND, IDM_UPDATE_DISASM_DIALOG);
    GetEventHandler()->ProcessEvent(ev);

    if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
      Core::DisplayMessage(_("Breakpoint encountered! Step out aborted.").ToStdString(), 2000);
    else if (clock::now() >= timeout)
      Core::DisplayMessage(_("Step out timed out!").ToStdString(), 2000);
    else
      Core::DisplayMessage(_("Step out successful!").ToStdString(), 2000);

    // Update all toolbars in the aui manager
    Parent->UpdateGUI();
  }
}

void CCodeWindow::ToggleBreakpoint()
{
  if (CPU::IsStepping())
  {
    if (codeview)
      codeview->ToggleBreakpoint(codeview->GetSelection());
    Repopulate();
  }
}

void CCodeWindow::UpdateLists()
{
  callers->Clear();
  u32 addr = codeview->GetSelection();
  Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);
  if (!symbol)
    return;

  for (auto& call : symbol->callers)
  {
    u32 caller_addr = call.callAddress;
    Symbol* caller_symbol = g_symbolDB.GetSymbolFromAddr(caller_addr);
    if (caller_symbol)
    {
      int idx = callers->Append(StrToWxStr(
          StringFromFormat("< %s (%08x)", caller_symbol->name.c_str(), caller_addr).c_str()));
      callers->SetClientData(idx, (void*)(u64)caller_addr);
    }
  }

  calls->Clear();
  for (auto& call : symbol->calls)
  {
    u32 call_addr = call.function;
    Symbol* call_symbol = g_symbolDB.GetSymbolFromAddr(call_addr);
    if (call_symbol)
    {
      int idx = calls->Append(StrToWxStr(
          StringFromFormat("> %s (%08x)", call_symbol->name.c_str(), call_addr).c_str()));
      calls->SetClientData(idx, (void*)(u64)call_addr);
    }
  }
}

void CCodeWindow::UpdateCallstack()
{
  if (Core::GetState() == Core::State::Stopping)
    return;

  callstack->Clear();

  std::vector<Dolphin_Debugger::CallstackEntry> stack;

  bool ret = Dolphin_Debugger::GetCallstack(stack);

  for (auto& frame : stack)
  {
    int idx = callstack->Append(StrToWxStr(frame.Name));
    callstack->SetClientData(idx, (void*)(u64)frame.vAddress);
  }

  if (!ret)
    callstack->Append(StrToWxStr("invalid callstack"));
}

// CPU Mode and JIT Menu
void CCodeWindow::OnCPUMode(wxCommandEvent& event)
{
  Core::RunAsCPUThread([&event] {
    switch (event.GetId())
    {
    case IDM_INTERPRETER:
      PowerPC::SetMode(event.IsChecked() ? PowerPC::CoreMode::Interpreter : PowerPC::CoreMode::JIT);
      break;
    case IDM_JIT_OFF:
      SConfig::GetInstance().bJITOff = event.IsChecked();
      break;
    case IDM_JIT_LS_OFF:
      SConfig::GetInstance().bJITLoadStoreOff = event.IsChecked();
      break;
    case IDM_JIT_LSLXZ_OFF:
      SConfig::GetInstance().bJITLoadStorelXzOff = event.IsChecked();
      break;
    case IDM_JIT_LSLWZ_OFF:
      SConfig::GetInstance().bJITLoadStorelwzOff = event.IsChecked();
      break;
    case IDM_JIT_LSLBZX_OFF:
      SConfig::GetInstance().bJITLoadStorelbzxOff = event.IsChecked();
      break;
    case IDM_JIT_LSF_OFF:
      SConfig::GetInstance().bJITLoadStoreFloatingOff = event.IsChecked();
      break;
    case IDM_JIT_LSP_OFF:
      SConfig::GetInstance().bJITLoadStorePairedOff = event.IsChecked();
      break;
    case IDM_JIT_FP_OFF:
      SConfig::GetInstance().bJITFloatingPointOff = event.IsChecked();
      break;
    case IDM_JIT_I_OFF:
      SConfig::GetInstance().bJITIntegerOff = event.IsChecked();
      break;
    case IDM_JIT_P_OFF:
      SConfig::GetInstance().bJITPairedOff = event.IsChecked();
      break;
    case IDM_JIT_SR_OFF:
      SConfig::GetInstance().bJITSystemRegistersOff = event.IsChecked();
      break;
    }

    // Clear the JIT cache to enable these changes
    JitInterface::ClearCache();
  });
}

void CCodeWindow::OnJitMenu(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_LOG_INSTRUCTIONS:
    PPCTables::LogCompiledInstructions();
    break;

  case IDM_CLEAR_CODE_CACHE:
    Core::RunAsCPUThread(JitInterface::ClearCache);
    break;

  case IDM_SEARCH_INSTRUCTION:
  {
    wxString str = wxGetTextFromUser("", _("Op?"), wxEmptyString, this);
    auto const wx_name = WxStrToStr(str);
    bool found = false;
    for (u32 addr = 0x80000000; addr < 0x80180000; addr += 4)
    {
      const char* name = PPCTables::GetInstructionName(PowerPC::HostRead_U32(addr));
      if (name && (wx_name == name))
      {
        NOTICE_LOG(POWERPC, "Found %s at %08x", wx_name.c_str(), addr);
        found = true;
      }
    }
    if (!found)
      NOTICE_LOG(POWERPC, "Opcode %s not found", wx_name.c_str());
    break;
  }
  }
}

// Update GUI
void CCodeWindow::Repopulate(bool refresh_codeview)
{
  if (!codeview)
    return;

  if (refresh_codeview)
    codeview->Refresh();

  UpdateCallstack();

  // Do not automatically show the current PC position when a breakpoint is hit or
  // when we pause since this can be called at other times too.
  // codeview->Center(PC);
}

void CCodeWindow::UpdateFonts()
{
  callstack->SetFont(DebuggerFont);
  symbols->SetFont(DebuggerFont);
  callers->SetFont(DebuggerFont);
  calls->SetFont(DebuggerFont);
  m_aui_manager.GetArtProvider()->SetFont(wxAUI_DOCKART_CAPTION_FONT, DebuggerFont);
  m_aui_manager.Update();
}
