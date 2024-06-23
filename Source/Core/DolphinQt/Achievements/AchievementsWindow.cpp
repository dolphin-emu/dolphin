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
      [this](AchievementManager::UpdatedItems updated_items) {
        QueueOnObject(this, [this, updated_items = std::move(updated_items)] {
          AchievementsWindow::UpdateData(std::move(updated_items));
        });
      });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this] { m_settings_widget->UpdateData(); });
  connect(&Settings::Instance(), &Settings::HardcoreStateChanged, this,
          [this] { AchievementsWindow::UpdateData({.all = true}); });
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

void AchievementsWindow::UpdateData(AchievementManager::UpdatedItems updated_items)
{
  m_settings_widget->UpdateData();
  if (updated_items.all)
  {
    m_header_widget->UpdateData();
    m_progress_widget->UpdateData(true);
    m_leaderboard_widget->UpdateData(true);
  }
  else
  {
    if (updated_items.player_icon || updated_items.game_icon || updated_items.rich_presence ||
        updated_items.all_achievements || updated_items.achievements.size() > 0)
    {
      m_header_widget->UpdateData();
    }
    if (updated_items.all_achievements)
      m_progress_widget->UpdateData(false);
    else if (updated_items.achievements.size() > 0)
      m_progress_widget->UpdateData(updated_items.achievements);
    if (updated_items.all_leaderboards)
      m_leaderboard_widget->UpdateData(false);
    else if (updated_items.leaderboards.size() > 0)
      m_leaderboard_widget->UpdateData(updated_items.leaderboards);
  }

  {
    auto& instance = AchievementManager::GetInstance();
    std::lock_guard lg{instance.GetLock()};
    const bool is_game_loaded = instance.IsGameLoaded();
    m_header_widget->setVisible(instance.HasAPIToken());
    m_tab_widget->setTabVisible(1, is_game_loaded);
    m_tab_widget->setTabVisible(2, is_game_loaded);
  }
  update();
}

void AchievementsWindow::ForceSettingsTab()
{
  m_tab_widget->setCurrentIndex(0);
}

#endif  // USE_RETRO_ACHIEVEMENTS
