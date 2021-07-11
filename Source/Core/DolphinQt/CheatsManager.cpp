// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatsManager.h"

#include <functional>

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Core/ActionReplay.h"
#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "UICommon/GameFile.h"

#include "DolphinQt/CheatSearchNewWidget.h"
#include "DolphinQt/CheatSearchWidget.h"
#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/Settings.h"

CheatsManager::CheatsManager(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Cheats Manager"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &CheatsManager::OnStateChanged);

  OnStateChanged(Core::GetState());

  CreateWidgets();
  ConnectWidgets();
}

CheatsManager::~CheatsManager() = default;

void CheatsManager::OnStateChanged(Core::State state)
{
  if (state != Core::State::Running && state != Core::State::Paused)
    return;

  const auto& game_id = SConfig::GetInstance().GetGameID();
  const auto& game_tdb_id = SConfig::GetInstance().GetGameTDBID();
  const u16 revision = SConfig::GetInstance().GetRevision();

  if (m_game_id == game_id && m_game_tdb_id == game_tdb_id && m_revision == revision)
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
}

void CheatsManager::CreateWidgets()
{
  m_tab_widget = new QTabWidget;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_cheat_search_new =
      new CheatSearchNewWidget([this](std::vector<Cheats::MemoryRange> memory_ranges,
                                      PowerPC::RequestedAddressSpace address_space,
                                      Cheats::DataType data_type, bool data_aligned) {
        auto* w = new CheatSearchWidget(memory_ranges, address_space, data_type, data_aligned,
                                        [&](ActionReplay::ARCode ar_code) {
                                          if (m_ar_code)
                                            m_ar_code->AddCode(std::move(ar_code));
                                        });
        const int tab_index = m_tab_widget->addTab(w, tr("Cheat Search"));
        w->connect(w, &QObject::destroyed, this, [this, w] {
          const int inner_tab_index = m_tab_widget->indexOf(w);
          if (inner_tab_index != -1)
            m_tab_widget->removeTab(inner_tab_index);
        });
        m_tab_widget->setCurrentIndex(tab_index);
      });
  m_tab_widget->addTab(m_cheat_search_new, tr("Start New Cheat Search"));

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void CheatsManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
