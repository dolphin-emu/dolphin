// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/LogWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

// Delay in ms between calls of UpdateLog()
constexpr int UPDATE_LOG_DELAY = 100;
// Maximum lines to process at a time
constexpr int MAX_LOG_LINES = 200;
// Timestamp length
constexpr int TIMESTAMP_LENGTH = 10;

LogWidget::LogWidget(QWidget* parent) : QDockWidget(parent), m_timer(new QTimer(this))
{
  setWindowTitle(tr("Log"));
  setObjectName(QStringLiteral("log"));

  setHidden(!Settings::Instance().IsLogVisible());
  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();
  LoadSettings();

  ConnectWidgets();

  connect(m_timer, &QTimer::timeout, this, &LogWidget::UpdateLog);
  m_timer->start(UPDATE_LOG_DELAY);

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &LogWidget::UpdateFont);

  LogManager::GetInstance()->RegisterListener(LogListener::LOG_WINDOW_LISTENER, this);
}

LogWidget::~LogWidget()
{
  SaveSettings();

  LogManager::GetInstance()->RegisterListener(LogListener::LOG_WINDOW_LISTENER, nullptr);
}

void LogWidget::UpdateLog()
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

void LogWidget::UpdateFont()
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
  case 2:  // Debugger font
    f = Settings::Instance().GetDebugFont();
    break;
  }
  m_log_text->setFont(f);
}

void LogWidget::CreateWidgets()
{
  // Log
  m_tab_log = new QWidget;
  m_log_text = new QTextEdit;
  m_log_wrap = new QCheckBox(tr("Word Wrap"));
  m_log_font = new QComboBox;
  m_log_clear = new QPushButton(tr("Clear"));

  m_log_font->addItems({tr("Default Font"), tr("Monospaced Font"), tr("Selected Font")});

  auto* log_layout = new QGridLayout;
  m_tab_log->setLayout(log_layout);
  log_layout->addWidget(m_log_wrap, 0, 0);
  log_layout->addWidget(m_log_font, 0, 1);
  log_layout->addWidget(m_log_clear, 0, 2);
  log_layout->addWidget(m_log_text, 1, 0, 1, -1);

  QWidget* widget = new QWidget;
  widget->setLayout(log_layout);

  setWidget(widget);

  m_log_text->setReadOnly(true);

  QPalette palette = m_log_text->palette();
  palette.setColor(QPalette::Base, Qt::black);
  palette.setColor(QPalette::Text, Qt::white);
  m_log_text->setPalette(palette);
}

void LogWidget::ConnectWidgets()
{
  connect(m_log_clear, &QPushButton::clicked, m_log_text, &QTextEdit::clear);
  connect(m_log_wrap, &QCheckBox::toggled, this, &LogWidget::SaveSettings);
  connect(m_log_font, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &LogWidget::SaveSettings);
  connect(this, &QDockWidget::topLevelChanged, this, &LogWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });
}

void LogWidget::LoadSettings()
{
  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("logwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("logwidget/floating")).toBool());

  // Log - Wrap Lines
  m_log_wrap->setChecked(settings.value(QStringLiteral("logging/wraplines")).toBool());
  m_log_text->setLineWrapMode(m_log_wrap->isChecked() ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);

  // Log - Font Selection
  // Currently "Debugger Font" is not supported as there is no Qt Debugger, defaulting to Monospace
  m_log_font->setCurrentIndex(std::min(settings.value(QStringLiteral("logging/font")).toInt(), 1));
  UpdateFont();
}

void LogWidget::SaveSettings()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("logwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("logwidget/floating"), isFloating());

  // Log - Wrap Lines
  settings.setValue(QStringLiteral("logging/wraplines"), m_log_wrap->isChecked());
  m_log_text->setLineWrapMode(m_log_wrap->isChecked() ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);

  // Log - Font Selection
  settings.setValue(QStringLiteral("logging/font"), m_log_font->currentIndex());
  UpdateFont();
}

void LogWidget::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  // The text has to be copied here as it will be deallocated after this method has returned
  std::string str(text);

  QueueOnObject(this, [this, level, str]() mutable {
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
      color = "lime";
      break;
    case LogTypes::LOG_LEVELS::LINFO:
      color = "cyan";
      break;
    case LogTypes::LOG_LEVELS::LDEBUG:
      color = "lightgrey";
      break;
    }

    StringPopBackIf(&str, '\n');
    m_log_queue.push(
        QStringLiteral("%1 <span style=\"color: %2; white-space: pre\">%3</span>")
            .arg(QString::fromStdString(str.substr(0, TIMESTAMP_LENGTH)),
                 QString::fromStdString(color),
                 QString::fromStdString(str.substr(TIMESTAMP_LENGTH)).toHtmlEscaped()));
  });
}

void LogWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetLogVisible(false);
}
