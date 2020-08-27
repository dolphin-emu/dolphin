// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <ctime>

#include <QDialog>

class QGridLayout;
class QPixmap;

class DiscordJoinRequestDialog : public QDialog
{
  Q_OBJECT
public:
  explicit DiscordJoinRequestDialog(QWidget* parent, const std::string& id,
                                    const std::string& discord_tag, const std::string& avatar);
  std::time_t GetCloseTimestamp() const;

  static constexpr std::time_t s_max_lifetime_seconds = 30;

private:
  void CreateLayout(const std::string& discord_tag, const QPixmap& avatar);
  void ConnectWidgets();
  void Reply(int reply);

  QGridLayout* m_main_layout;
  QPushButton* m_invite_button;
  QPushButton* m_decline_button;
  QPushButton* m_ignore_button;

  const std::string m_user_id;
  const std::time_t m_close_timestamp;
};
