// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementsWindow.h"

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "DolphinQt/Achievements/AchievementHeaderWidget.h"
#include "DolphinQt/Achievements/AchievementSettingsWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

AchievementsWindow::AchievementsWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Achievements"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();
  ConnectWidgets();
}

void AchievementsWindow::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  update();
}

void AchievementsWindow::CreateMainLayout()
{
  auto* layout = new QVBoxLayout();

  m_header_widget = new AchievementHeaderWidget(this);
  m_tab_widget = new QTabWidget();
  m_tab_widget->addTab(
      GetWrappedWidget(new AchievementSettingsWidget(m_tab_widget, this), this, 125, 100),
      tr("Settings"));

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  layout->addWidget(m_header_widget);
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  WrapInScrollArea(this, layout);
}

void AchievementsWindow::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AchievementsWindow::UpdateData()
{
  m_header_widget->UpdateData();
  m_header_widget->setVisible(AchievementManager::GetInstance()->IsLoggedIn());
  update();
}

#endif  // USE_RETRO_ACHIEVEMENTS
