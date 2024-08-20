// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatsManager.h"

#include <functional>

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "Core/ActionReplay.h"
#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "UICommon/GameFile.h"

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
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &CheatsManager::OnStateChanged);

  CreateWidgets();
  ConnectWidgets();

  RefreshCodeTabs(Core::GetState(m_system), true);

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
  RefreshCodeTabs(state, false);
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
  m_VI_end_field_event = VIEndFieldEvent::Register([this] { OnFrameEnd(); }, "CheatsManager");
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

void CheatsManager::RefreshCodeTabs(Core::State state, bool force)
{
  if (!force && (state == Core::State::Starting || state == Core::State::Stopping))
    return;

  const auto& game_id =
      state != Core::State::Uninitialized ? SConfig::GetInstance().GetGameID() : std::string();
  const auto& game_tdb_id = SConfig::GetInstance().GetGameTDBID();
  const u16 revision = SConfig::GetInstance().GetRevision();

  if (!force && m_game_id == game_id && m_game_tdb_id == game_tdb_id && m_revision == revision)
    return;

  m_game_id = game_id;
  m_game_tdb_id = game_tdb_id;
  m_revision = revision;

  if (m_ar_code)
  {
    const int tab_index = m_tab_widget->indexOf(m_ar_code);
    if (tab_index != -1)
      m_tab_widget->removeTab(tab_index);
    m_ar_code->deleteLater();
    m_ar_code = nullptr;
  }

  if (m_gecko_code)
  {
    const int tab_index = m_tab_widget->indexOf(m_gecko_code);
    if (tab_index != -1)
      m_tab_widget->removeTab(tab_index);
    m_gecko_code->deleteLater();
    m_gecko_code = nullptr;
  }

  m_ar_code = new ARCodeWidget(m_game_id, m_revision, false);
  m_gecko_code = new GeckoCodeWidget(m_game_id, m_game_tdb_id, m_revision, false);
  m_tab_widget->insertTab(0, m_ar_code, tr("AR Code"));
  m_tab_widget->insertTab(1, m_gecko_code, tr("Gecko Codes"));
  m_tab_widget->setTabUnclosable(0);
  m_tab_widget->setTabUnclosable(1);

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

void CheatsManager::CreateWidgets()
{
  m_tab_widget = new PartiallyClosableTabWidget;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_cheat_search_new = new CheatSearchFactoryWidget();
  m_tab_widget->addTab(m_cheat_search_new, tr("Start New Cheat Search"));
  m_tab_widget->setTabUnclosable(0);

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void CheatsManager::OnNewSessionCreated(const Cheats::CheatSearchSessionBase& session)
{
  auto* w = new CheatSearchWidget(m_system, session.Clone());
  const int tab_index = m_tab_widget->addTab(w, tr("Cheat Search"));
  w->connect(w, &CheatSearchWidget::ActionReplayCodeGenerated, this,
             [this](const ActionReplay::ARCode& ar_code) {
               if (m_ar_code)
                 m_ar_code->AddCode(ar_code);
             });
  w->connect(w, &CheatSearchWidget::ShowMemory, [this](u32 address) { emit ShowMemory(address); });
  w->connect(w, &CheatSearchWidget::RequestWatch,
             [this](QString name, u32 address) { emit RequestWatch(name, address); });
  m_tab_widget->setCurrentIndex(tab_index);
}

void CheatsManager::OnTabCloseRequested(int index)
{
  if (auto* w = m_tab_widget->widget(index))
    w->deleteLater();
}

void CheatsManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_cheat_search_new, &CheatSearchFactoryWidget::NewSessionCreated, this,
          &CheatsManager::OnNewSessionCreated);
  connect(m_tab_widget, &QTabWidget::tabCloseRequested, this, &CheatsManager::OnTabCloseRequested);
}
