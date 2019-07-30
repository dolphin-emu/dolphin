// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QMenuBar>

#include "Common/Lazy.h"
#include "Core/NetPlayClient.h"
#include "VideoCommon/OnScreenDisplay.h"

class ChunkedProgressDialog;
class MD5Dialog;
class GameListModel;
class PadMappingDialog;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTableWidget;
class QTextEdit;

class NetPlayDialog : public QDialog, public NetPlay::NetPlayUI
{
  Q_OBJECT
public:
  explicit NetPlayDialog(QWidget* parent = nullptr);
  ~NetPlayDialog();

  void show(std::string nickname, bool use_traversal);
  void reject() override;

  // NetPlayUI methods
  void BootGame(const std::string& filename) override;
  void StopGame() override;
  bool IsHosting() const override;

  void Update() override;
  void AppendChat(const std::string& msg) override;

  void OnMsgChangeGame(const std::string& filename) override;
  void OnMsgStartGame() override;
  void OnMsgStopGame() override;
  void OnMsgPowerButton() override;
  void OnPadBufferChanged(u32 buffer) override;
  void OnHostInputAuthorityChanged(bool enabled) override;
  void OnDesync(u32 frame, const std::string& player) override;
  void OnConnectionLost() override;
  void OnConnectionError(const std::string& message) override;
  void OnTraversalError(TraversalClient::FailureReason error) override;
  void OnTraversalStateChanged(TraversalClient::State state) override;
  void OnGameStartAborted() override;
  void OnGolferChanged(bool is_golfer, const std::string& golfer_name) override;

  void OnIndexAdded(bool success, const std::string error) override;
  void OnIndexRefreshFailed(const std::string error) override;

  bool IsRecording() override;
  std::string FindGame(const std::string& game) override;
  std::shared_ptr<const UICommon::GameFile> FindGameFile(const std::string& game) override;

  void LoadSettings();
  void SaveSettings();

  void ShowMD5Dialog(const std::string& file_identifier) override;
  void SetMD5Progress(int pid, int progress) override;
  void SetMD5Result(int pid, const std::string& result) override;
  void AbortMD5() override;

  void ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                 const std::vector<int>& players) override;
  void HideChunkedProgressDialog() override;
  void SetChunkedProgress(int pid, u64 progress) override;
signals:
  void Boot(const QString& filename);
  void Stop();

private:
  void CreateChatLayout();
  void CreatePlayersLayout();
  void CreateMainLayout();
  void ConnectWidgets();
  void OnChat();
  void OnStart();
  void DisplayMessage(const QString& msg, const std::string& color,
                      int duration = OSD::Duration::NORMAL);
  void ResetExternalIP();
  void UpdateDiscordPresence();
  void UpdateGUI();
  void GameStatusChanged(bool running);
  void SetOptionsEnabled(bool enabled);

  void SetGame(const QString& game_path);

  // Chat
  QGroupBox* m_chat_box;
  QTextEdit* m_chat_edit;
  QLineEdit* m_chat_type_edit;
  QPushButton* m_chat_send_button;

  // Players
  QGroupBox* m_players_box;
  QComboBox* m_room_box;
  QLabel* m_hostcode_label;
  QPushButton* m_hostcode_action_button;
  QTableWidget* m_players_list;
  QPushButton* m_kick_button;
  QPushButton* m_assign_ports_button;

  // Other
  QMenuBar* m_menu_bar;
  QMenu* m_data_menu;
  QMenu* m_network_menu;
  QMenu* m_md5_menu;
  QMenu* m_other_menu;
  QPushButton* m_game_button;
  QPushButton* m_start_button;
  QLabel* m_buffer_label;
  QSpinBox* m_buffer_size_box;
  QAction* m_save_sd_action;
  QAction* m_load_wii_action;
  QAction* m_sync_save_data_action;
  QAction* m_sync_codes_action;
  QAction* m_record_input_action;
  QAction* m_reduce_polling_rate_action;
  QAction* m_strict_settings_sync_action;
  QAction* m_host_input_authority_action;
  QAction* m_sync_all_wii_saves_action;
  QAction* m_golf_mode_action;
  QAction* m_golf_mode_overlay_action;
  QAction* m_fixed_delay_action;
  QPushButton* m_quit_button;
  QSplitter* m_splitter;
  QActionGroup* m_network_mode_group;

  QGridLayout* m_main_layout;
  MD5Dialog* m_md5_dialog;
  ChunkedProgressDialog* m_chunked_progress_dialog;
  PadMappingDialog* m_pad_mapping;
  std::string m_current_game;
  Common::Lazy<std::string> m_external_ip_address;
  std::string m_nickname;
  GameListModel* m_game_list_model = nullptr;
  bool m_use_traversal = false;
  bool m_is_copy_button_retry = false;
  bool m_got_stop_request = true;
  int m_buffer_size = 0;
  int m_player_count = 0;
  int m_old_player_count = 0;
  bool m_host_input_authority = false;
};
