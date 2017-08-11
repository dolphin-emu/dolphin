// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings/InterfacePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"

#include "DolphinQt2/Settings.h"

InterfacePane::InterfacePane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  ConnectLayout();
  LoadConfig();
}

void InterfacePane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateUI();
  CreateInGame();

  m_main_layout->setContentsMargins(0, 0, 0, 0);
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void InterfacePane::CreateUI()
{
  auto* groupbox = new QGroupBox(tr("User Interface"));
  auto* groupbox_layout = new QVBoxLayout;
  groupbox->setLayout(groupbox_layout);
  m_main_layout->addWidget(groupbox);

  auto* combobox_layout = new QFormLayout;
  groupbox_layout->addLayout(combobox_layout);

  m_combobox_language = new QComboBox;
  // TODO: Support more languages other then English
  m_combobox_language->addItem(tr("English"));
  combobox_layout->addRow(tr("&Language:"), m_combobox_language);

  // Theme Combobox
  m_combobox_theme = new QComboBox;
  combobox_layout->addRow(tr("&Theme:"), m_combobox_theme);

  // List avalable themes
  auto file_search_results =
      Common::DoFileSearch({File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR});
  for (const std::string& filename : file_search_results)
  {
    std::string name, ext;
    SplitPath(filename, nullptr, &name, &ext);
    name += ext;
    QString qt_name = QString::fromStdString(name);
    m_combobox_theme->addItem(qt_name);
  }

  // Checkboxes
  m_checkbox_auto_window = new QCheckBox(tr("Auto-Adjust Window Size"));
  m_checkbox_top_window = new QCheckBox(tr("Keep Window on Top"));
  m_checkbox_render_to_window = new QCheckBox(tr("Render to Main Window"));
  m_checkbox_use_builtin_title_database = new QCheckBox(tr("Use Built-In Database of Game Names"));
  groupbox_layout->addWidget(m_checkbox_auto_window);
  groupbox_layout->addWidget(m_checkbox_top_window);
  groupbox_layout->addWidget(m_checkbox_render_to_window);
  groupbox_layout->addWidget(m_checkbox_use_builtin_title_database);
}

void InterfacePane::CreateInGame()
{
  auto* groupbox = new QGroupBox(tr("In Game"));
  auto* groupbox_layout = new QVBoxLayout;
  groupbox->setLayout(groupbox_layout);
  m_main_layout->addWidget(groupbox);

  m_checkbox_confirm_on_stop = new QCheckBox(tr("Confirm on Stop"));
  m_checkbox_use_panic_handlers = new QCheckBox(tr("Use Panic Handlers"));
  m_checkbox_enable_osd = new QCheckBox(tr("Show On-Screen Messages"));
  m_checkbox_show_active_title = new QCheckBox(tr("Show Active Title in Window Title"));
  m_checkbox_pause_on_focus_lost = new QCheckBox(tr("Pause on Focus Loss"));
  m_checkbox_hide_mouse = new QCheckBox(tr("Always Hide Mouse Cursor"));

  groupbox_layout->addWidget(m_checkbox_confirm_on_stop);
  groupbox_layout->addWidget(m_checkbox_use_panic_handlers);
  groupbox_layout->addWidget(m_checkbox_enable_osd);
  groupbox_layout->addWidget(m_checkbox_show_active_title);
  groupbox_layout->addWidget(m_checkbox_pause_on_focus_lost);
  groupbox_layout->addWidget(m_checkbox_hide_mouse);
}

void InterfacePane::ConnectLayout()
{
  connect(m_checkbox_auto_window, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_top_window, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_render_to_window, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_use_builtin_title_database, &QCheckBox::clicked, this,
          &InterfacePane::OnSaveConfig);
  connect(m_combobox_theme, static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::activated),
          &Settings::Instance(), &Settings::SetThemeName);
  connect(m_combobox_language, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
          [this](int index) { OnSaveConfig(); });
  connect(m_checkbox_confirm_on_stop, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_use_panic_handlers, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_enable_osd, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_pause_on_focus_lost, &QCheckBox::clicked, this, &InterfacePane::OnSaveConfig);
  connect(m_checkbox_hide_mouse, &QCheckBox::clicked, &Settings::Instance(),
          &Settings::SetHideCursor);
}

void InterfacePane::LoadConfig()
{
  const SConfig& startup_params = SConfig::GetInstance();
  m_checkbox_auto_window->setChecked(startup_params.bRenderWindowAutoSize);
  m_checkbox_top_window->setChecked(startup_params.bKeepWindowOnTop);
  m_checkbox_render_to_window->setChecked(startup_params.bRenderToMain);
  m_checkbox_use_builtin_title_database->setChecked(startup_params.m_use_builtin_title_database);
  m_combobox_theme->setCurrentIndex(
      m_combobox_theme->findText(QString::fromStdString(SConfig::GetInstance().theme_name)));

  // In Game Options
  m_checkbox_confirm_on_stop->setChecked(startup_params.bConfirmStop);
  m_checkbox_use_panic_handlers->setChecked(startup_params.bUsePanicHandlers);
  m_checkbox_enable_osd->setChecked(startup_params.bOnScreenDisplayMessages);
  m_checkbox_show_active_title->setChecked(startup_params.m_show_active_title);
  m_checkbox_pause_on_focus_lost->setChecked(startup_params.m_PauseOnFocusLost);
  m_checkbox_hide_mouse->setChecked(Settings::Instance().GetHideCursor());
}

void InterfacePane::OnSaveConfig()
{
  SConfig& settings = SConfig::GetInstance();
  settings.bRenderWindowAutoSize = m_checkbox_auto_window->isChecked();
  settings.bKeepWindowOnTop = m_checkbox_top_window->isChecked();
  settings.bRenderToMain = m_checkbox_render_to_window->isChecked();
  settings.m_use_builtin_title_database = m_checkbox_use_builtin_title_database->isChecked();

  // In Game Options
  settings.bConfirmStop = m_checkbox_confirm_on_stop->isChecked();
  settings.bUsePanicHandlers = m_checkbox_use_panic_handlers->isChecked();
  settings.bOnScreenDisplayMessages = m_checkbox_enable_osd->isChecked();
  settings.m_show_active_title = m_checkbox_show_active_title->isChecked();
  settings.m_PauseOnFocusLost = m_checkbox_pause_on_focus_lost->isChecked();

  settings.SaveSettings();
}
