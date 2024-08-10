// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/LogConfigWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"

#include "DolphinQt/Settings.h"

LogConfigWidget::LogConfigWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Log Configuration"));
  setObjectName(QStringLiteral("logconfig"));

  setHidden(!Settings::Instance().IsLogConfigVisible());
  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
}

LogConfigWidget::~LogConfigWidget()
{
  SaveSettings();
}

void LogConfigWidget::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  auto* verbosity = new QGroupBox(tr("Verbosity"));
  auto* verbosity_layout = new QVBoxLayout;
  verbosity->setLayout(verbosity_layout);
  m_verbosity_notice = new QRadioButton(tr("Notice"));
  m_verbosity_error = new QRadioButton(tr("Error"));
  m_verbosity_warning = new QRadioButton(tr("Warning"));
  m_verbosity_info = new QRadioButton(tr("Info"));
  m_verbosity_debug = new QRadioButton(tr("Debug"));
  m_verbosity_debug->setVisible(Common::Log::MAX_LOGLEVEL == Common::Log::LogLevel::LDEBUG);

  auto* outputs = new QGroupBox(tr("Logger Outputs"));
  auto* outputs_layout = new QVBoxLayout;
  outputs->setLayout(outputs_layout);
  m_out_file = new QCheckBox(tr("Write to File"));
  m_out_console = new QCheckBox(tr("Write to Console"));
  m_out_window = new QCheckBox(tr("Write to Window"));

  auto* types = new QGroupBox(tr("Log Types"));
  auto* types_layout = new QVBoxLayout;
  types->setLayout(types_layout);
  m_types_toggle = new QPushButton(tr("Toggle All Log Types"));
  m_types_list = new QListWidget;

  const auto* const log_manager = Common::Log::LogManager::GetInstance();
  for (int i = 0; i < static_cast<int>(Common::Log::LogType::NUMBER_OF_LOGS); i++)
  {
    const auto log_type = static_cast<Common::Log::LogType>(i);
    const QString full_name = QString::fromUtf8(log_manager->GetFullName(log_type));
    const QString short_name = QString::fromUtf8(log_manager->GetShortName(log_type));
    auto* widget = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(full_name, short_name));
    widget->setCheckState(Qt::Unchecked);
    m_types_list->addItem(widget);
  }

  layout->addWidget(verbosity);
  verbosity_layout->addWidget(m_verbosity_notice);
  verbosity_layout->addWidget(m_verbosity_error);
  verbosity_layout->addWidget(m_verbosity_warning);
  verbosity_layout->addWidget(m_verbosity_info);
  verbosity_layout->addWidget(m_verbosity_debug);

  layout->addWidget(outputs);
  outputs_layout->addWidget(m_out_file);
  outputs_layout->addWidget(m_out_console);
  outputs_layout->addWidget(m_out_window);

  layout->addWidget(types);
  types_layout->addWidget(m_types_toggle);
  types_layout->addWidget(m_types_list);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void LogConfigWidget::ConnectWidgets()
{
  // Configuration
  connect(m_verbosity_notice, &QRadioButton::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_verbosity_error, &QRadioButton::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_verbosity_warning, &QRadioButton::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_verbosity_info, &QRadioButton::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_verbosity_debug, &QRadioButton::toggled, this, &LogConfigWidget::SaveSettings);

  connect(m_out_file, &QCheckBox::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_out_console, &QCheckBox::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_out_window, &QCheckBox::toggled, this, &LogConfigWidget::SaveSettings);

  connect(m_types_toggle, &QPushButton::clicked, [this] {
    m_all_enabled = !m_all_enabled;

    // Don't save every time we change an item
    m_block_save = true;

    for (int i = 0; i < m_types_list->count(); i++)
      m_types_list->item(i)->setCheckState(m_all_enabled ? Qt::Checked : Qt::Unchecked);

    m_block_save = false;

    SaveSettings();
  });

  connect(m_types_list, &QListWidget::itemChanged, this, &LogConfigWidget::SaveSettings);

  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });
}

void LogConfigWidget::LoadSettings()
{
  const auto* const log_manager = Common::Log::LogManager::GetInstance();
  const auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("logconfigwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("logconfigwidget/floating")).toBool());

  // Config - Verbosity
  const Common::Log::LogLevel verbosity = log_manager->GetLogLevel();
  m_verbosity_notice->setChecked(verbosity == Common::Log::LogLevel::LNOTICE);
  m_verbosity_error->setChecked(verbosity == Common::Log::LogLevel::LERROR);
  m_verbosity_warning->setChecked(verbosity == Common::Log::LogLevel::LWARNING);
  m_verbosity_info->setChecked(verbosity == Common::Log::LogLevel::LINFO);
  m_verbosity_debug->setChecked(verbosity == Common::Log::LogLevel::LDEBUG);

  // Config - Outputs
  m_out_file->setChecked(log_manager->IsListenerEnabled(Common::Log::LogListener::FILE_LISTENER));
  m_out_console->setChecked(
      log_manager->IsListenerEnabled(Common::Log::LogListener::CONSOLE_LISTENER));
  m_out_window->setChecked(
      log_manager->IsListenerEnabled(Common::Log::LogListener::LOG_WINDOW_LISTENER));

  // Config - Log Types
  for (int i = 0; i < static_cast<int>(Common::Log::LogType::NUMBER_OF_LOGS); ++i)
  {
    const auto log_type = static_cast<Common::Log::LogType>(i);
    const bool log_enabled = log_manager->IsEnabled(log_type);

    if (!log_enabled)
      m_all_enabled = false;

    m_types_list->item(i)->setCheckState(log_enabled ? Qt::Checked : Qt::Unchecked);
  }
}

void LogConfigWidget::SaveSettings()
{
  if (m_block_save)
    return;

  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("logconfigwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("logconfigwidget/floating"), isFloating());

  // Config - Verbosity
  auto verbosity = Common::Log::LogLevel::LNOTICE;

  if (m_verbosity_notice->isChecked())
    verbosity = Common::Log::LogLevel::LNOTICE;

  if (m_verbosity_error->isChecked())
    verbosity = Common::Log::LogLevel::LERROR;

  if (m_verbosity_warning->isChecked())
    verbosity = Common::Log::LogLevel::LWARNING;

  if (m_verbosity_info->isChecked())
    verbosity = Common::Log::LogLevel::LINFO;

  if (m_verbosity_debug->isChecked())
    verbosity = Common::Log::LogLevel::LDEBUG;

  auto* const log_manager = Common::Log::LogManager::GetInstance();

  // Config - Verbosity
  log_manager->SetLogLevel(verbosity);

  // Config - Outputs
  log_manager->EnableListener(Common::Log::LogListener::FILE_LISTENER, m_out_file->isChecked());
  log_manager->EnableListener(Common::Log::LogListener::CONSOLE_LISTENER,
                              m_out_console->isChecked());
  log_manager->EnableListener(Common::Log::LogListener::LOG_WINDOW_LISTENER,
                              m_out_window->isChecked());
  // Config - Log Types
  for (int i = 0; i < static_cast<int>(Common::Log::LogType::NUMBER_OF_LOGS); ++i)
  {
    const auto type = static_cast<Common::Log::LogType>(i);
    const bool enabled = m_types_list->item(i)->checkState() == Qt::Checked;
    const bool was_enabled = log_manager->IsEnabled(type);

    if (enabled != was_enabled)
      log_manager->SetEnable(type, enabled);
  }
}

void LogConfigWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetLogConfigVisible(false);
}
