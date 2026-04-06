// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Config/HardcoreWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "Core/AchievementManager.h"

#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/Settings.h"

HardcoreWarningWidget::HardcoreWarningWidget(QWidget* parent) : QWidget(parent)
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] { Update(); });

  Update();
}

void HardcoreWarningWidget::CreateWidgets()
{
  auto* const text = new QLabel(tr("Only approved codes will be applied in hardcore mode."));
  auto* const icon_warning = QtUtils::CreateIconWarning(this, QStyle::SP_MessageBoxWarning, text);

  m_settings_button = new QPushButton(tr("Achievement Settings"));

  auto* const layout = new QHBoxLayout{this};

  layout->addWidget(icon_warning, 1);
  layout->addWidget(m_settings_button);

  layout->setContentsMargins(0, 0, 0, 0);
}

void HardcoreWarningWidget::ConnectWidgets()
{
  connect(m_settings_button, &QPushButton::clicked, this,
          &HardcoreWarningWidget::OpenAchievementSettings);
}

void HardcoreWarningWidget::Update()
{
  setHidden(!AchievementManager::GetInstance().IsHardcoreModeActive());
}
#endif  // USE_RETRO_ACHIEVEMENTS
