// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/GameDigestDialog.h"

#include <algorithm>
#include <functional>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/Settings.h"

static QString GetPlayerNameFromPID(int pid)
{
  QString player_name = QObject::tr("Invalid Player ID");
  auto client = Settings::Instance().GetNetPlayClient();
  if (!client)
    return player_name;

  for (const auto* player : client->GetPlayers())
  {
    if (player->pid == pid)
    {
      player_name = QString::fromStdString(player->name);
      break;
    }
  }
  return player_name;
}

GameDigestDialog::GameDigestDialog(QWidget* parent) : QDialog(parent)
{
  CreateWidgets();
  ConnectWidgets();
  setWindowTitle(tr("SHA1 Digest"));
  setWindowFlags(Qt::Sheet | Qt::Dialog);
  setWindowModality(Qt::WindowModal);
}

void GameDigestDialog::CreateWidgets()
{
  m_main_layout = new QVBoxLayout;
  m_progress_box = new QGroupBox;
  m_progress_layout = new QVBoxLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::NoButton);
  m_check_label = new QLabel;

  m_progress_box->setLayout(m_progress_layout);

  m_main_layout->addWidget(m_progress_box);
  m_main_layout->addWidget(m_check_label);
  m_main_layout->addWidget(m_button_box);
  setLayout(m_main_layout);
}

void GameDigestDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &GameDigestDialog::reject);
}

void GameDigestDialog::show(const QString& title)
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
  m_results.clear();
  m_check_label->setText(QString::fromStdString(""));

  auto client = Settings::Instance().GetNetPlayClient();
  if (!client)
    return;

  if (Settings::Instance().GetNetPlayServer())
  {
    m_button_box->setStandardButtons(QDialogButtonBox::Cancel);
    QPushButton* cancel_button = m_button_box->button(QDialogButtonBox::Cancel);
    cancel_button->setAutoDefault(false);
    cancel_button->setDefault(false);
  }
  else
  {
    m_button_box->setStandardButtons(QDialogButtonBox::Close);
    QPushButton* close_button = m_button_box->button(QDialogButtonBox::Close);
    close_button->setAutoDefault(false);
    close_button->setDefault(false);
  }

  for (const auto* player : client->GetPlayers())
  {
    m_progress_bars[player->pid] = new QProgressBar;
    m_status_labels[player->pid] = new QLabel;

    m_progress_layout->addWidget(m_progress_bars[player->pid]);
    m_progress_layout->addWidget(m_status_labels[player->pid]);
  }

  QDialog::show();
}

void GameDigestDialog::SetProgress(int pid, int progress)
{
  QString player_name = GetPlayerNameFromPID(pid);

  if (!m_status_labels.contains(pid))
    return;

  m_status_labels[pid]->setText(
      tr("%1[%2]: %3 %").arg(player_name, QString::number(pid), QString::number(progress)));
  m_progress_bars[pid]->setValue(progress);
}

void GameDigestDialog::SetResult(int pid, const std::string& result)
{
  QString player_name = GetPlayerNameFromPID(pid);

  if (!m_status_labels.contains(pid))
    return;

  m_status_labels[pid]->setText(
      tr("%1[%2]: %3").arg(player_name, QString::number(pid), QString::fromStdString(result)));

  m_results.push_back(result);

  auto client = Settings::Instance().GetNetPlayClient();
  if (client && m_results.size() >= client->GetPlayers().size())
  {
    if (std::adjacent_find(m_results.begin(), m_results.end(), std::not_equal_to<>()) ==
        m_results.end())
    {
      m_check_label->setText(tr("The hashes match!"));
    }
    else
    {
      m_check_label->setText(tr("The hashes do not match!"));
    }

    m_button_box->setStandardButtons(QDialogButtonBox::Close);
    QPushButton* close_button = m_button_box->button(QDialogButtonBox::Close);
    close_button->setAutoDefault(false);
    close_button->setDefault(false);
  }
}

void GameDigestDialog::reject()
{
  if (auto server = Settings::Instance().GetNetPlayServer())
    server->AbortGameDigest();

  QDialog::reject();
}
