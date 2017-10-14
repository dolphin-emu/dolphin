// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/LogConfigWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"

LogConfigWidget::LogConfigWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Log Configuration"));
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

  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
  {
    QListWidgetItem* widget = new QListWidgetItem(QString::fromStdString(
        LogManager::GetInstance()->GetFullName(static_cast<LogTypes::LOG_TYPE>(i))));
    widget->setCheckState(Qt::Unchecked);
    m_types_list->addItem(widget);
  }

  layout->addWidget(verbosity);
  verbosity_layout->addWidget(m_verbosity_notice);
  verbosity_layout->addWidget(m_verbosity_error);
  verbosity_layout->addWidget(m_verbosity_warning);
  verbosity_layout->addWidget(m_verbosity_info);

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
  auto* logmanager = LogManager::GetInstance();
  QSettings settings;

  restoreGeometry(settings.value(QStringLiteral("logconfigwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("logconfigwidget/floating")).toBool());

  // Config - Verbosity
  int verbosity = logmanager->GetLogLevel();
  m_verbosity_notice->setChecked(verbosity == 1);
  m_verbosity_error->setChecked(verbosity == 2);
  m_verbosity_warning->setChecked(verbosity == 3);
  m_verbosity_info->setChecked(verbosity == 4);

  // Config - Outputs
  m_out_file->setChecked(logmanager->IsListenerEnabled(LogListener::FILE_LISTENER));
  m_out_console->setChecked(logmanager->IsListenerEnabled(LogListener::CONSOLE_LISTENER));
  m_out_window->setChecked(logmanager->IsListenerEnabled(LogListener::LOG_WINDOW_LISTENER));

  // Config - Log Types
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
  {
    bool log_enabled = LogManager::GetInstance()->IsEnabled(static_cast<LogTypes::LOG_TYPE>(i));

    if (!log_enabled)
      m_all_enabled = false;

    m_types_list->item(i)->setCheckState(log_enabled ? Qt::Checked : Qt::Unchecked);
  }
}

void LogConfigWidget::SaveSettings()
{
  if (m_block_save)
    return;

  QSettings settings;

  settings.setValue(QStringLiteral("logconfigwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("logconfigwidget/floating"), isFloating());

  // Config - Verbosity
  int verbosity = 1;

  if (m_verbosity_notice->isChecked())
    verbosity = 1;

  if (m_verbosity_error->isChecked())
    verbosity = 2;

  if (m_verbosity_warning->isChecked())
    verbosity = 3;

  if (m_verbosity_info->isChecked())
    verbosity = 4;

  // Config - Verbosity
  LogManager::GetInstance()->SetLogLevel(static_cast<LogTypes::LOG_LEVELS>(verbosity));

  // Config - Outputs
  LogManager::GetInstance()->EnableListener(LogListener::FILE_LISTENER, m_out_file->isChecked());
  LogManager::GetInstance()->EnableListener(LogListener::CONSOLE_LISTENER,
                                            m_out_console->isChecked());
  LogManager::GetInstance()->EnableListener(LogListener::LOG_WINDOW_LISTENER,
                                            m_out_window->isChecked());
  // Config - Log Types
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
  {
    const auto type = static_cast<LogTypes::LOG_TYPE>(i);
    bool enabled = m_types_list->item(i)->checkState() == Qt::Checked;
    bool was_enabled = LogManager::GetInstance()->IsEnabled(type);

    if (enabled != was_enabled)
      LogManager::GetInstance()->SetEnable(type, enabled);
  }
}

void LogConfigWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetLogConfigVisible(false);
}
