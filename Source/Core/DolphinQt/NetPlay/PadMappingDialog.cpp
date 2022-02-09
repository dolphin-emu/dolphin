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

void PadMappingDialog::CreateWidgets()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  for (unsigned int i = 0; i < m_gc_boxes.size(); i++)
  {
    m_gc_boxes[i] = new QComboBox;

    m_main_layout->addWidget(new QLabel(tr("GC Port %1").arg(i + 1)), 0, i);
    m_main_layout->addWidget(m_gc_boxes[i], 1, i);
  }

  m_main_layout->addWidget(m_button_box, 2, 0, 1, -1);

  setLayout(m_main_layout);
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  for (const auto& combo : m_gc_boxes)
    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &PadMappingDialog::OnMappingChanged);
}

int PadMappingDialog::exec()
{
  auto client = Settings::Instance().GetNetPlayClient();
  auto server = Settings::Instance().GetNetPlayServer();
  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();

  QStringList players;

  players.append(tr("None"));

  for (const auto& player : m_players)
  {
    players.append(
        QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid));
  }

  for (size_t i = 0; i < m_gc_boxes.size(); i++)
  {
    auto& combo = m_gc_boxes[i];
    const QSignalBlocker blocker(combo);

    combo->clear();
    combo->addItems(players);

    const auto index = m_pad_mapping[i];

    combo->setCurrentIndex(index);
  }

  return QDialog::exec();
}

NetPlay::PadMappingArray PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

void PadMappingDialog::OnMappingChanged()
{
  for (unsigned int i = 0; i < m_gc_boxes.size(); i++)
  {
    int gc_id = m_gc_boxes[i]->currentIndex();

    m_pad_mapping[i] = gc_id > 0 ? m_players[gc_id - 1]->pid : 0;
  }
}
