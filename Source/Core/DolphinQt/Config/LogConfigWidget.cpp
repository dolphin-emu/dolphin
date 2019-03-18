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

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"

#include "DolphinQt/Settings.h"

class LogTypeModel : public QAbstractItemModel
{
public:
  LogTypeModel(QObject* parent = nullptr) : QAbstractItemModel(parent) {}

  int columnCount(const QModelIndex& parent) const override { return 4; }
  QVariant data(const QModelIndex& index, int role) const override
  {
    if (!index.isValid())
    {
      return QVariant();
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
      return QVariant();
    }
    switch (index.column())
    {
    case 0:
      return QString::fromStdString(LogTypes::INFO[index.internalId()].long_name);
    case 1:
      return LogManager::GetInstance()->GetLogLevel(LogTypes::LOG_TYPE(index.internalId()));
    case 2:
      return index.internalId();
    case 3:
      return LogTypes::INFO[index.internalId()].end;
    default:
      return QVariant();
    }
  }
  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    if (!index.isValid())
    {
      return Qt::NoItemFlags;
    }
    if (index.column() == 0)
    {
      return LogTypes::INFO[index.internalId()].end ? Qt::ItemIsEnabled :
                                                      Qt::ItemNeverHasChildren | Qt::ItemIsEnabled;
    }
    else if (index.column() == 1)
    {
      return Qt::ItemNeverHasChildren | Qt::ItemIsEnabled;
    }
    else
    {
      return Qt::ItemNeverHasChildren;
    }
  }
  bool hasChildren(const QModelIndex& parent) const override
  {
    if (!parent.isValid())
    {
      return true;
    }
    if (parent.column() != 0)
    {
      return false;
    }
    return !LogTypes::IsLeaf(LogTypes::LOG_TYPE(parent.internalId()));
  }
  QModelIndex index(int row, int column, const QModelIndex& parent) const override
  {
    if (column < 0 || column > 3)
    {
      // have only 2 columns ⇒ invalid
      return QModelIndex();
    }
    if (!parent.isValid())
    {
      if (row != 0)
      {
        return QModelIndex();
      }
      return createIndex(0, column, quintptr(0));
    }
    auto par_type = LogTypes::LOG_TYPE(parent.internalId());
    ASSERT(LogTypes::IsValid(par_type));
    auto child_type = LogTypes::FirstChild(par_type);
    for (int count = 0; count < row; ++count)
    {
      ASSERT(LogTypes::IsValid(child_type));
      child_type = LogTypes::NextSibling(child_type);
    }
    return createIndex(row, column, quintptr(child_type));
  }
  QModelIndex parent(const QModelIndex& child) const override
  {
    if (!child.isValid() || child.internalId() == 0)
    {
      return QModelIndex();
    }
    auto par_type = LogTypes::Parent(LogTypes::LOG_TYPE(child.internalId()));
    if (IsValid(par_type))
    {
      auto grandpar = LogTypes::Parent(par_type);
      if (LogTypes::IsValid(grandpar))
      {
        return createIndex(int(grandpar) - 1 - int(par_type), 0, quintptr(par_type));
      }
      else
      {
        return createIndex(0, 0, quintptr(0));
      }
    }
    return QModelIndex();
  }
  int rowCount(const QModelIndex& parent) const override
  {
    if (!parent.isValid())
    {
      return 1;
    }
    return LogTypes::CountChildren(LogTypes::LOG_TYPE(parent.internalId()));
  }
  bool setData(const QModelIndex& index, const QVariant& value, int role) override
  {
    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
      return false;
    }
    if (!index.isValid() || index.column() != 1)
    {
      return false;
    }
    bool ok;
    int val = value.toInt(&ok);
    if (!ok)
    {
      return false;
    }
    if (val < 0 || val > MAX_LOGLEVEL)
    {
      return false;
    }
    LogManager::GetInstance()->SetLogLevel(LogTypes::LOG_TYPE(index.internalId()),
                                           LogTypes::LOG_LEVELS(val));
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
  }
};

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
