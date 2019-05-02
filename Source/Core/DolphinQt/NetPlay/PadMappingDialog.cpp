// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/NetPlay/PadMappingDialog.h"

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

void PadMappingDialog::CreateWidgets()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    m_gc_boxes[i] = new QComboBox;
    m_wii_boxes[i] = new QComboBox;

    m_main_layout->addWidget(new QLabel(tr("GC Port %1").arg(i + 1)), 0, i);
    m_main_layout->addWidget(m_gc_boxes[i], 1, i);
    m_main_layout->addWidget(new QLabel(tr("Wii Remote %1").arg(i + 1)), 2, i);
    m_main_layout->addWidget(m_wii_boxes[i], 3, i);
  }

  m_main_layout->addWidget(m_button_box, 4, 0, 1, -1);

  setLayout(m_main_layout);
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  for (auto& combo_group : {m_gc_boxes, m_wii_boxes})
  {
    for (auto& combo : combo_group)
    {
      connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
              &PadMappingDialog::OnMappingChanged);
    }
  }
}

int PadMappingDialog::exec()
{
  auto client = Settings::Instance().GetNetPlayClient();
  auto server = Settings::Instance().GetNetPlayServer();
  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
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

  return QDialog::exec();
}

NetPlay::PadMappingArray PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

NetPlay::PadMappingArray PadMappingDialog::GetWiimoteArray()
{
  return m_wii_mapping;
}

void PadMappingDialog::OnMappingChanged()
{
  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    int gc_id = m_gc_boxes[i]->currentIndex();
    int wii_id = m_wii_boxes[i]->currentIndex();

    m_pad_mapping[i] = gc_id > 0 ? m_players[gc_id - 1]->pid : 0;
    m_wii_mapping[i] = wii_id > 0 ? m_players[wii_id - 1]->pid : 0;
  }
}
