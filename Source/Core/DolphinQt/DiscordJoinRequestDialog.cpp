// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_DISCORD_PRESENCE

#include "DolphinQt/DiscordJoinRequestDialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

#include <discord_rpc.h>
#include <fmt/format.h>

#include "Common/HttpRequest.h"

DiscordJoinRequestDialog::DiscordJoinRequestDialog(QWidget* parent, const std::string& id,
                                                   const std::string& discord_tag,
                                                   const std::string& avatar)
    : QDialog(parent), m_user_id(id), m_close_timestamp(std::time(nullptr) + s_max_lifetime_seconds)
{
  setWindowTitle(tr("Request to Join Your Party"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QPixmap avatar_pixmap;

  if (!avatar.empty())
  {
    const std::string avatar_endpoint =
        fmt::format("https://cdn.discordapp.com/avatars/{}/{}.png", id, avatar);

    Common::HttpRequest request;
    Common::HttpRequest::Response response = request.Get(avatar_endpoint);

    if (response.has_value())
      avatar_pixmap.loadFromData(response->data(), static_cast<uint>(response->size()), "png");
  }

  CreateLayout(discord_tag, avatar_pixmap);
  ConnectWidgets();
}

std::time_t DiscordJoinRequestDialog::GetCloseTimestamp() const
{
  return m_close_timestamp;
}

void DiscordJoinRequestDialog::CreateLayout(const std::string& discord_tag, const QPixmap& avatar)
{
  m_main_layout = new QGridLayout;

  m_invite_button = new QPushButton(tr("\u2714 Invite"));
  m_decline_button = new QPushButton(tr("\u2716 Decline"));
  m_ignore_button = new QPushButton(tr("Ignore"));

  QLabel* text =
      new QLabel(tr("%1\nwants to join your party.").arg(QString::fromStdString(discord_tag)));
  text->setAlignment(Qt::AlignCenter);

  if (!avatar.isNull())
  {
    QLabel* picture = new QLabel();
    picture->setPixmap(avatar);
    m_main_layout->addWidget(picture, 1, 0, 1, 3, Qt::AlignHCenter);
  }

  m_main_layout->addWidget(text, 2, 0, 3, 3, Qt::AlignHCenter);
  m_main_layout->addWidget(m_invite_button, 8, 0);
  m_main_layout->addWidget(m_decline_button, 8, 1);
  m_main_layout->addWidget(m_ignore_button, 8, 2);

  setLayout(m_main_layout);
}

void DiscordJoinRequestDialog::ConnectWidgets()
{
  connect(m_invite_button, &QPushButton::clicked, [this] { Reply(DISCORD_REPLY_YES); });
  connect(m_decline_button, &QPushButton::clicked, [this] { Reply(DISCORD_REPLY_NO); });
  connect(m_ignore_button, &QPushButton::clicked, [this] { Reply(DISCORD_REPLY_IGNORE); });
  connect(this, &QDialog::rejected, [this] { Reply(DISCORD_REPLY_IGNORE); });
}

void DiscordJoinRequestDialog::Reply(int reply)
{
  Discord_Respond(m_user_id.c_str(), reply);
  close();
}

#endif
