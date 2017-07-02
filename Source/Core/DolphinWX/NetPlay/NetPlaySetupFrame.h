// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <wx/frame.h>

class GameListCtrl;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxNotebook;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

class NetPlaySetupFrame final : public wxFrame
{
public:
  NetPlaySetupFrame(wxWindow* const parent, const GameListCtrl* const game_list);
  ~NetPlaySetupFrame();

private:
  static constexpr int CONNECT_TAB = 0;
  static constexpr int HOST_TAB = 1;

  static constexpr int DIRECT_CHOICE = 0;
  static constexpr int TRAVERSAL_CHOICE = 1;

  void CreateGUI();
  wxNotebook* CreateNotebookGUI(wxWindow* parent);

  void OnJoin(wxCommandEvent& event);
  void OnHost(wxCommandEvent& event);
  void DoJoin();
  void DoHost();
  void OnQuit(wxCommandEvent& event);
  void OnDirectTraversalChoice(wxCommandEvent& event);
  void OnResetTraversal(wxCommandEvent& event);
  void OnTraversalListenPortChanged(wxCommandEvent& event);
  void OnKeyDown(wxKeyEvent& event);
  void OnTabChanged(wxCommandEvent& event);
  void DispatchFocus();

  wxStaticText* m_ip_lbl;
  wxStaticText* m_client_port_lbl;
  wxTextCtrl* m_nickname_text;
  wxStaticText* m_host_port_lbl;
  wxTextCtrl* m_host_port_text;
  wxTextCtrl* m_connect_port_text;
  wxTextCtrl* m_connect_ip_text;
  wxTextCtrl* m_connect_hashcode_text;
  wxChoice* m_direct_traversal;
  wxStaticText* m_traversal_lbl;
  wxButton* m_trav_reset_btn;
  wxCheckBox* m_traversal_listen_port_enabled;
  wxSpinCtrl* m_traversal_listen_port;
  wxNotebook* m_notebook;

  wxListBox* m_game_lbox;
#ifdef USE_UPNP
  wxCheckBox* m_upnp_chk;
#endif

  wxString m_traversal_string;
  const GameListCtrl* const m_game_list;
};
