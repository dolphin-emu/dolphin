// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/PadMappingDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/Settings.h"

PadMappingDialog::PadMappingDialog(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("Assign Controllers"));

  CreateWidgets();
  ConnectWidgets();
}

int PadMappingDialog::exec()
{
  auto client = Settings::Instance().GetNetPlayClient();
  auto server = Settings::Instance().GetNetPlayServer();

  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
  m_gba_config = server->GetGBAConfig();
  m_wii_mapping = server->GetWiimoteMapping();

  QStringList players;

  players.append(tr("None"));

  for (const auto& player : m_players)
  {
    players.append(
        QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid));
  }

  for (size_t i = 0; i < 4; i++)
  {
    const QSignalBlocker blocker1(m_gc_boxes_player1[i]);
    const QSignalBlocker blocker2(m_gc_boxes_player2[i]);
    const QSignalBlocker blocker3(m_wii_boxes_player1[i]);
    const QSignalBlocker blocker4(m_wii_boxes_player2[i]);

    m_gc_boxes_player1[i]->clear();
    m_gc_boxes_player2[i]->clear();
    m_wii_boxes_player1[i]->clear();
    m_wii_boxes_player2[i]->clear();

    m_gc_boxes_player1[i]->addItems(players);
    m_gc_boxes_player2[i]->addItems(players);
    m_wii_boxes_player1[i]->addItems(players);
    m_wii_boxes_player2[i]->addItems(players);

    m_gc_boxes_player1[i]->setCurrentIndex(m_pad_mapping[i].first);
    m_gc_boxes_player2[i]->setCurrentIndex(m_pad_mapping[i].second);
    m_wii_boxes_player1[i]->setCurrentIndex(m_wii_mapping[i].first);
    m_wii_boxes_player2[i]->setCurrentIndex(m_wii_mapping[i].second);
  }

  for (size_t i = 0; i < m_gba_boxes.size(); i++)
  {
    const QSignalBlocker blocker(m_gba_boxes[i]);
    m_gba_boxes[i]->setChecked(m_gba_config[i].enabled);
  }

  return QDialog::exec();
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  for (const auto& combo_group : {m_gc_boxes_player1, m_gc_boxes_player2, m_wii_boxes_player1, m_wii_boxes_player2})
  {
    for (const auto& combo : combo_group)
    {
      connect(combo, &QComboBox::currentIndexChanged, this, &PadMappingDialog::OnMappingChanged);
    }
  }
  for (const auto& checkbox : m_gba_boxes)
  {
    connect(checkbox, &QCheckBox::stateChanged, this, &PadMappingDialog::OnMappingChanged);
  }
}

int PadMappingDialog::exec()
{
  auto client = Settings::Instance().GetNetPlayClient();
  auto server = Settings::Instance().GetNetPlayServer();

  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
  m_gba_config = server->GetGBAConfig();
  m_wii_mapping = server->GetWiimoteMapping();

  QStringList players;

  players.append(tr("None"));

  for (const auto& player : m_players)
  {
    players.append(
        QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid));
  }

  for (auto& combo_group : {m_gc_boxes, m_wii_boxes})
  {
    bool gc = combo_group == m_gc_boxes;
    for (size_t i = 0; i < combo_group.size(); i++)
    {
      auto& combo = combo_group[i];
      const QSignalBlocker blocker(combo);

      combo->clear();
      combo->addItems(players);

      const auto index = gc ? m_pad_mapping[i] : m_wii_mapping[i];

      combo->setCurrentIndex(index);
    }
  }

  for (size_t i = 0; i < m_gba_boxes.size(); i++)
  {
    const QSignalBlocker blocker(m_gba_boxes[i]);

    m_gba_boxes[i]->setChecked(m_gba_config[i].enabled);
  }

  return QDialog::exec();
}

NetPlay::PadMappingArray PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

NetPlay::GBAConfigArray PadMappingDialog::GetGBAArray()
{
  return m_gba_config;
}

NetPlay::PadMappingArray PadMappingDialog::GetWiimoteArray()
{
  return m_wii_mapping;
}

void PadMappingDialog::OnMappingChanged()
{
  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    int gc_id1 = m_gc_boxes_player1[i]->currentIndex();
    int gc_id2 = m_gc_boxes_player2[i]->currentIndex();
    int wii_id1 = m_wii_boxes_player1[i]->currentIndex();
    int wii_id2 = m_wii_boxes_player2[i]->currentIndex();

    m_pad_mapping[i] = {
      gc_id1 > 0 ? m_players[gc_id1 - 1]->pid : 0,
      gc_id2 > 0 ? m_players[gc_id2 - 1]->pid : 0
    };
    m_wii_mapping[i] = {
      wii_id1 > 0 ? m_players[wii_id1 - 1]->pid : 0,
      wii_id2 > 0 ? m_players[wii_id2 - 1]->pid : 0
    };  
  }
}

// Update getter functions to return the new mapping format
std::array<std::pair<u32, u32>, 4> PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

std::array<std::pair<u32, u32>, 4> PadMappingDialog::GetWiimoteArray()
{
  return m_wii_mapping;
}