// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <wx/frame.h>

#include "Common/CommonTypes.h"
#include "Common/FifoQueue.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/NetPlayServer.h"

#ifdef _WIN32
// HACK: wxWidgets headers don't play well with some of the macros defined in Windows
// headers and perform their own magic to fix things, as long as they're included entirely
// either before or after any Windows headers.
//
// This file can cause a conflict in other DolphinWX files because NetPlay headers directly
// include ENet headers, which leak Windows header macros. To fix this, explicitly tell
// wxWidgets here that it needs to re-clean macros.
#include <wx/msw/winundef.h>
#endif

class GameListCtrl;
class MD5Dialog;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxSizer;
class wxStaticText;
class wxString;
class wxTextCtrl;

enum
{
  NP_GUI_EVT_CHANGE_GAME = 45,
  NP_GUI_EVT_START_GAME,
  NP_GUI_EVT_STOP_GAME,
  NP_GUI_EVT_DISPLAY_MD5_DIALOG,
  NP_GUI_EVT_MD5_PROGRESS,
  NP_GUI_EVT_MD5_RESULT,
  NP_GUI_EVT_PAD_BUFFER_CHANGE,
  NP_GUI_EVT_DESYNC,
  NP_GUI_EVT_CONNECTION_LOST,
  NP_GUI_EVT_TRAVERSAL_CONNECTION_ERROR,
};

enum
{
  INITIAL_PAD_BUFFER_SIZE = 5
};

enum class ChatMessageType
{
  // Info messages logged to chat
  Info,
  // Error messages logged to chat
  Error,
  // Incoming user chat messages
  UserIn,
  // Outcoming user chat messages
  UserOut,
};

// IDs are UI-dependent here
enum class MD5Target
{
  CurrentGame = 1,
  OtherGame = 2,
  SdCard = 3
};

class NetPlayDialog : public wxFrame, public NetPlayUI
{
public:
  NetPlayDialog(wxWindow* parent, const GameListCtrl* const game_list, const std::string& game,
                const bool is_hosting = false);
  ~NetPlayDialog();

  void OnStart(wxCommandEvent& event);

  // implementation of NetPlayUI methods
  void BootGame(const std::string& filename) override;
  void StopGame() override;

  void Update() override;
  void AppendChat(const std::string& msg) override;

  void ShowMD5Dialog(const std::string& file_identifier) override;
  void SetMD5Progress(int pid, int progress) override;
  void SetMD5Result(int pid, const std::string& result) override;
  void AbortMD5() override;

  void OnMsgChangeGame(const std::string& filename) override;
  void OnMsgStartGame() override;
  void OnMsgStopGame() override;
  void OnPadBufferChanged(u32 buffer) override;
  void OnDesync(u32 frame, const std::string& player) override;
  void OnConnectionLost() override;
  void OnTraversalError(TraversalClient::FailureReason error) override;

  static NetPlayDialog*& GetInstance() { return npd; }
  static NetPlayClient*& GetNetPlayClient() { return netplay_client; }
  static NetPlayServer*& GetNetPlayServer() { return netplay_server; }
  static void FillWithGameNames(wxListBox* game_lbox, const GameListCtrl& game_list);

  bool IsRecording() override;

private:
  void CreateGUI();
  wxSizer* CreateTopGUI(wxWindow* parent);
  wxSizer* CreateMiddleGUI(wxWindow* parent);
  wxSizer* CreateChatGUI(wxWindow* parent);
  wxSizer* CreatePlayerListGUI(wxWindow* parent);
  wxSizer* CreateBottomGUI(wxWindow* parent);

  void OnChat(wxCommandEvent& event);
  void OnQuit(wxCommandEvent& event);
  void OnThread(wxThreadEvent& event);
  void OnChangeGame(wxCommandEvent& event);
  void OnMD5ComputeRequested(wxCommandEvent& event);
  void OnAdjustBuffer(wxCommandEvent& event);
  void OnAssignPads(wxCommandEvent& event);
  void OnKick(wxCommandEvent& event);
  void OnPlayerSelect(wxCommandEvent& event);
  void GetNetSettings(NetSettings& settings);
  std::string FindCurrentGame();
  std::string FindGame(const std::string& game) override;
  void AddChatMessage(ChatMessageType type, const std::string& msg);

  void OnCopyIP(wxCommandEvent&);
  void OnChoice(wxCommandEvent& event);
  void UpdateHostLabel();

  wxListBox* m_player_lbox;
  wxTextCtrl* m_chat_text;
  wxTextCtrl* m_chat_msg_text;
  wxCheckBox* m_memcard_write;
  wxCheckBox* m_copy_wii_save;
  wxCheckBox* m_record_chkbox;

  std::string m_selected_game;
  wxButton* m_player_config_btn;
  wxButton* m_game_btn;
  wxButton* m_start_btn;
  wxButton* m_kick_btn;
  wxStaticText* m_host_label;
  wxChoice* m_host_type_choice;
  wxButton* m_host_copy_btn;
  wxChoice* m_MD5_choice = nullptr;
  MD5Dialog* m_MD5_dialog = nullptr;
  bool m_host_copy_btn_is_retry;
  bool m_is_hosting;
  u32 m_pad_buffer;
  u32 m_desync_frame;
  std::string m_desync_player;

  std::vector<int> m_playerids;
  Common::FifoQueue<std::string> m_chat_msgs;

  const GameListCtrl* const m_game_list;

  static NetPlayDialog* npd;
  static NetPlayServer* netplay_server;
  static NetPlayClient* netplay_client;
};
