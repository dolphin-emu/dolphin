// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/WatchWidget.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

WatchWidget::WatchWidget(QWidget* parent)
    : QDockWidget(parent), m_system(Core::System::GetInstance())
{
  // i18n: This kind of "watch" is used for watching emulated memory.
  // It's not related to timekeeping devices.
  setWindowTitle(tr("Watch"));
  setObjectName(QStringLiteral("watch"));

  setHidden(!Settings::Instance().IsWatchVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("watchwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("watchwidget/floating")).toBool());

  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    UpdateButtonsEnabled();
    if (state != Core::State::Starting)
      Update();
  });

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &WatchWidget::Update);

  connect(&Settings::Instance(), &Settings::WatchVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsWatchVisible()); });

  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &WatchWidget::UpdateIcons);
  UpdateIcons();
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
  m_toolbar->setContentsMargins(0, 0, 0, 0);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_table = new QTableWidget;
  m_table->setTabKeyNavigation(false);

  m_table->setContentsMargins(0, 0, 0, 0);
  m_table->setColumnCount(NUM_COLUMNS);
  m_table->verticalHeader()->setHidden(true);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setHorizontalHeaderLabels(
      {tr("Label"), tr("Address"), tr("Hexadecimal"),
       // i18n: The base 10 numeral system. Not related to non-integer numbers
       tr("Decimal"),
       // i18n: Data type used in computing
       tr("String"),
       // i18n: Floating-point (non-integer) number
       tr("Float"), tr("Locked")});
  m_table->setRowCount(1);
  SetEmptyRow(0);

  m_new = m_toolbar->addAction(tr("New"), this, &WatchWidget::OnNewWatch);
  m_delete = m_toolbar->addAction(tr("Delete"), this, &WatchWidget::OnDelete);
  m_clear = m_toolbar->addAction(tr("Clear"), this, &WatchWidget::OnClear);
  m_load = m_toolbar->addAction(tr("Load"), this, &WatchWidget::OnLoad);
  m_save = m_toolbar->addAction(tr("Save"), this, &WatchWidget::OnSave);

  m_new->setEnabled(false);
  m_delete->setEnabled(false);
  m_clear->setEnabled(false);
  m_load->setEnabled(false);
  m_save->setEnabled(false);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(0);
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

void WatchWidget::UpdateIcons()
{
  // TODO: Create a "debugger_add_watch" icon
  m_new->setIcon(Resources::GetThemeIcon("debugger_add_breakpoint"));
  m_delete->setIcon(Resources::GetThemeIcon("debugger_delete"));
  m_clear->setIcon(Resources::GetThemeIcon("debugger_clear"));
  m_load->setIcon(Resources::GetThemeIcon("debugger_load"));
  m_save->setIcon(Resources::GetThemeIcon("debugger_save"));
}

void WatchWidget::UpdateButtonsEnabled()
{
  if (!isVisible())
    return;

  const bool is_enabled = Core::IsRunning();
  m_new->setEnabled(is_enabled);
  m_delete->setEnabled(is_enabled);
  m_clear->setEnabled(is_enabled);
  m_load->setEnabled(is_enabled);
  m_save->setEnabled(is_enabled);
}

void WatchWidget::Update()
{
  if (!isVisible())
    return;

  m_updating = true;

  if (Core::GetState() != Core::State::Paused)
  {
    m_table->setDisabled(true);
    m_updating = false;
    return;
  }

  m_table->setDisabled(false);
  m_table->clearContents();

  Core::CPUThreadGuard guard(m_system);
  auto& debug_interface = guard.GetSystem().GetPowerPC().GetDebugInterface();

  int size = static_cast<int>(debug_interface.GetWatches().size());

  m_table->setRowCount(size + 1);

  for (int i = 0; i < size; i++)
  {
    const auto& entry = debug_interface.GetWatch(i);

    auto* label = new QTableWidgetItem(QString::fromStdString(entry.name));
    auto* address =
        new QTableWidgetItem(QStringLiteral("%1").arg(entry.address, 8, 16, QLatin1Char('0')));
    auto* hex = new QTableWidgetItem;
    auto* decimal = new QTableWidgetItem;
    auto* string = new QTableWidgetItem;
    auto* floatValue = new QTableWidgetItem;

    auto* lockValue = new QTableWidgetItem;
    lockValue->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);

    std::array<QTableWidgetItem*, NUM_COLUMNS> items = {label,  address,    hex,      decimal,
                                                        string, floatValue, lockValue};

    QBrush brush = QPalette().brush(QPalette::Text);

    if (!Core::IsRunning() || !PowerPC::MMU::HostIsRAMAddress(guard, entry.address))
      brush.setColor(Qt::red);

    if (Core::IsRunning())
    {
      if (PowerPC::MMU::HostIsRAMAddress(guard, entry.address))
      {
        hex->setText(QStringLiteral("%1").arg(PowerPC::MMU::HostRead_U32(guard, entry.address), 8,
                                              16, QLatin1Char('0')));
        decimal->setText(QString::number(PowerPC::MMU::HostRead_U32(guard, entry.address)));
        string->setText(
            QString::fromStdString(PowerPC::MMU::HostGetString(guard, entry.address, 32)));
        floatValue->setText(QString::number(PowerPC::MMU::HostRead_F32(guard, entry.address)));
        lockValue->setCheckState(entry.locked ? Qt::Checked : Qt::Unchecked);
      }
    }

    address->setForeground(brush);
    string->setFlags(Qt::ItemIsEnabled);

    for (int column = 0; column < NUM_COLUMNS; column++)
    {
      auto* item = items[column];
      item->setData(Qt::UserRole, i);
      item->setData(Qt::UserRole + 1, column);
      m_table->setItem(i, column, item);
    }
  }

  SetEmptyRow(size);

  m_updating = false;
}

void WatchWidget::SetEmptyRow(int row)
{
  auto* label = new QTableWidgetItem;
  label->setData(Qt::UserRole, -1);

  m_table->setItem(row, 0, label);

  for (int i = 1; i < NUM_COLUMNS; i++)
  {
    auto* no_edit = new QTableWidgetItem;
    no_edit->setFlags(Qt::ItemIsEnabled);
    m_table->setItem(row, i, no_edit);
  }
}

void WatchWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetWatchVisible(false);
}

void WatchWidget::showEvent(QShowEvent* event)
{
  UpdateButtonsEnabled();
  Update();
}

void WatchWidget::OnDelete()
{
  if (m_table->selectedItems().empty())
    return;

  DeleteSelectedWatches();
}

void WatchWidget::OnClear()
{
  m_system.GetPowerPC().GetDebugInterface().ClearWatches();
  Update();
}

void WatchWidget::OnNewWatch()
{
  const QString text =
      QInputDialog::getText(this, tr("Input"), tr("Enter address to watch:"), QLineEdit::Normal,
                            QString{}, nullptr, Qt::WindowCloseButtonHint);
  bool good;
  const uint address = text.toUInt(&good, 16);

  if (!good)
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Invalid watch address: %1").arg(text));
    return;
  }

  const QString name = QStringLiteral("mem_%1").arg(address, 8, 16, QLatin1Char('0'));
  AddWatch(name, address);
}

void WatchWidget::OnLoad()
{
  Common::IniFile ini;

  std::vector<std::string> watches;

  if (!ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
                false))
  {
    return;
  }

  Core::CPUThreadGuard guard(m_system);

  if (ini.GetLines("Watches", &watches, false))
  {
    auto& debug_interface = guard.GetSystem().GetPowerPC().GetDebugInterface();
    for (const auto& watch : debug_interface.GetWatches())
    {
      debug_interface.UnsetPatch(guard, watch.address);
    }
    debug_interface.ClearWatches();
    debug_interface.LoadWatchesFromStrings(watches);
  }

  Update();
}

void WatchWidget::OnSave()
{
  Common::IniFile ini;
  ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
           false);
  ini.SetLines("Watches", m_system.GetPowerPC().GetDebugInterface().SaveWatchesToStrings());
  ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini");
}

void WatchWidget::ShowContextMenu()
{
  QMenu* menu = new QMenu(this);

  if (!m_table->selectedItems().empty())
  {
    const std::size_t count = m_table->selectionModel()->selectedRows().count();
    if (count > 1)
    {
      // i18n: This kind of "watch" is used for watching emulated memory.
      // It's not related to timekeeping devices.
      menu->addAction(tr("&Delete Watches"), this, [this] { DeleteSelectedWatches(); });
      // i18n: This kind of "watch" is used for watching emulated memory.
      // It's not related to timekeeping devices.
      menu->addAction(tr("&Lock Watches"), this, [this] { LockSelectedWatches(); });
      // i18n: This kind of "watch" is used for watching emulated memory.
      // It's not related to timekeeping devices.
      menu->addAction(tr("&Unlock Watches"), this, [this] { UnlockSelectedWatches(); });
    }
    else if (count == 1)
    {
      auto row_variant = m_table->selectedItems()[0]->data(Qt::UserRole);

      if (!row_variant.isNull())
      {
        int row = row_variant.toInt();

        if (row >= 0)
        {
          menu->addAction(tr("Show in Memory"), this, [this, row] { ShowInMemory(row); });
          // i18n: This kind of "watch" is used for watching emulated memory.
          // It's not related to timekeeping devices.
          menu->addAction(tr("&Delete Watch"), this, [this, row] { DeleteWatchAndUpdate(row); });
          menu->addAction(tr("&Add Memory Breakpoint"), this,
                          [this, row] { AddWatchBreakpoint(row); });
        }
      }
    }
  }

  menu->addSeparator();

  menu->addAction(tr("Update"), this, &WatchWidget::Update);

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
    case COLUMN_INDEX_LABEL:
      if (item->text().isEmpty())
        DeleteWatchAndUpdate(row);
      else
        m_system.GetPowerPC().GetDebugInterface().UpdateWatchName(row, item->text().toStdString());
      break;
    case COLUMN_INDEX_ADDRESS:
    case COLUMN_INDEX_HEX:
    case COLUMN_INDEX_DECIMAL:
    {
      bool good;
      const bool column_uses_hex_formatting =
          column == COLUMN_INDEX_ADDRESS || column == COLUMN_INDEX_HEX;
      quint32 value = item->text().toUInt(&good, column_uses_hex_formatting ? 16 : 10);

      if (good)
      {
        Core::CPUThreadGuard guard(m_system);

        auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
        if (column == COLUMN_INDEX_ADDRESS)
        {
          const auto& watch = debug_interface.GetWatch(row);
          debug_interface.UnsetPatch(guard, watch.address);
          debug_interface.UpdateWatchAddress(row, value);
          if (watch.locked)
            LockWatchAddress(guard, value);
        }
        else
        {
          PowerPC::MMU::HostWrite_U32(guard, value, debug_interface.GetWatch(row).address);
        }
      }
      else
      {
        ModalMessageBox::critical(this, tr("Error"), tr("Invalid input provided"));
      }
      break;
    }
    case COLUMN_INDEX_LOCK:
    {
      auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
      debug_interface.UpdateWatchLockedState(row, item->checkState() == Qt::Checked);
      const auto& watch = debug_interface.GetWatch(row);
      Core::CPUThreadGuard guard(m_system);
      if (watch.locked)
        LockWatchAddress(guard, watch.address);
      else
        debug_interface.UnsetPatch(guard, watch.address);
      break;
    }
    }

    Update();
  }
}

void WatchWidget::LockWatchAddress(const Core::CPUThreadGuard& guard, u32 address)
{
  const std::string memory_data_as_string = PowerPC::MMU::HostGetString(guard, address, 4);

  std::vector<u8> bytes;
  for (const char c : memory_data_as_string)
  {
    bytes.push_back(static_cast<u8>(c));
  }

  m_system.GetPowerPC().GetDebugInterface().SetFramePatch(guard, address, bytes);
}

void WatchWidget::DeleteSelectedWatches()
{
  {
    Core::CPUThreadGuard guard(m_system);
    std::vector<int> row_indices;
    for (const auto& index : m_table->selectionModel()->selectedRows())
    {
      const auto* item = m_table->item(index.row(), index.column());
      const auto row_variant = item->data(Qt::UserRole);
      if (row_variant.isNull())
        continue;

      row_indices.push_back(row_variant.toInt());
    }

    // Sort greatest to smallest, so we don't stomp on existing indices
    std::sort(row_indices.begin(), row_indices.end(), std::greater{});
    for (const int row : row_indices)
    {
      DeleteWatch(guard, row);
    }
  }

  Update();
}

void WatchWidget::DeleteWatch(const Core::CPUThreadGuard& guard, int row)
{
  auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
  debug_interface.UnsetPatch(guard, debug_interface.GetWatch(row).address);
  debug_interface.RemoveWatch(row);
}

void WatchWidget::DeleteWatchAndUpdate(int row)
{
  {
    Core::CPUThreadGuard guard(m_system);
    DeleteWatch(guard, row);
  }

  Update();
}

void WatchWidget::AddWatchBreakpoint(int row)
{
  emit RequestMemoryBreakpoint(m_system.GetPowerPC().GetDebugInterface().GetWatch(row).address);
}

void WatchWidget::ShowInMemory(int row)
{
  emit ShowMemory(m_system.GetPowerPC().GetDebugInterface().GetWatch(row).address);
}

void WatchWidget::AddWatch(QString name, u32 addr)
{
  m_system.GetPowerPC().GetDebugInterface().SetWatch(addr, name.toStdString());
  Update();
}

void WatchWidget::LockSelectedWatches()
{
  {
    Core::CPUThreadGuard guard(m_system);
    auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
    for (const auto& index : m_table->selectionModel()->selectedRows())
    {
      const auto* item = m_table->item(index.row(), index.column());
      const auto row_variant = item->data(Qt::UserRole);
      if (row_variant.isNull())
        continue;
      const int row = row_variant.toInt();
      const auto& watch = debug_interface.GetWatch(row);
      if (watch.locked)
        continue;
      debug_interface.UpdateWatchLockedState(row, true);
      LockWatchAddress(guard, watch.address);
    }
  }

  Update();
}

void WatchWidget::UnlockSelectedWatches()
{
  {
    auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
    Core::CPUThreadGuard guard(m_system);
    for (const auto& index : m_table->selectionModel()->selectedRows())
    {
      const auto* item = m_table->item(index.row(), index.column());
      const auto row_variant = item->data(Qt::UserRole);
      if (row_variant.isNull())
        continue;
      const int row = row_variant.toInt();
      const auto& watch = debug_interface.GetWatch(row);
      if (!watch.locked)
        continue;
      debug_interface.UpdateWatchLockedState(row, false);
      debug_interface.UnsetPatch(guard, watch.address);
    }
  }

  Update();
}
