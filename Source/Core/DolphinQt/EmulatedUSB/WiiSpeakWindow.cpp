// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/WiiSpeakWindow.h"

#include <string>

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>

#include "Common/IOFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/Microphone.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Settings.h"

WiiSpeakWindow::WiiSpeakWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Wii Speak Manager"));
  setObjectName(QStringLiteral("wii_speak_manager"));
  setMinimumSize(QSize(700, 200));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &WiiSpeakWindow::OnEmulationStateChanged);

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
};

WiiSpeakWindow::~WiiSpeakWindow() = default;

void WiiSpeakWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignLeft);
  m_checkbox = new QCheckBox(tr("Emulate Wii Speak"), this);
  m_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_WII_SPEAK));
  connect(m_checkbox, &QCheckBox::toggled, this, &WiiSpeakWindow::EmulateWiiSpeak);
  checkbox_layout->addWidget(m_checkbox);
  checkbox_group->setLayout(checkbox_layout);

  m_combobox_microphones = new QComboBox();
  for (const std::string& device : IOS::HLE::USB::Microphone::ListDevices())
  {
    m_combobox_microphones->addItem(QString::fromStdString(device));
  }

  checkbox_layout->addWidget(m_combobox_microphones);

  main_layout->addWidget(checkbox_group);
  setLayout(main_layout);
}

void WiiSpeakWindow::EmulateWiiSpeak(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_WII_SPEAK, emulate);
}

void WiiSpeakWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox->setEnabled(!running);
}
