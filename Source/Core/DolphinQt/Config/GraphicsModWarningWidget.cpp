// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GraphicsModWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/Settings.h"

GraphicsModWarningWidget::GraphicsModWarningWidget(QWidget* parent) : QWidget(parent)
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EnableGfxModsChanged, this,
          &GraphicsModWarningWidget::Update);
  Update();
}

void GraphicsModWarningWidget::CreateWidgets()
{
  m_text = new QLabel();

  m_config_button = new QPushButton(tr("Configure Dolphin"));
  m_config_button->setHidden(true);

  auto* const layout = new QHBoxLayout{this};

  layout->addWidget(QtUtils::CreateIconWarning(this, QStyle::SP_MessageBoxWarning, m_text));
  layout->addWidget(m_config_button);

  layout->setContentsMargins(0, 0, 0, 0);
}

void GraphicsModWarningWidget::Update()
{
  bool hide_widget = true;
  bool hide_config_button = true;

  if (!Settings::Instance().GetGraphicModsEnabled())
  {
    hide_widget = false;
    hide_config_button = false;
    m_text->setText(tr("Graphics mods are currently disabled."));
  }

  setHidden(hide_widget);
  m_config_button->setHidden(hide_config_button);
}

void GraphicsModWarningWidget::ConnectWidgets()
{
  connect(m_config_button, &QPushButton::clicked, this,
          &GraphicsModWarningWidget::GraphicsModEnableSettings);
}
