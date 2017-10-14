// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/NetPlay/PadMappingDialog.h"
#include "DolphinQt2/Settings.h"

#include "Core/NetPlayClient.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>

PadMappingDialog::PadMappingDialog(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

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
    m_main_layout->addWidget(new QLabel(tr("Wii Remote %1").arg(i + 1)), 0, 4 + i);
    m_main_layout->addWidget(m_gc_boxes[i], 1, i);
    m_main_layout->addWidget(m_wii_boxes[i], 1, 4 + i);
  }

  m_main_layout->addWidget(m_button_box, 2, 0, 1, -1);

  setLayout(m_main_layout);
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

int PadMappingDialog::exec()
{
  m_players = Settings::Instance().GetNetPlayClient()->GetPlayers();

  QStringList players;

  players.append(tr("None"));

  for (const auto& player : m_players)
    players.append(QString::fromStdString(player->name));

  for (auto& combo_group : {m_gc_boxes, m_wii_boxes})
  {
    for (auto& combo : combo_group)
    {
      combo->clear();
      combo->addItems(players);
      connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
              &PadMappingDialog::OnMappingChanged);
    }
  }

  return QDialog::exec();
}

PadMappingArray PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

PadMappingArray PadMappingDialog::GetWiimoteArray()
{
  return m_wii_mapping;
}

void PadMappingDialog::OnMappingChanged()
{
  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    int gc_id = m_gc_boxes[i]->currentIndex();
    int wii_id = m_wii_boxes[i]->currentIndex();

    m_pad_mapping[i] = gc_id > 0 ? m_players[gc_id - 1]->pid : -1;
    m_wii_mapping[i] = wii_id > 0 ? m_players[wii_id - 1]->pid : -1;
  }
}
