// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/LogWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>

#include "Core/ConfigManager.h"

#include "DolphinQt/Settings.h"

// Delay in ms between calls of UpdateLog()
constexpr int UPDATE_LOG_DELAY = 100;
// Maximum lines to process at a time
constexpr size_t MAX_LOG_LINES_TO_UPDATE = 200;
// Timestamp length
constexpr size_t TIMESTAMP_LENGTH = 10;

// A helper function to construct QString from std::string_view in one line
static QString QStringFromStringView(std::string_view str)
{
  return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
}

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
  connect(this, &QDockWidget::visibilityChanged, [this](bool visible) {
    if (visible)
      m_timer->start(UPDATE_LOG_DELAY);
    else
      m_timer->stop();
  });

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &LogWidget::UpdateFont);

  Common::Log::LogManager::GetInstance()->RegisterListener(LogListener::LOG_WINDOW_LISTENER, this);
}

LogWidget::~LogWidget()
{
  SaveSettings();

  Common::Log::LogManager::GetInstance()->RegisterListener(LogListener::LOG_WINDOW_LISTENER,
                                                           nullptr);
}

void LogWidget::UpdateLog()
{
  std::vector<LogEntry> elements_to_push;
  {
    std::lock_guard lock(m_log_mutex);
    if (m_log_ring_buffer.empty())
      return;

    elements_to_push.reserve(std::min(MAX_LOG_LINES_TO_UPDATE, m_log_ring_buffer.size()));

    for (size_t i = 0; !m_log_ring_buffer.empty() && i < MAX_LOG_LINES_TO_UPDATE; i++)
      elements_to_push.push_back(m_log_ring_buffer.pop_front());
  }

  for (auto& line : elements_to_push)
  {
    const char* color = "white";
    switch (std::get<Common::Log::LogLevel>(line))
    {
    case Common::Log::LogLevel::LERROR:
      color = "red";
      break;
    case Common::Log::LogLevel::LWARNING:
      color = "yellow";
      break;
    case Common::Log::LogLevel::LNOTICE:
      color = "lime";
      break;
    case Common::Log::LogLevel::LINFO:
      color = "cyan";
      break;
    case Common::Log::LogLevel::LDEBUG:
      color = "lightgrey";
      break;
    }

    const std::string_view str_view(std::get<std::string>(line));

    m_log_text->appendHtml(
        QStringLiteral("%1<span style=\"color: %2; white-space: pre\">%3</span>")
            .arg(QStringFromStringView(str_view.substr(0, TIMESTAMP_LENGTH)),
                 QString::fromUtf8(color),
                 QStringFromStringView(str_view.substr(TIMESTAMP_LENGTH)).toHtmlEscaped()));
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
    f = QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
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
  m_log_text = new QPlainTextEdit;
  m_log_wrap = new QCheckBox(tr("Word Wrap"));
  m_log_font = new QComboBox;
  m_log_clear = new QPushButton(tr("Clear"));

  m_log_font->addItems({tr("Default Font"), tr("Monospaced Font"), tr("Selected Font")});

  auto* log_layout = new QGridLayout;
  log_layout->addWidget(m_log_wrap, 0, 0);
  log_layout->addWidget(m_log_font, 0, 1);
  log_layout->addWidget(m_log_clear, 0, 2);
  log_layout->addWidget(m_log_text, 1, 0, 1, -1);

  QWidget* widget = new QWidget;
  widget->setLayout(log_layout);

  setWidget(widget);

  m_log_text->setReadOnly(true);
  m_log_text->setUndoRedoEnabled(false);
  m_log_text->setMaximumBlockCount(MAX_LOG_LINES);

  QPalette palette = m_log_text->palette();
  palette.setColor(QPalette::Base, Qt::black);
  palette.setColor(QPalette::Text, Qt::white);
  m_log_text->setPalette(palette);
}

void LogWidget::ConnectWidgets()
{
  connect(m_log_clear, &QPushButton::clicked, [this] {
    m_log_text->clear();
    m_log_ring_buffer.clear();
  });
  connect(m_log_wrap, &QCheckBox::toggled, this, &LogWidget::SaveSettings);
  connect(m_log_font, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LogWidget::SaveSettings);
  connect(this, &QDockWidget::topLevelChanged, this, &LogWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, this, &LogWidget::setVisible);
}

void LogWidget::LoadSettings()
{
  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("logwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("logwidget/floating")).toBool());

  // Log - Wrap Lines
  m_log_wrap->setChecked(settings.value(QStringLiteral("logging/wraplines")).toBool());
  m_log_text->setLineWrapMode(m_log_wrap->isChecked() ? QPlainTextEdit::WidgetWidth :
                                                        QPlainTextEdit::NoWrap);
  m_log_text->setHorizontalScrollBarPolicy(m_log_wrap->isChecked() ? Qt::ScrollBarAsNeeded :
                                                                     Qt::ScrollBarAlwaysOn);

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
  m_log_text->setLineWrapMode(m_log_wrap->isChecked() ? QPlainTextEdit::WidgetWidth :
                                                        QPlainTextEdit::NoWrap);
  m_log_text->setHorizontalScrollBarPolicy(m_log_wrap->isChecked() ? Qt::ScrollBarAsNeeded :
                                                                     Qt::ScrollBarAlwaysOn);

  // Log - Font Selection
  settings.setValue(QStringLiteral("logging/font"), m_log_font->currentIndex());
  UpdateFont();
}

void LogWidget::Log(Common::Log::LogLevel level, const char* text)
{
  size_t text_length = strlen(text);
  while (text_length > 0 && text[text_length - 1] == '\n')
    text_length--;

  std::lock_guard lock(m_log_mutex);
  m_log_ring_buffer.emplace(std::piecewise_construct, std::forward_as_tuple(text, text_length),
                            std::forward_as_tuple(level));
}

void LogWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetLogVisible(false);
}
