// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementsWindow.h"

#include <mutex>

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Core/AchievementManager.h"

#include "DolphinQt/Achievements/AchievementHeaderWidget.h"
#include "DolphinQt/Achievements/AchievementLeaderboardWidget.h"
#include "DolphinQt/Achievements/AchievementProgressWidget.h"
#include "DolphinQt/Achievements/AchievementSettingsWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings.h"

AchievementsWindow::AchievementsWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Achievements"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();
  ConnectWidgets();
  AchievementManager::GetInstance().SetUpdateCallback(
      [this] { QueueOnObject(this, &AchievementsWindow::UpdateData); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &AchievementsWindow::UpdateData);

  UpdateData();
}

void AchievementsWindow::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  update();
}

void AchievementsWindow::CreateMainLayout()
{
  const auto is_game_loaded = AchievementManager::GetInstance().IsGameLoaded();

  m_header_widget = new AchievementHeaderWidget(this);
  m_tab_widget = new QTabWidget();
  m_settings_widget = new AchievementSettingsWidget(m_tab_widget);
  m_progress_widget = new AchievementProgressWidget(m_tab_widget);
  m_leaderboard_widget = new AchievementLeaderboardWidget(m_tab_widget);
  m_tab_widget->addTab(GetWrappedWidget(m_settings_widget, this, 125, 100), tr("Settings"));
  m_tab_widget->addTab(GetWrappedWidget(m_progress_widget, this, 125, 100), tr("Progress"));
  m_tab_widget->setTabVisible(1, is_game_loaded);
  m_tab_widget->addTab(GetWrappedWidget(m_leaderboard_widget, this, 125, 100), tr("Leaderboards"));
  m_tab_widget->setTabVisible(2, is_game_loaded);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  auto* layout = new QVBoxLayout();
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
  {
    auto& instance = AchievementManager::GetInstance();
    std::lock_guard lg{instance.GetLock()};
    const bool is_game_loaded = instance.IsGameLoaded();

    m_header_widget->UpdateData();
    m_header_widget->setVisible(instance.IsLoggedIn());
    m_settings_widget->UpdateData();
    m_progress_widget->UpdateData();
    m_tab_widget->setTabVisible(1, is_game_loaded);
    m_leaderboard_widget->UpdateData();
    m_tab_widget->setTabVisible(2, is_game_loaded);
  }
  update();
}

void AchievementsWindow::ForceSettingsTab()
{
  m_tab_widget->setCurrentIndex(0);
}

#endif  // USE_RETRO_ACHIEVEMENTS
