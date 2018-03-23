// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Debugger/WatchWidget.h"

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Settings.h"

#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

WatchWidget::WatchWidget(QWidget* parent) : QDockWidget(parent)
{
  // i18n: This kind of "watch" is used for watching emulated memory.
  // It's not related to timekeeping devices.
  setWindowTitle(tr("Watch"));
  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("watchwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("watchwidget/floating")).toBool());

  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [this](Core::State state) {
    if (!Settings::Instance().IsDebugModeEnabled())
      return;

    m_load->setEnabled(Core::IsRunning());
    m_save->setEnabled(Core::IsRunning());

    if (state != Core::State::Starting)
      Update();
  });

  connect(&Settings::Instance(), &Settings::WatchVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsWatchVisible()); });

  setHidden(!Settings::Instance().IsWatchVisible() || !Settings::Instance().IsDebugModeEnabled());

  Update();
}

WatchWidget::~WatchWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("watchwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("watchwidget/floating"), isFloating());
}

void WatchWidget::CreateWidgets()
{
  m_toolbar = new QToolBar;
  m_table = new QTableWidget;

  m_table->setColumnCount(5);
  m_table->verticalHeader()->setHidden(true);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);

  m_load = AddAction(m_toolbar, tr("Load"), this, &WatchWidget::OnLoad);
  m_save = AddAction(m_toolbar, tr("Save"), this, &WatchWidget::OnSave);

  m_load->setEnabled(false);
  m_save->setEnabled(false);

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_toolbar);
  layout->addWidget(m_table);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void WatchWidget::ConnectWidgets()
{
  connect(m_table, &QTableWidget::customContextMenuRequested, this, &WatchWidget::ShowContextMenu);
  connect(m_table, &QTableWidget::itemChanged, this, &WatchWidget::OnItemChanged);
}

void WatchWidget::Update()
{
  m_updating = true;

  m_table->clear();

  int size = static_cast<int>(PowerPC::watches.GetWatches().size());

  m_table->setRowCount(size + 1);

  m_table->setHorizontalHeaderLabels({tr("Label"), tr("Address"), tr("Hexadecimal"), tr("Decimal"),
                                      // i18n: Data type used in computing
                                      tr("String")});

  for (int i = 0; i < size; i++)
  {
    auto entry = PowerPC::watches.GetWatches().at(i);

    auto* label = new QTableWidgetItem(QString::fromStdString(entry.name));
    auto* address =
        new QTableWidgetItem(QStringLiteral("%1").arg(entry.address, 8, 16, QLatin1Char('0')));
    auto* hex = new QTableWidgetItem;
    auto* decimal = new QTableWidgetItem;
    auto* string = new QTableWidgetItem;

    QBrush brush = QPalette().brush(QPalette::Text);

    if (!Core::IsRunning() || !PowerPC::HostIsRAMAddress(entry.address))
      brush.setColor(Qt::red);

    if (Core::IsRunning())
    {
      if (PowerPC::HostIsRAMAddress(entry.address))
      {
        hex->setText(QStringLiteral("%1").arg(PowerPC::HostRead_U32(entry.address), 8, 16,
                                              QLatin1Char('0')));
        decimal->setText(QString::number(PowerPC::HostRead_U32(entry.address)));
        string->setText(QString::fromStdString(PowerPC::HostGetString(entry.address, 32)));
      }
    }

    address->setForeground(brush);

    int column = 0;

    for (auto* item : {label, address, hex, decimal, string})
    {
      item->setData(Qt::UserRole, i);
      item->setData(Qt::UserRole + 1, column++);
    }

    string->setFlags(Qt::ItemIsEnabled);

    m_table->setItem(i, 0, label);
    m_table->setItem(i, 1, address);
    m_table->setItem(i, 2, hex);
    m_table->setItem(i, 3, decimal);
    m_table->setItem(i, 4, string);
  }

  auto* label = new QTableWidgetItem;
  label->setData(Qt::UserRole, -1);

  m_table->setItem(size, 0, label);

  for (int i = 1; i < 5; i++)
  {
    auto* no_edit = new QTableWidgetItem;
    no_edit->setFlags(Qt::ItemIsEnabled);
    m_table->setItem(size, i, no_edit);
  }

  m_updating = false;
}

void WatchWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetWatchVisible(false);
}

void WatchWidget::OnLoad()
{
  IniFile ini;

  Watches::TWatchesStr watches;

  if (!ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
                false))
  {
    return;
  }

  if (ini.GetLines("Watches", &watches, false))
  {
    PowerPC::watches.Clear();
    PowerPC::watches.AddFromStrings(watches);
  }

  Update();
}

void WatchWidget::OnSave()
{
  IniFile ini;
  ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
           false);
  ini.SetLines("Watches", PowerPC::watches.GetStrings());
  ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini");
}

void WatchWidget::ShowContextMenu()
{
  QMenu* menu = new QMenu(this);

  if (m_table->selectedItems().size())
  {
    auto row_variant = m_table->selectedItems()[0]->data(Qt::UserRole);

    if (!row_variant.isNull())
    {
      int row = row_variant.toInt();

      if (row >= 0)
      {
        // i18n: This kind of "watch" is used for watching emulated memory.
        // It's not related to timekeeping devices.
        AddAction(menu, tr("&Delete Watch"), this, [this, row] { DeleteWatch(row); });
        AddAction(menu, tr("&Add Memory Breakpoint"), this,
                  [this, row] { AddWatchBreakpoint(row); });
      }
    }
  }

  menu->addSeparator();

  AddAction(menu, tr("Update"), this, &WatchWidget::Update);

  menu->exec(QCursor::pos());
}

void WatchWidget::OnItemChanged(QTableWidgetItem* item)
{
  if (m_updating || item->data(Qt::UserRole).isNull())
    return;

  int row = item->data(Qt::UserRole).toInt();
  int column = item->data(Qt::UserRole + 1).toInt();

  if (row == -1)
  {
    if (!item->text().isEmpty())
    {
      AddWatch(item->text(), 0);

      Update();
      return;
    }
  }
  else
  {
    switch (column)
    {
    // Label
    case 0:
      if (item->text().isEmpty())
        DeleteWatch(row);
      else
        PowerPC::watches.UpdateName(row, item->text().toStdString());
      break;
    // Address
    // Hexadecimal
    // Decimal
    case 1:
    case 2:
    case 3:
    {
      bool good;
      quint32 value = item->text().toUInt(&good, column < 3 ? 16 : 10);

      if (good)
      {
        if (column == 1)
          PowerPC::watches.Update(row, value);
        else
          PowerPC::HostWrite_U32(value, PowerPC::watches.GetWatches().at(row).address);
      }
      else
      {
        QMessageBox::critical(this, tr("Error"), tr("Invalid input provided"));
      }
      break;
    }
    }

    Update();
  }
}

void WatchWidget::DeleteWatch(int row)
{
  PowerPC::watches.Remove(PowerPC::watches.GetWatches().at(row).address);
  Update();
}

void WatchWidget::AddWatchBreakpoint(int row)
{
  emit RequestMemoryBreakpoint(PowerPC::watches.GetWatches().at(row).address);
}

void WatchWidget::AddWatch(QString name, u32 addr)
{
  PowerPC::watches.Add(addr);
  PowerPC::watches.UpdateName(static_cast<int>(PowerPC::watches.GetWatches().size()) - 1,
                              name.toStdString());
}
