// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/Keyboard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "Common/Keyboard.h"
#include "Core/Config/MainSettings.h"
#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Settings.h"

KeyboardWindow::KeyboardWindow(QWidget* parent) : QWidget(parent)
{
  // i18n: Window for managing the Wii keyboard emulation
  setWindowTitle(tr("Keyboard Manager"));
  setObjectName(QStringLiteral("keyboard_manager"));
  setMinimumSize(QSize(500, 200));

  auto* main_layout = new QVBoxLayout();

  {
    auto* group = new QGroupBox();
    auto* hbox_layout = new QHBoxLayout();
    hbox_layout->setAlignment(Qt::AlignHCenter);
    auto* checkbox_emulate = new QCheckBox(tr("Emulate USB keyboard"), this);
    checkbox_emulate->setChecked(Settings::Instance().IsUSBKeyboardConnected());
    connect(checkbox_emulate, &QCheckBox::toggled, this, &KeyboardWindow::EmulateKeyboard);
    connect(&Settings::Instance(), &Settings::USBKeyboardConnectionChanged, checkbox_emulate,
            &QCheckBox::setChecked);
    hbox_layout->addWidget(checkbox_emulate);
    group->setLayout(hbox_layout);

    main_layout->addWidget(group);
  }

  {
    auto* group = new QGroupBox(tr("Layout Configuration"));
    auto* grid_layout = new QGridLayout();
    auto* checkbox_translate =
        new ConfigBool(tr("Enable partial translation"), Config::MAIN_WII_KEYBOARD_TRANSLATION);
    grid_layout->addWidget(checkbox_translate, 0, 0, 1, 2, Qt::AlignLeft);

    auto create_combo = [checkbox_translate, grid_layout](int row, const QString& name,
                                                          const auto& config_info) {
      grid_layout->addWidget(new QLabel(name), row, 0);
      auto* combo = new QComboBox();

      combo->addItem(tr("Automatic detection"), Common::KeyboardLayout::AUTO);
      combo->addItem(QStringLiteral("QWERTY"), Common::KeyboardLayout::QWERTY);
      combo->addItem(QStringLiteral("AZERTY"), Common::KeyboardLayout::AZERTY);
      combo->addItem(QStringLiteral("QWERTZ"), Common::KeyboardLayout::QWERTZ);
      combo->setCurrentIndex(combo->findData(Config::Get(config_info)));
      combo->setEnabled(checkbox_translate->isChecked());

      connect(combo, &QComboBox::currentIndexChanged, combo, [combo, config_info] {
        const auto keyboard_layout = combo->currentData();
        if (!keyboard_layout.isValid())
          return;

        Config::SetBaseOrCurrent(config_info, keyboard_layout.toInt());
        Common::KeyboardContext::UpdateLayout();
      });
      connect(checkbox_translate, &QCheckBox::toggled, combo, &QComboBox::setEnabled);

      grid_layout->addWidget(combo, row, 1);
    };

    create_combo(1, tr("Host layout:"), Config::MAIN_WII_KEYBOARD_HOST_LAYOUT);
    create_combo(2, tr("Game layout:"), Config::MAIN_WII_KEYBOARD_GAME_LAYOUT);
    group->setLayout(grid_layout);

    main_layout->addWidget(group);
  }

  setLayout(main_layout);
}

KeyboardWindow::~KeyboardWindow() = default;

void KeyboardWindow::EmulateKeyboard(bool emulate) const
{
  Settings::Instance().SetUSBKeyboardConnected(emulate);
}
