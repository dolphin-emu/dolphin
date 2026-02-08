// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatsManager.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "DolphinQt/CheatSearchFactoryWidget.h"
#include "DolphinQt/CheatSearchWidget.h"
#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/QtUtils/PartiallyClosableTabWidget.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoEvents.h"

CheatsManager::CheatsManager(Core::System& system, QWidget* parent)
    : QDialog(parent), m_system(system)
{
  setWindowTitle(tr("Cheats Manager"));

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &CheatsManager::OnStateChanged);

  CreateWidgets();
  ConnectWidgets();

  QtUtils::AdjustSizeWithinScreen(this);

  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("cheatsmanager/geometry")).toByteArray());
}

CheatsManager::~CheatsManager()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("cheatsmanager/geometry"), saveGeometry());
}

void CheatsManager::OnStateChanged(Core::State state)
{
  RefreshCodeTabs(state);
  if (state == Core::State::Paused)
    UpdateAllCheatSearchWidgetCurrentValues();
}

void CheatsManager::OnFrameEnd()
{
  if (!isVisible())
    return;

  auto* const selected_cheat_search_widget =
      qobject_cast<CheatSearchWidget*>(m_tab_widget->currentWidget());
  if (selected_cheat_search_widget != nullptr)
  {
    selected_cheat_search_widget->UpdateTableVisibleCurrentValues(
        CheatSearchWidget::UpdateSource::Auto);
  }
}

void CheatsManager::UpdateAllCheatSearchWidgetCurrentValues()
{
  for (int i = 0; i < m_tab_widget->count(); ++i)
  {
    auto* const cheat_search_widget = qobject_cast<CheatSearchWidget*>(m_tab_widget->widget(i));
    if (cheat_search_widget != nullptr)
      cheat_search_widget->UpdateTableAllCurrentValues(CheatSearchWidget::UpdateSource::Auto);
  }
}

void CheatsManager::RegisterAfterFrameEventCallback()
{
  m_VI_end_field_event =
      m_system.GetVideoEvents().vi_end_field_event.Register([this] { OnFrameEnd(); });
}

void CheatsManager::RemoveAfterFrameEventCallback()
{
  m_VI_end_field_event.reset();
}

void CheatsManager::hideEvent(QHideEvent* event)
{
  RemoveAfterFrameEventCallback();
}

void CheatsManager::showEvent(QShowEvent* event)
{
  RegisterAfterFrameEventCallback();
}

void CheatsManager::RefreshCodeTabs(Core::State state)
{
  if (state == Core::State::Starting || state == Core::State::Stopping)
    return;

  const auto& game_id = state == Core::State::Running || state == Core::State::Paused ?
                            SConfig::GetInstance().GetGameID() :
                            std::string();
  const auto& game_tdb_id = SConfig::GetInstance().GetGameTDBID();
  const u16 revision = SConfig::GetInstance().GetRevision();

  if (m_game_id == game_id && m_game_tdb_id == game_tdb_id && m_revision == revision)
    return;

  m_game_id = game_id;
  m_game_tdb_id = game_tdb_id;
  m_revision = revision;

  m_ar_code->ChangeGame(m_game_id, m_revision);
  m_gecko_code->ChangeGame(m_game_id, m_game_tdb_id, m_revision);
}

void CheatsManager::CreateWidgets()
{
  m_tab_widget = new PartiallyClosableTabWidget;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  int tab_index = 0;

  m_ar_code = new ARCodeWidget(m_game_id, m_revision, false);
  tab_index = m_tab_widget->addTab(GetWrappedWidget(m_ar_code), tr("AR Code"));
  m_tab_widget->setTabUnclosable(tab_index);

  m_gecko_code = new GeckoCodeWidget(m_game_id, m_game_tdb_id, m_revision, false);
  tab_index = m_tab_widget->addTab(GetWrappedWidget(m_gecko_code), tr("Gecko Codes"));
  m_tab_widget->setTabUnclosable(tab_index);

  m_cheat_search_new = new CheatSearchFactoryWidget();
  tab_index =
      m_tab_widget->addTab(GetWrappedWidget(m_cheat_search_new), tr("Start New Cheat Search"));
  m_tab_widget->setTabUnclosable(tab_index);

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void CheatsManager::OnNewSessionCreated(const Cheats::CheatSearchSessionBase& session)
{
  auto* w = new CheatSearchWidget(m_system, session.Clone());
  const int tab_index = m_tab_widget->addTab(w, tr("Cheat Search"));
  connect(w, &CheatSearchWidget::ActionReplayCodeGenerated, m_ar_code, &ARCodeWidget::AddCode);
  connect(w, &CheatSearchWidget::ShowMemory, this, &CheatsManager::ShowMemory);
  connect(w, &CheatSearchWidget::RequestWatch, this, &CheatsManager::RequestWatch);
  m_tab_widget->setCurrentIndex(tab_index);
}

void CheatsManager::OnTabCloseRequested(int index)
{
  auto* w = m_tab_widget->widget(index);
  if (w)
    w->deleteLater();
}

void CheatsManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_cheat_search_new, &CheatSearchFactoryWidget::NewSessionCreated, this,
          &CheatsManager::OnNewSessionCreated);
  connect(m_tab_widget, &QTabWidget::tabCloseRequested, this, &CheatsManager::OnTabCloseRequested);
  connect(m_ar_code, &ARCodeWidget::OpenGeneralSettings, this, &CheatsManager::OpenGeneralSettings);
  connect(m_gecko_code, &GeckoCodeWidget::OpenGeneralSettings, this,
          &CheatsManager::OpenGeneralSettings);
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_ar_code, &ARCodeWidget::OpenAchievementSettings, this,
          &CheatsManager::OpenAchievementSettings);
  connect(m_gecko_code, &GeckoCodeWidget::OpenAchievementSettings, this,
          &CheatsManager::OpenAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS
}
