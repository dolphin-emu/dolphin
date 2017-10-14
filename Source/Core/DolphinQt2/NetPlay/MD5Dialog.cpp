// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "MD5Dialog.h"

#include "DolphinQt2/Settings.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

static QString GetPlayerNameFromPID(int pid)
{
  QString player_name = QObject::tr("Invalid Player ID");
  for (const auto* player : Settings::Instance().GetNetPlayClient()->GetPlayers())
  {
    if (player->pid == pid)
    {
      player_name = QString::fromStdString(player->name);
      break;
    }
  }
  return player_name;
}

MD5Dialog::MD5Dialog(QWidget* parent) : QDialog(parent)
{
  CreateWidgets();
  ConnectWidgets();
  setWindowTitle(tr("MD5 Checksum"));
}

void MD5Dialog::CreateWidgets()
{
  m_main_layout = new QVBoxLayout;
  m_progress_box = new QGroupBox;
  m_progress_layout = new QVBoxLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  m_check_label = new QLabel;

  m_progress_box->setLayout(m_progress_layout);

  m_main_layout->addWidget(m_progress_box);
  m_main_layout->addWidget(m_check_label);
  m_main_layout->addWidget(m_button_box);
  setLayout(m_main_layout);
}

void MD5Dialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &MD5Dialog::reject);
}

void MD5Dialog::show(const QString& title)
{
  m_progress_box->setTitle(title);

  for (auto& pair : m_progress_bars)
  {
    m_progress_layout->removeWidget(pair.second);
    pair.second->deleteLater();
  }

  for (auto& pair : m_status_labels)
  {
    m_progress_layout->removeWidget(pair.second);
    pair.second->deleteLater();
  }

  m_progress_bars.clear();
  m_status_labels.clear();

  for (const auto* player : Settings::Instance().GetNetPlayClient()->GetPlayers())
  {
    m_progress_bars[player->pid] = new QProgressBar;
    m_status_labels[player->pid] = new QLabel;

    m_progress_layout->addWidget(m_progress_bars[player->pid]);
    m_progress_layout->addWidget(m_status_labels[player->pid]);
  }

  m_last_result = "";

  QDialog::show();
}

void MD5Dialog::SetProgress(int pid, int progress)
{
  QString player_name = GetPlayerNameFromPID(pid);

  if (!m_status_labels.count(pid))
    return;

  m_status_labels[pid]->setText(
      tr("%1[%2]: %3 %").arg(player_name, QString::number(pid), QString::number(progress)));
  m_progress_bars[pid]->setValue(progress);
}

void MD5Dialog::SetResult(int pid, const std::string& result)
{
  QString player_name = GetPlayerNameFromPID(pid);

  if (!m_status_labels.count(pid))
    return;

  m_status_labels[pid]->setText(
      tr("%1[%2]: %3").arg(player_name, QString::number(pid), QString::fromStdString(result)));

  if (m_last_result == "")
  {
    m_check_label->setText(tr("The hashes match!"));
    return;
  }

  if (m_last_result != result)
  {
    m_check_label->setText(tr("The hashes do not match!"));
  }

  m_last_result = result;
}

void MD5Dialog::reject()
{
  auto* server = Settings::Instance().GetNetPlayServer();

  if (server)
    server->AbortMD5();

  QDialog::reject();
}
