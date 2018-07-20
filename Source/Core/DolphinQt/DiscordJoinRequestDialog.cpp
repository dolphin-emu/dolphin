// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef USE_DISCORD_PRESENCE

#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

#include <discord-rpc/include/discord_rpc.h>

#include "Common/HttpRequest.h"
#include "Common/StringUtil.h"

#include "DolphinQt/DiscordJoinRequestDialog.h"

DiscordJoinRequestDialog::DiscordJoinRequestDialog(QWidget* parent, const char* id,
                                                   const std::string& discord_tag,
                                                   const char* avatar)
    : QDialog(parent), m_user_id(id), m_close_timestamp(std::time(nullptr) + s_max_lifetime_seconds)
{
  setWindowTitle(tr("Request to Join Your Party"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QPixmap avatar_pixmap;

  if (avatar[0] != '\0')
  {
    const std::string avatar_endpoint =
        StringFromFormat("https://cdn.discordapp.com/avatars/%s/%s.png", id, avatar);

    Common::HttpRequest request;
    Common::HttpRequest::Response response = request.Get(avatar_endpoint);

    if (response.has_value())
      avatar_pixmap.loadFromData(response->data(), static_cast<uint>(response->size()), "png");
  }

  CreateMainLayout(discord_tag, avatar_pixmap);
  ConnectWidgets();
}

std::time_t DiscordJoinRequestDialog::GetCloseTimestamp() const
{
  return m_close_timestamp;
}

void DiscordJoinRequestDialog::CreateMainLayout(const std::string& discord_tag,
                                                const QPixmap& avatar)
{
  m_main_layout = new QGridLayout;

  m_invite_button = new QPushButton(QString::fromWCharArray(L"\u2714 Invite"));
  m_decline_button = new QPushButton(QString::fromWCharArray(L"\u2716 Decline"));
  m_ignore_button = new QPushButton(tr("Ignore"));

  if (!avatar.isNull())
  {
    QLabel* picture = new QLabel();
    picture->setPixmap(avatar);
    m_main_layout->addWidget(picture, 1, 0, 1, 3, Qt::AlignHCenter);
  }

  m_main_layout->addWidget(new QLabel(tr(discord_tag.c_str())), 2, 0, 3, 3, Qt::AlignHCenter);
  m_main_layout->addWidget(new QLabel(tr("wants to join your party.")), 4, 0, 4, 3,
                           Qt::AlignHCenter);
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
  connect(this, &QDialog::rejected, this, [this] { Reply(DISCORD_REPLY_IGNORE); });
}

void DiscordJoinRequestDialog::Reply(int reply)
{
  Discord_Respond(m_user_id, reply);
  close();
}

#endif
