// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/BreakpointWidget.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/Debugger/NewBreakpointDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

BreakpointWidget::BreakpointWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Breakpoints"));
  setObjectName(QStringLiteral("breakpoints"));

  setHidden(!Settings::Instance().IsBreakpointsVisible() ||
            !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("breakpointwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("breakpointwidget/floating")).toBool());

  CreateWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [this](Core::State state) {
    UpdateButtonsEnabled();
    if (state == Core::State::Uninitialized)
    {
      PowerPC::breakpoints.Clear();
      PowerPC::memchecks.Clear();
      Update();
    }
  });

  connect(&Settings::Instance(), &Settings::BreakpointsVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsBreakpointsVisible());
  });

  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &BreakpointWidget::UpdateIcons);
  UpdateIcons();
}

BreakpointWidget::~BreakpointWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("breakpointwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("breakpointwidget/floating"), isFloating());
}

void BreakpointWidget::CreateWidgets()
{
  m_toolbar = new QToolBar;
  m_toolbar->setContentsMargins(0, 0, 0, 0);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_table = new QTableWidget;
  m_table->setContentsMargins(0, 0, 0, 0);
  m_table->setColumnCount(5);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->verticalHeader()->hide();

  connect(m_table, &QTableWidget::itemClicked, [this](QTableWidgetItem* item) {
    if (m_table->selectedItems()[0]->row() == item->row() &&
        Core::GetState() == Core::State::Paused)
    {
      auto address = m_table->selectedItems()[0]->data(Qt::UserRole).toUInt();
      emit SelectedBreakpoint(address);
    }
  });

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_toolbar);
  layout->addWidget(m_table);
  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(0);

  m_new = m_toolbar->addAction(tr("New"), this, &BreakpointWidget::OnNewBreakpoint);
  m_delete = m_toolbar->addAction(tr("Delete"), this, &BreakpointWidget::OnDelete);
  m_clear = m_toolbar->addAction(tr("Clear"), this, &BreakpointWidget::OnClear);

  m_load = m_toolbar->addAction(tr("Load"), this, &BreakpointWidget::OnLoad);
  m_save = m_toolbar->addAction(tr("Save"), this, &BreakpointWidget::OnSave);

  m_new->setEnabled(false);
  m_load->setEnabled(false);
  m_save->setEnabled(false);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void BreakpointWidget::UpdateIcons()
{
  m_new->setIcon(Resources::GetScaledThemeIcon("debugger_add_breakpoint"));
  m_delete->setIcon(Resources::GetScaledThemeIcon("debugger_delete"));
  m_clear->setIcon(Resources::GetScaledThemeIcon("debugger_clear"));
  m_load->setIcon(Resources::GetScaledThemeIcon("debugger_load"));
  m_save->setIcon(Resources::GetScaledThemeIcon("debugger_save"));
}

void BreakpointWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetBreakpointsVisible(false);
}

void BreakpointWidget::showEvent(QShowEvent* event)
{
  UpdateButtonsEnabled();
  Update();
}

void BreakpointWidget::UpdateButtonsEnabled()
{
  if (!isVisible())
    return;

  const bool is_initialised = Core::GetState() != Core::State::Uninitialized;
  m_new->setEnabled(is_initialised);
  m_load->setEnabled(is_initialised);
  m_save->setEnabled(is_initialised);
}

void BreakpointWidget::Update()
{
  if (!isVisible())
    return;

  m_table->clear();

  m_table->setHorizontalHeaderLabels(
      {tr("Active"), tr("Type"), tr("Function"), tr("Address"), tr("Flags")});

  int i = 0;
  m_table->setRowCount(i);

  auto create_item = [](const QString string = QStringLiteral("")) {
    QTableWidgetItem* item = new QTableWidgetItem(string);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
  };

  // Breakpoints
  for (const auto& bp : PowerPC::breakpoints.GetBreakPoints())
  {
    m_table->setRowCount(i + 1);

    auto* active = create_item(bp.is_enabled ? tr("on") : QString());

    active->setData(Qt::UserRole, bp.address);

    m_table->setItem(i, 0, active);
    m_table->setItem(i, 1, create_item(QStringLiteral("BP")));

    if (g_symbolDB.GetSymbolFromAddr(bp.address))
    {
      m_table->setItem(i, 2,
                       create_item(QString::fromStdString(g_symbolDB.GetDescription(bp.address))));
    }

    m_table->setItem(i, 3,
                     create_item(QStringLiteral("%1").arg(bp.address, 8, 16, QLatin1Char('0'))));

    m_table->setItem(i, 4, create_item());

    i++;
  }

  // Memory Breakpoints
  for (const auto& mbp : PowerPC::memchecks.GetMemChecks())
  {
    m_table->setRowCount(i + 1);
    auto* active = create_item(mbp.break_on_hit || mbp.log_on_hit ? tr("on") : QString());
    active->setData(Qt::UserRole, mbp.start_address);

    m_table->setItem(i, 0, active);
    m_table->setItem(i, 1, create_item(QStringLiteral("MBP")));

    if (g_symbolDB.GetSymbolFromAddr(mbp.start_address))
    {
      m_table->setItem(
          i, 2, create_item(QString::fromStdString(g_symbolDB.GetDescription(mbp.start_address))));
    }

    if (mbp.is_ranged)
    {
      m_table->setItem(i, 3,
                       create_item(QStringLiteral("%1 - %2")
                                       .arg(mbp.start_address, 8, 16, QLatin1Char('0'))
                                       .arg(mbp.end_address, 8, 16, QLatin1Char('0'))));
    }
    else
    {
      m_table->setItem(
          i, 3, create_item(QStringLiteral("%1").arg(mbp.start_address, 8, 16, QLatin1Char('0'))));
    }

    QString flags;

    if (mbp.is_break_on_read)
      flags.append(QStringLiteral("r"));

    if (mbp.is_break_on_write)
      flags.append(QStringLiteral("w"));

    m_table->setItem(i, 4, create_item(flags));

    i++;
  }
}

void BreakpointWidget::OnDelete()
{
  if (m_table->selectedItems().empty())
    return;

  auto address = m_table->selectedItems()[0]->data(Qt::UserRole).toUInt();

  PowerPC::breakpoints.Remove(address);
  Settings::Instance().blockSignals(true);
  PowerPC::memchecks.Remove(address);
  Settings::Instance().blockSignals(false);

  Update();
}

void BreakpointWidget::OnClear()
{
  PowerPC::debug_interface.ClearAllBreakpoints();
  Settings::Instance().blockSignals(true);
  PowerPC::debug_interface.ClearAllMemChecks();
  Settings::Instance().blockSignals(false);

  m_table->setRowCount(0);
  Update();
}

void BreakpointWidget::OnNewBreakpoint()
{
  NewBreakpointDialog* dialog = new NewBreakpointDialog(this);
  dialog->exec();
}

void BreakpointWidget::OnLoad()
{
  IniFile ini;
  if (!ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
                false))
  {
    return;
  }

  BreakPoints::TBreakPointsStr new_bps;
  if (ini.GetLines("BreakPoints", &new_bps, false))
  {
    PowerPC::breakpoints.Clear();
    PowerPC::breakpoints.AddFromStrings(new_bps);
  }

  MemChecks::TMemChecksStr new_mcs;
  if (ini.GetLines("MemoryBreakPoints", &new_mcs, false))
  {
    PowerPC::memchecks.Clear();
    Settings::Instance().blockSignals(true);
    PowerPC::memchecks.AddFromStrings(new_mcs);
    Settings::Instance().blockSignals(false);
  }

  Update();
}

void BreakpointWidget::OnSave()
{
  IniFile ini;
  ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
           false);
  ini.SetLines("BreakPoints", PowerPC::breakpoints.GetStrings());
  ini.SetLines("MemoryBreakPoints", PowerPC::memchecks.GetStrings());
  ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini");
}

void BreakpointWidget::AddBP(u32 addr)
{
  PowerPC::breakpoints.Add(addr);

  Update();
}

void BreakpointWidget::AddAddressMBP(u32 addr, bool on_read, bool on_write, bool do_log,
                                     bool do_break)
{
  TMemCheck check;

  check.start_address = addr;
  check.end_address = addr;
  check.is_ranged = false;
  check.is_break_on_read = on_read;
  check.is_break_on_write = on_write;
  check.log_on_hit = do_log;
  check.break_on_hit = do_break;

  Settings::Instance().blockSignals(true);
  PowerPC::memchecks.Add(check);
  Settings::Instance().blockSignals(false);

  Update();
}

void BreakpointWidget::AddRangedMBP(u32 from, u32 to, bool on_read, bool on_write, bool do_log,
                                    bool do_break)
{
  TMemCheck check;

  check.start_address = from;
  check.end_address = to;
  check.is_ranged = true;
  check.is_break_on_read = on_read;
  check.is_break_on_write = on_write;
  check.log_on_hit = do_log;
  check.break_on_hit = do_break;

  Settings::Instance().blockSignals(true);
  PowerPC::memchecks.Add(check);
  Settings::Instance().blockSignals(false);

  Update();
}
