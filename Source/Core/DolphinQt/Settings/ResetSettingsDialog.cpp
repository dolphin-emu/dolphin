// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/ResetSettingsDialog.h"

#include <QCheckBox>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/ConfigManager.h"

ResetSettingsDialog::ResetSettingsDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Reset Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto* reset_settings_button = new QPushButton(tr("Reset Settings"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  cancel_button->setDefault(true);
  auto* checkbox_group_box = new QGroupBox();
  auto* checkbox_layout = new QVBoxLayout();
  checkbox_group_box->setLayout(checkbox_layout);

  m_reset_core = new QCheckBox(tr("Reset Core Emulation Settings"));
  checkbox_layout->addWidget(m_reset_core);
  m_reset_graphics = new QCheckBox(tr("Reset Graphics Settings"));
  checkbox_layout->addWidget(m_reset_graphics);
  m_reset_paths = new QCheckBox(tr("Reset Customized Paths"));
  checkbox_layout->addWidget(m_reset_paths);
  m_reset_gui = new QCheckBox(tr("Reset GUI Settings"));
  checkbox_layout->addWidget(m_reset_gui);
  m_reset_controller = new QCheckBox(tr("Reset Controller Settings"));
  checkbox_layout->addWidget(m_reset_controller);
  m_reset_netplay = new QCheckBox(tr("Reset Netplay Settings"));
  checkbox_layout->addWidget(m_reset_netplay);

  auto* button_layout = new QHBoxLayout();
  button_layout->addStretch();
  button_layout->addWidget(reset_settings_button, 0, Qt::AlignRight);
  button_layout->addWidget(cancel_button, 0, Qt::AlignRight);

  auto* layout = new QVBoxLayout();
  layout->addWidget(checkbox_group_box);
  layout->addLayout(button_layout);
  setLayout(layout);

  connect(reset_settings_button, &QPushButton::clicked, this, &ResetSettingsDialog::ResetSelected);
  connect(cancel_button, &QPushButton::clicked, this, &ResetSettingsDialog::close);
}

void ResetSettingsDialog::ResetSelected()
{
  auto to_reset = ResetSettingsEnum::None;

  if (m_reset_core->isChecked())
    to_reset |= ResetSettingsEnum::Core;
  if (m_reset_graphics->isChecked())
    to_reset |= ResetSettingsEnum::Graphics;
  if (m_reset_paths->isChecked())
    to_reset |= ResetSettingsEnum::Paths;
  if (m_reset_gui->isChecked())
    to_reset |= ResetSettingsEnum::GUI;
  if (m_reset_controller->isChecked())
    to_reset |= ResetSettingsEnum::Controller;
  if (m_reset_netplay->isChecked())
    to_reset |= ResetSettingsEnum::Netplay;

  SConfig::ResetSettings(to_reset);
  close();
}
