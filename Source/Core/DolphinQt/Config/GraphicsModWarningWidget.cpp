// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GraphicsModWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

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
  auto* icon = new QLabel;

  const auto size = 1.5 * QFontMetrics(font()).height();

  QPixmap warning_icon = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size);

  icon->setPixmap(warning_icon);

  m_text = new QLabel();
  m_config_button = new QPushButton(tr("Configure Dolphin"));

  m_config_button->setHidden(true);

  auto* layout = new QHBoxLayout;

  layout->addWidget(icon);
  layout->addWidget(m_text, 1);
  layout->addWidget(m_config_button);

  layout->setContentsMargins(0, 0, 0, 0);

  setLayout(layout);
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
