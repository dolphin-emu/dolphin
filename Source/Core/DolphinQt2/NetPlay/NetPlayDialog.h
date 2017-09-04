// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Core/NetPlayClient.h"
#include "VideoCommon/OnScreenDisplay.h"

class MD5Dialog;
class GameListModel;
class NetPlayServer;
class PadMappingDialog;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTextEdit;

class NetPlayDialog : public QDialog, public NetPlayUI
{
  Q_OBJECT
public:
  NetPlayDialog(QWidget* parent);

  void show(std::string nickname, bool use_traversal);
  void reject() override;

  // NetPlayUI methods
  void BootGame(const std::string& filename) override;
  void StopGame() override;

  void Update() override;
  void AppendChat(const std::string& msg) override;

  void OnMsgChangeGame(const std::string& filename) override;
  void OnMsgStartGame() override;
  void OnMsgStopGame() override;
  void OnPadBufferChanged(u32 buffer) override;
  void OnDesync(u32 frame, const std::string& player) override;
  void OnConnectionLost() override;
  void OnTraversalError(TraversalClient::FailureReason error) override;
  bool IsRecording() override;
  std::string FindGame(const std::string& game) override;
  void ShowMD5Dialog(const std::string& file_identifier) override;
  void SetMD5Progress(int pid, int progress) override;
  void SetMD5Result(int pid, const std::string& result) override;
  void AbortMD5() override;
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
  void OnMD5Combo(int index);
  void DisplayMessage(const QString& msg, const std::string& color,
                      int duration = OSD::Duration::NORMAL);
  void UpdateGUI();
  void GameStatusChanged(bool running);

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
  QListWidget* m_players_list;
  QPushButton* m_kick_button;
  QPushButton* m_assign_ports_button;

  // Other
  QPushButton* m_game_button;
  QComboBox* m_md5_box;
  QPushButton* m_start_button;
  QLabel* m_buffer_label;
  QSpinBox* m_buffer_size_box;
  QCheckBox* m_save_sd_box;
  QCheckBox* m_load_wii_box;
  QCheckBox* m_record_input_box;
  QPushButton* m_quit_button;

  QGridLayout* m_main_layout;
  MD5Dialog* m_md5_dialog;
  PadMappingDialog* m_pad_mapping;
  std::string m_current_game;
  std::string m_nickname;
  GameListModel* m_game_list_model = nullptr;
  bool m_use_traversal = false;
  bool m_is_copy_button_retry = false;
};
