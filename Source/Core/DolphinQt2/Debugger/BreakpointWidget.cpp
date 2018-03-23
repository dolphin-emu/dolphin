// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Debugger/BreakpointWidget.h"

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
#include "DolphinQt2/Debugger/NewBreakpointDialog.h"
#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Settings.h"

BreakpointWidget::BreakpointWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Breakpoints"));
  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("breakpointwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("breakpointwidget/floating")).toBool());

  CreateWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [this](Core::State state) {
    if (!Settings::Instance().IsDebugModeEnabled())
      return;

    m_load->setEnabled(Core::IsRunning());
    m_save->setEnabled(Core::IsRunning());
  });

  connect(&Settings::Instance(), &Settings::BreakpointsVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsBreakpointsVisible());
  });

  setHidden(!Settings::Instance().IsBreakpointsVisible() ||
            !Settings::Instance().IsDebugModeEnabled());

  Update();
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
  m_table = new QTableWidget;
  m_table->setColumnCount(5);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->verticalHeader()->hide();

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_toolbar);
  layout->addWidget(m_table);

  AddAction(m_toolbar, tr("New"), this, &BreakpointWidget::OnNewBreakpoint);
  AddAction(m_toolbar, tr("Delete"), this, &BreakpointWidget::OnDelete);
  AddAction(m_toolbar, tr("Clear"), this, &BreakpointWidget::OnClear);

  m_load = AddAction(m_toolbar, tr("Load"), this, &BreakpointWidget::OnLoad);
  m_save = AddAction(m_toolbar, tr("Save"), this, &BreakpointWidget::OnSave);

  m_load->setEnabled(false);
  m_save->setEnabled(false);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void BreakpointWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetBreakpointsVisible(false);
}

void BreakpointWidget::Update()
{
  m_table->clear();

  m_table->setHorizontalHeaderLabels(
      {tr("Active"), tr("Type"), tr("Function"), tr("Address"), tr("Flags")});

  int i = 0;

  auto create_item = [this](const QString string = QStringLiteral("")) {
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
      m_table->setItem(i, 3, create_item(QStringLiteral("%1 - %2")
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
  if (m_table->selectedItems().size() == 0)
    return;

  auto address = m_table->selectedItems()[0]->data(Qt::UserRole).toUInt();

  PowerPC::breakpoints.Remove(address);
  PowerPC::memchecks.Remove(address);

  Update();
}

void BreakpointWidget::OnClear()
{
  PowerPC::debug_interface.ClearAllBreakpoints();
  PowerPC::debug_interface.ClearAllMemChecks();

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
  BreakPoints::TBreakPointsStr newbps;
  MemChecks::TMemChecksStr newmcs;

  if (!ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
                false))
  {
    return;
  }

  if (ini.GetLines("BreakPoints", &newbps, false))
  {
    PowerPC::breakpoints.Clear();
    PowerPC::breakpoints.AddFromStrings(newbps);
  }

  if (ini.GetLines("MemoryBreakPoints", &newmcs, false))
  {
    PowerPC::memchecks.Clear();
    PowerPC::memchecks.AddFromStrings(newmcs);
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
  check.is_break_on_read = on_read;
  check.is_break_on_write = on_write;
  check.log_on_hit = do_log;
  check.break_on_hit = do_break;

  PowerPC::memchecks.Add(check);

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

  PowerPC::memchecks.Add(check);

  Update();
}
