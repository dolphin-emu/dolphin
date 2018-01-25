// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <wx/aui/framemanager.h>
#include <wx/panel.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "DolphinWX/Globals.h"

class CCodeView;
class CFrame;
struct SConfig;
class CBreakPointWindow;
class CRegisterWindow;
class CWatchWindow;
class CMemoryWindow;
class CJitWindow;
class DSPDebuggerLLE;
class GFXDebuggerPanel;

class DolphinAuiToolBar;
class wxListBox;
class wxMenu;
class wxMenuBar;
class wxSearchCtrl;
class wxToolBar;

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif

namespace Details
{
template <class T>
struct DebugPanelToID;

template <>
struct DebugPanelToID<CBreakPointWindow>
{
  static constexpr int ID = IDM_BREAKPOINT_WINDOW;
};
template <>
struct DebugPanelToID<CRegisterWindow>
{
  static constexpr int ID = IDM_REGISTER_WINDOW;
};
template <>
struct DebugPanelToID<CWatchWindow>
{
  static constexpr int ID = IDM_WATCH_WINDOW;
};
template <>
struct DebugPanelToID<CMemoryWindow>
{
  static constexpr int ID = IDM_MEMORY_WINDOW;
};
template <>
struct DebugPanelToID<CJitWindow>
{
  static constexpr int ID = IDM_JIT_WINDOW;
};
template <>
struct DebugPanelToID<DSPDebuggerLLE>
{
  static constexpr int ID = IDM_SOUND_WINDOW;
};
template <>
struct DebugPanelToID<GFXDebuggerPanel>
{
  static constexpr int ID = IDM_VIDEO_WINDOW;
};
}

class CCodeWindow : public wxPanel
{
public:
  explicit CCodeWindow(CFrame* parent, wxWindowID id = wxID_ANY,
                       const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                       long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
                       const wxString& name = _("Code"));
  ~CCodeWindow();

  void Load();
  void Save();

  bool JumpToAddress(u32 address);

  void Repopulate(bool refresh_codeview = true);
  void NotifyMapLoaded();
  void OpenPages();

  // FIXME: This belongs in a separate class.
  void TogglePanel(int id, bool show);
  wxPanel* GetUntypedPanel(int id) const;
  bool HasUntypedPanel(int id) const { return GetUntypedPanel(id) != nullptr; }
  template <class T>
  T* GetPanel() const
  {
    return static_cast<T*>(GetUntypedPanel(Details::DebugPanelToID<T>::ID));
  }
  template <class T>
  bool HasPanel() const
  {
    return HasUntypedPanel(Details::DebugPanelToID<T>::ID);
  }
  template <class T>
  T* RequirePanel()
  {
    if (T* p = GetPanel<T>())
      return p;

    TogglePanel(Details::DebugPanelToID<T>::ID, true);
    return GetPanel<T>();
  }

  // Settings
  bool bShowOnStart[IDM_DEBUG_WINDOW_LIST_END - IDM_DEBUG_WINDOW_LIST_START];
  int iNbAffiliation[IDM_DEBUG_WINDOW_LIST_END - IDM_DEBUG_WINDOW_LIST_START];

private:
  wxMenuBar* GetParentMenuBar();

  void OnCPUMode(wxCommandEvent& event);

  void OnChangeFont(wxCommandEvent& event);

  void OnCodeStep(wxCommandEvent& event);
  void OnAddrBoxChange(wxCommandEvent& event);
  void OnSymbolFilterText(wxCommandEvent& event);
  void OnSymbolsMenu(wxCommandEvent& event);
  void OnJitMenu(wxCommandEvent& event);
  void OnProfilerMenu(wxCommandEvent& event);

  void OnBootToPauseSelected(wxCommandEvent& event);
  void OnAutomaticStartSelected(wxCommandEvent& event);

  void OnSymbolListChange(wxCommandEvent& event);
  void OnCallstackListChange(wxCommandEvent& event);
  void OnCallersListChange(wxCommandEvent& event);
  void OnCallsListChange(wxCommandEvent& event);
  void OnCodeViewChange(wxCommandEvent& event);
  void OnHostMessage(wxCommandEvent& event);

  // Debugger functions
  void SingleStep();
  void StepOver();
  void StepOut();
  void ToggleBreakpoint();

  void UpdateFonts();
  void UpdateLists();
  void UpdateCallstack();

  void ReloadSymbolListBox();

  wxPanel* CreateSiblingPanel(int id);

  // Sibling debugger panels
  // FIXME: This obviously belongs in some manager class above this one.
  std::array<wxPanel*, IDM_DEBUG_WINDOW_LIST_END - IDM_DEBUG_WINDOW_LIST_START> m_sibling_panels{};

  CFrame* Parent;
  CCodeView* codeview;
  wxSearchCtrl* m_symbol_filter_ctrl;
  wxListBox* callstack;
  wxListBox* symbols;
  wxListBox* callers;
  wxListBox* calls;
  Common::Event sync_event;

  wxAuiManager m_aui_manager;
  DolphinAuiToolBar* m_aui_toolbar;
};
#if defined(_MSC_VER) && _MSC_VER <= 1800
#undef constexpr
#endif
