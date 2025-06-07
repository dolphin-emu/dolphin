// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/FreeLookRotation.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "Core/FreeLookManager.h"
#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "InputCommon/InputConfig.h"

FreeLookRotation::FreeLookRotation(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void FreeLookRotation::CreateMainLayout()
{
  m_main_layout = new QGridLayout;

  auto* alternate_input_layout = new QHBoxLayout();
  auto* note_label = new QLabel(
      tr("Note: motion input may require configuring alternate input sources before use."));
  note_label->setWordWrap(true);
  auto* alternate_input_sources_button = new QPushButton(tr("Alternate Input Sources"));
  alternate_input_layout->addWidget(note_label, 1);
  alternate_input_layout->addWidget(alternate_input_sources_button, 0, Qt::AlignRight);
  connect(alternate_input_sources_button, &QPushButton::clicked, this, [this] {
    ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->setWindowModality(Qt::WindowModality::WindowModal);
    window->show();
  });
  m_main_layout->addLayout(alternate_input_layout, 0, 0, 1, -1);

  m_main_layout->addWidget(
      CreateGroupBox(tr("Incremental Rotation (rad/sec)"),
                     FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Rotation)),
      1, 0);

  setLayout(m_main_layout);
}

InputConfig* FreeLookRotation::GetConfig()
{
  return FreeLook::GetInputConfig();
}

void FreeLookRotation::LoadSettings()
{
  FreeLook::LoadInputConfig();
}

void FreeLookRotation::SaveSettings()
{
  FreeLook::GetInputConfig()->SaveConfig();
}
