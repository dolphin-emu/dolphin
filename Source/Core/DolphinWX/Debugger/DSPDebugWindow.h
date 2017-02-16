// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/aui/framemanager.h>
#include <wx/panel.h>

#include "Common/CommonTypes.h"
#include "Core/HW/DSPLLE/DSPDebugInterface.h"

class DSPRegisterView;
class CCodeView;
class CMemoryView;
class wxAuiNotebook;
class DolphinAuiToolBar;
class wxListBox;

class DSPDebuggerLLE : public wxPanel
{
public:
  DSPDebuggerLLE(wxWindow* parent, wxWindowID id = wxID_ANY);
  virtual ~DSPDebuggerLLE();

  void Repopulate();

private:
  enum
  {
    ID_TOOLBAR = 1000,
    ID_RUNTOOL,
    ID_STEPTOOL,
    ID_SHOWPCTOOL,
  };

  DSP::LLE::DSPDebugInterface debug_interface;
  u64 m_CachedStepCounter;

  // GUI updaters
  void UpdateDisAsmListView();
  void UpdateRegisterFlags();
  void UpdateSymbolMap();
  void UpdateState();

  // GUI items
  wxAuiManager m_mgr;
  DolphinAuiToolBar* m_Toolbar;
  CCodeView* m_CodeView;
  CMemoryView* m_MemView;
  DSPRegisterView* m_Regs;
  wxListBox* m_SymbolList;
  wxTextCtrl* m_addr_txtctrl;
  wxAuiNotebook* m_MainNotebook;
  wxSize m_toolbar_item_size;

  void OnChangeState(wxCommandEvent& event);
  // void OnRightClick(wxListEvent& event);
  // void OnDoubleClick(wxListEvent& event);
  void OnAddrBoxChange(wxCommandEvent& event);
  void OnSymbolListChange(wxCommandEvent& event);

  bool JumpToAddress(u16 addr);

  void FocusOnPC();
  // void UnselectAll();
};
