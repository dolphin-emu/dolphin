// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/LogConfigWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"

#include "DolphinQt/Config/LogTypeModel.h"
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

  auto* outputs = new QGroupBox(tr("Logger Outputs"));
  auto* outputs_layout = new QVBoxLayout;
  outputs->setLayout(outputs_layout);
  m_out_file = new QCheckBox(tr("Write to File"));
  m_out_console = new QCheckBox(tr("Write to Console"));
  m_out_window = new QCheckBox(tr("Write to Window"));

  m_log_types = new QTreeView(this);
  m_log_types->setModel(new LogTypeModel(m_log_types));

  layout->addWidget(outputs);
  outputs_layout->addWidget(m_out_file);
  outputs_layout->addWidget(m_out_console);
  outputs_layout->addWidget(m_out_window);

  layout->addWidget(m_log_types);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void LogConfigWidget::ConnectWidgets()
{
  // Configuration
  connect(m_out_file, &QCheckBox::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_out_console, &QCheckBox::toggled, this, &LogConfigWidget::SaveSettings);
  connect(m_out_window, &QCheckBox::toggled, this, &LogConfigWidget::SaveSettings);

  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });
}

void LogConfigWidget::LoadSettings()
{
  auto* logmanager = LogManager::GetInstance();
  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("logconfigwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("logconfigwidget/floating")).toBool());
  // Config - Outputs
  m_out_file->setChecked(logmanager->IsListenerEnabled(LogListener::FILE_LISTENER));
  m_out_console->setChecked(logmanager->IsListenerEnabled(LogListener::CONSOLE_LISTENER));
  m_out_window->setChecked(logmanager->IsListenerEnabled(LogListener::LOG_WINDOW_LISTENER));
}

void LogConfigWidget::SaveSettings()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("logconfigwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("logconfigwidget/floating"), isFloating());

  LogManager* lm = LogManager::GetInstance();

  // Config - Outputs
  lm->EnableListener(LogListener::FILE_LISTENER, m_out_file->isChecked());
  lm->EnableListener(LogListener::CONSOLE_LISTENER, m_out_console->isChecked());
  lm->EnableListener(LogListener::LOG_WINDOW_LISTENER, m_out_window->isChecked());

  lm->SaveSettings();
}

void LogConfigWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetLogConfigVisible(false);
}
