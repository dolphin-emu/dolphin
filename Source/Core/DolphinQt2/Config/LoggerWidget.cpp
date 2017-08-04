// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/LoggerWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSettings>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"

// Delay in ms between calls of UpdateLog()
constexpr int UPDATE_LOG_DELAY = 100;
// Maximum lines to process at a time
constexpr int MAX_LOG_LINES = 200;
// Timestamp length
constexpr int TIMESTAMP_LENGTH = 10;

LoggerWidget::LoggerWidget(QWidget* parent)
    : QDockWidget(tr("Logging"), parent), m_timer(new QTimer(this))
{
  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();
  CreateMainLayout();
  LoadSettings();

  ConnectWidgets();
  OnTabVisibilityChanged();

  LogManager::GetInstance()->RegisterListener(LogListener::LOG_WINDOW_LISTENER, this);

  connect(m_timer, &QTimer::timeout, this, &LoggerWidget::UpdateLog);
  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, this,
          &LoggerWidget::OnTabVisibilityChanged);
  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, this,
          &LoggerWidget::OnTabVisibilityChanged);

  m_timer->start(UPDATE_LOG_DELAY);

  QSettings settings;

  restoreGeometry(settings.value(QStringLiteral("logging/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("logging/floating")).toBool());

  installEventFilter(this);
}

LoggerWidget::~LoggerWidget()
{
  SaveSettings();

  LogManager::GetInstance()->RegisterListener(LogListener::LOG_WINDOW_LISTENER, nullptr);
}

void LoggerWidget::UpdateLog()
{
  std::lock_guard<std::mutex> lock(m_log_mutex);

  if (m_log_queue.empty())
    return;

  auto* vscroll = m_log_text->verticalScrollBar();
  auto* hscroll = m_log_text->horizontalScrollBar();

  // If the vertical scrollbar is within 50 units of the maximum value, count it as being at the
  // bottom
  bool vscroll_bottom = vscroll->maximum() - vscroll->value() < 50;

  int old_horizontal = hscroll->value();
  int old_vertical = vscroll->value();

  for (int i = 0; !m_log_queue.empty() && i < MAX_LOG_LINES; i++)
  {
    m_log_text->append(m_log_queue.front());
    m_log_queue.pop();
  }

  if (hscroll->value() != old_horizontal)
    hscroll->setValue(old_horizontal);

  if (vscroll->value() != old_vertical)
  {
    if (vscroll_bottom)
      vscroll->setValue(vscroll->maximum());
    else
      vscroll->setValue(old_vertical);
  }
}

void LoggerWidget::UpdateFont()
{
  QFont f;

  switch (m_log_font->currentIndex())
  {
  case 0:  // Default font
    break;
  case 1:  // Monospace font
    f = QFont(QStringLiteral("Monospace"));
    f.setStyleHint(QFont::TypeWriter);
    break;
  }
  m_log_text->setFont(f);
}

void LoggerWidget::CreateWidgets()
{
  m_tab_widget = new QTabWidget;

  // Log
  m_tab_log = new QWidget;
  m_log_text = new QTextEdit;
  m_log_wrap = new QCheckBox(tr("Word Wrap"));
  m_log_font = new QComboBox;
  m_log_clear = new QPushButton(tr("Clear"));

  m_log_font->addItems({tr("Default Font"), tr("Monospaced Font")});

  auto* log_layout = new QGridLayout;
  m_tab_log->setLayout(log_layout);
  log_layout->addWidget(m_log_wrap, 0, 0);
  log_layout->addWidget(m_log_font, 0, 1);
  log_layout->addWidget(m_log_clear, 0, 2);
  log_layout->addWidget(m_log_text, 1, 0, 1, -1);

  m_log_text->setReadOnly(true);

  QPalette palette = m_log_text->palette();
  palette.setColor(QPalette::Base, Qt::black);
  palette.setColor(QPalette::Text, Qt::white);
  m_log_text->setPalette(palette);

  // Configuration
  m_tab_config = new QWidget();
  auto* config_layout = new QVBoxLayout;
  m_tab_config->setLayout(config_layout);

  auto* config_verbosity = new QGroupBox(tr("Verbosity"));
  auto* verbosity_layout = new QVBoxLayout;
  config_verbosity->setLayout(verbosity_layout);
  m_verbosity_notice = new QRadioButton(tr("Notice"));
  m_verbosity_error = new QRadioButton(tr("Error"));
  m_verbosity_warning = new QRadioButton(tr("Warning"));
  m_verbosity_info = new QRadioButton(tr("Info"));

  auto* config_outputs = new QGroupBox(tr("Logger Outputs"));
  auto* outputs_layout = new QVBoxLayout;
  config_outputs->setLayout(outputs_layout);
  m_out_file = new QCheckBox(tr("Write to File"));
  m_out_console = new QCheckBox(tr("Write to Console"));
  m_out_window = new QCheckBox(tr("Write to Window"));

  auto* config_types = new QGroupBox(tr("Log Types"));
  auto* types_layout = new QVBoxLayout;
  config_types->setLayout(types_layout);
  m_types_toggle = new QPushButton(tr("Toggle All Log Types"));
  m_types_list = new QListWidget;

  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
  {
    QListWidgetItem* widget = new QListWidgetItem(QString::fromStdString(
        LogManager::GetInstance()->GetFullName(static_cast<LogTypes::LOG_TYPE>(i))));
    widget->setCheckState(Qt::Unchecked);
    m_types_list->addItem(widget);
  }

  config_layout->addWidget(config_verbosity);
  verbosity_layout->addWidget(m_verbosity_notice);
  verbosity_layout->addWidget(m_verbosity_error);
  verbosity_layout->addWidget(m_verbosity_warning);
  verbosity_layout->addWidget(m_verbosity_info);

  config_layout->addWidget(config_outputs);
  outputs_layout->addWidget(m_out_file);
  outputs_layout->addWidget(m_out_console);
  outputs_layout->addWidget(m_out_window);

  config_layout->addWidget(config_types);
  types_layout->addWidget(m_types_toggle);
  types_layout->addWidget(m_types_list);
}

void LoggerWidget::CreateMainLayout()
{
  QWidget* widget = new QWidget;
  QVBoxLayout* layout = new QVBoxLayout;

  widget->setLayout(layout);
  layout->addWidget(m_tab_widget);

  setWidget(widget);
}

void LoggerWidget::ConnectWidgets()
{
  // Configuration
  connect(m_verbosity_notice, &QRadioButton::toggled, this, &LoggerWidget::SaveSettings);
  connect(m_verbosity_error, &QRadioButton::toggled, this, &LoggerWidget::SaveSettings);
  connect(m_verbosity_warning, &QRadioButton::toggled, this, &LoggerWidget::SaveSettings);
  connect(m_verbosity_info, &QRadioButton::toggled, this, &LoggerWidget::SaveSettings);

  connect(m_out_file, &QCheckBox::toggled, this, &LoggerWidget::SaveSettings);
  connect(m_out_console, &QCheckBox::toggled, this, &LoggerWidget::SaveSettings);
  connect(m_out_window, &QCheckBox::toggled, this, &LoggerWidget::SaveSettings);

  connect(m_types_toggle, &QPushButton::clicked, [this] {
    m_all_enabled = !m_all_enabled;

    // Don't save every time we change an item
    m_block_save = true;

    for (int i = 0; i < m_types_list->count(); i++)
      m_types_list->item(i)->setCheckState(m_all_enabled ? Qt::Checked : Qt::Unchecked);

    m_block_save = false;

    SaveSettings();
  });

  connect(m_types_list, &QListWidget::itemChanged, this, &LoggerWidget::SaveSettings);

  // Log
  connect(m_log_clear, &QPushButton::clicked, m_log_text, &QTextEdit::clear);
  connect(m_log_wrap, &QCheckBox::toggled, this, &LoggerWidget::SaveSettings);
  connect(m_log_font, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &LoggerWidget::SaveSettings);

  // Window tracking
  connect(this, &QDockWidget::topLevelChanged, this, &LoggerWidget::SaveSettings);
}

void LoggerWidget::LoadSettings()
{
  auto* logmanager = LogManager::GetInstance();
  QSettings settings;

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

  // Log - Wrap Lines
  m_log_wrap->setChecked(settings.value(QStringLiteral("logging/wraplines")).toBool());
  m_log_text->setLineWrapMode(m_log_wrap->isChecked() ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);

  // Log - Font Selection
  // Currently "Debugger Font" is not supported as there is no Qt Debugger, defaulting to Monospace
  m_log_font->setCurrentIndex(std::min(settings.value(QStringLiteral("logging/font")).toInt(), 1));
  UpdateFont();
}

void LoggerWidget::SaveSettings()
{
  if (m_block_save)
    return;

  QSettings settings;

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

  // Log - Wrap Lines
  settings.setValue(QStringLiteral("logging/wraplines"), m_log_wrap->isChecked());
  m_log_text->setLineWrapMode(m_log_wrap->isChecked() ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);

  // Log - Font Selection
  settings.setValue(QStringLiteral("logging/font"), m_log_font->currentIndex());
  UpdateFont();

  // Save window geometry
  settings.setValue(QStringLiteral("logging/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("logging/floating"), isFloating());
}

void LoggerWidget::OnTabVisibilityChanged()
{
  bool log_visible = Settings::Instance().IsLogVisible();
  bool config_visible = Settings::Instance().IsLogConfigVisible();

  m_tab_widget->clear();

  if (log_visible)
    m_tab_widget->addTab(m_tab_log, tr("Log"));

  if (config_visible)
    m_tab_widget->addTab(m_tab_config, tr("Configuration"));

  if (!log_visible && !config_visible)
    hide();
  else
    show();
}

void LoggerWidget::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  std::lock_guard<std::mutex> lock(m_log_mutex);

  const char* color = "white";

  switch (level)
  {
  case LogTypes::LOG_LEVELS::LERROR:
    color = "red";
    break;
  case LogTypes::LOG_LEVELS::LWARNING:
    color = "yellow";
    break;
  case LogTypes::LOG_LEVELS::LNOTICE:
    color = "green";
    break;
  case LogTypes::LOG_LEVELS::LINFO:
    color = "cyan";
    break;
  case LogTypes::LOG_LEVELS::LDEBUG:
    color = "lightgrey";
    break;
  }

  m_log_queue.push(QStringLiteral("%1 <font color='%2'>%3</font>")
                       .arg(QString::fromStdString(std::string(text).substr(0, TIMESTAMP_LENGTH)),
                            QString::fromStdString(color),
                            QString::fromStdString(std::string(text).substr(TIMESTAMP_LENGTH))));
}

bool LoggerWidget::eventFilter(QObject* object, QEvent* event)
{
  QSettings settings;

  if (event->type() == QEvent::Hide || event->type() == QEvent::Resize ||
      event->type() == QEvent::Move)
  {
    SaveSettings();
  }

  if (event->type() == QEvent::Close)
  {
    Settings::Instance().SetLogVisible(false);
    Settings::Instance().SetLogConfigVisible(false);
  }

  return false;
}
