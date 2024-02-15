// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/ThreadWidget.h"

#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Common/BitUtils.h"
#include "Core/Core.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

ThreadWidget::ThreadWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Threads"));
  setObjectName(QStringLiteral("threads"));

  setHidden(!Settings::Instance().IsThreadsVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("threadwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("threadwidget/floating")).toBool());

  ConnectWidgets();

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &ThreadWidget::Update);

  connect(&Settings::Instance(), &Settings::ThreadsVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsThreadsVisible());
  });
}

ThreadWidget::~ThreadWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("threadwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("threadwidget/floating"), isFloating());
}

void ThreadWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetThreadsVisible(false);
}

void ThreadWidget::showEvent(QShowEvent* event)
{
  Update();
}

void ThreadWidget::CreateWidgets()
{
  m_state = new QGroupBox(tr("State"));

  auto* state_layout = new QHBoxLayout;
  m_state->setLayout(state_layout);
  state_layout->addWidget(CreateContextGroup());
  state_layout->addWidget(CreateActiveThreadQueueGroup());
  state_layout->setContentsMargins(2, 2, 2, 2);
  state_layout->setSpacing(1);

  auto* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);
  layout->addWidget(m_state);
  layout->addWidget(CreateThreadGroup());
  layout->addWidget(CreateThreadContextGroup());
  layout->addWidget(CreateThreadCallstackGroup());
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
  setWidget(widget);

  Update();
  UpdateThreadContext({});
}

void ThreadWidget::ConnectWidgets()
{
  connect(m_thread_table->selectionModel(), &QItemSelectionModel::selectionChanged,
          [this](const QItemSelection& selected, const QItemSelection& deselected) {
            const auto indexes = selected.indexes();
            const int row = indexes.empty() ? -1 : indexes.first().row();
            OnSelectionChanged(row);
          });
  connect(m_context_table, &QTableWidget::customContextMenuRequested,
          [this] { ShowContextMenu(m_context_table); });
  connect(m_callstack_table, &QTableWidget::customContextMenuRequested,
          [this] { ShowContextMenu(m_callstack_table); });
}

void ThreadWidget::ShowContextMenu(QTableWidget* table)
{
  const auto* item = static_cast<QTableWidgetItem*>(table->currentItem());
  if (item == nullptr)
    return;

  bool ok;
  const u32 addr = item->text().toUInt(&ok, 16);
  if (!ok)
    return;

  QMenu* menu = new QMenu(this);
  const QString watch_name = QStringLiteral("thread_context_%1").arg(addr, 8, 16, QLatin1Char('0'));
  menu->addAction(tr("Add &breakpoint"), this, [this, addr] { emit RequestBreakpoint(addr); });
  menu->addAction(tr("Add memory breakpoint"), this,
                  [this, addr] { emit RequestMemoryBreakpoint(addr); });
  menu->addAction(tr("Add to &watch"), this,
                  [this, addr, watch_name] { emit RequestWatch(watch_name, addr); });
  menu->addAction(tr("View &memory"), this, [this, addr] { emit RequestViewInMemory(addr); });
  menu->addAction(tr("View &code"), this, [this, addr] { emit RequestViewInCode(addr); });
  menu->exec(QCursor::pos());
}

QLineEdit* ThreadWidget::CreateLineEdit() const
{
  QLineEdit* line_edit = new QLineEdit(QStringLiteral("00000000"));
  line_edit->setReadOnly(true);
  line_edit->setFixedWidth(
      line_edit->fontMetrics().boundingRect(QStringLiteral(" 00000000 ")).width());
  return line_edit;
}

QGroupBox* ThreadWidget::CreateContextGroup()
{
  QGroupBox* context_group = new QGroupBox(tr("Thread context"));
  QGridLayout* context_layout = new QGridLayout;
  context_group->setLayout(context_layout);
  context_layout->addWidget(new QLabel(tr("Current context")), 0, 0);
  m_current_context = CreateLineEdit();
  context_layout->addWidget(m_current_context, 0, 1);
  context_layout->addWidget(new QLabel(tr("Current thread")), 1, 0);
  m_current_thread = CreateLineEdit();
  context_layout->addWidget(m_current_thread, 1, 1);
  context_layout->addWidget(new QLabel(tr("Default thread")), 2, 0);
  m_default_thread = CreateLineEdit();
  context_layout->addWidget(m_default_thread, 2, 1);
  context_layout->setSpacing(1);
  return context_group;
}

QGroupBox* ThreadWidget::CreateActiveThreadQueueGroup()
{
  QGroupBox* thread_queue_group = new QGroupBox(tr("Active thread queue"));
  auto* thread_queue_layout = new QGridLayout;
  thread_queue_group->setLayout(thread_queue_layout);
  thread_queue_layout->addWidget(new QLabel(tr("Head")), 0, 0);
  m_queue_head = CreateLineEdit();
  thread_queue_layout->addWidget(m_queue_head, 0, 1);
  thread_queue_layout->addWidget(new QLabel(tr("Tail")), 1, 0);
  m_queue_tail = CreateLineEdit();
  thread_queue_layout->addWidget(m_queue_tail, 1, 1);
  thread_queue_layout->setSpacing(1);
  return thread_queue_group;
}

QGroupBox* ThreadWidget::CreateThreadGroup()
{
  QGroupBox* thread_group = new QGroupBox(tr("Active threads"));
  QGridLayout* thread_layout = new QGridLayout;
  thread_group->setLayout(thread_layout);

  m_thread_table = new QTableWidget();
  QStringList header{tr("Address"),
                     tr("State"),
                     tr("Detached"),
                     tr("Suspended"),
                     QStringLiteral("%1\n(%2)").arg(tr("Base priority"), tr("Effective priority")),
                     QStringLiteral("%1\n%2").arg(tr("Stack end"), tr("Stack start")),
                     tr("errno"),
                     tr("Specific")};
  m_thread_table->setColumnCount(header.size());

  m_thread_table->setHorizontalHeaderLabels(header);
  m_thread_table->setTabKeyNavigation(false);
  m_thread_table->verticalHeader()->setVisible(false);
  m_thread_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_thread_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_thread_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_thread_table->setWordWrap(false);

  thread_layout->addWidget(m_thread_table, 0, 0);
  thread_layout->setSpacing(1);
  return thread_group;
}

QGroupBox* ThreadWidget::CreateThreadContextGroup()
{
  QGroupBox* thread_context_group = new QGroupBox(tr("Selected thread context"));
  QGridLayout* thread_context_layout = new QGridLayout;
  thread_context_group->setLayout(thread_context_layout);

  m_context_table = new QTableWidget();
  m_context_table->setColumnCount(8);  // GPR, FPR, PSF, (GQR+others)
  m_context_table->setRowCount(32);
  m_context_table->setTabKeyNavigation(false);
  m_context_table->horizontalHeader()->setVisible(false);
  m_context_table->verticalHeader()->setVisible(false);
  m_context_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_context_table->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_context_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_context_table->setContextMenuPolicy(Qt::CustomContextMenu);

  thread_context_layout->addWidget(m_context_table, 0, 0);
  thread_context_layout->setSpacing(1);
  return thread_context_group;
}

QGroupBox* ThreadWidget::CreateThreadCallstackGroup()
{
  QGroupBox* thread_callstack_group = new QGroupBox(tr("Selected thread callstack"));
  QGridLayout* thread_callstack_layout = new QGridLayout;
  thread_callstack_group->setLayout(thread_callstack_layout);

  m_callstack_table = new QTableWidget();
  QStringList header{tr("Address"), tr("Back Chain"), tr("LR Save"), tr("Description")};
  m_callstack_table->setColumnCount(header.size());

  m_callstack_table->setHorizontalHeaderLabels(header);
  m_callstack_table->setRowCount(0);
  m_callstack_table->setTabKeyNavigation(false);
  m_callstack_table->verticalHeader()->setVisible(false);
  m_callstack_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_callstack_table->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_callstack_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_callstack_table->setContextMenuPolicy(Qt::CustomContextMenu);

  thread_callstack_layout->addWidget(m_callstack_table, 0, 0);
  thread_callstack_layout->setSpacing(1);
  return thread_callstack_group;
}

void ThreadWidget::Update()
{
  if (!isVisible())
    return;

  const auto emu_state = Core::GetState();
  if (emu_state == Core::State::Stopping)
  {
    m_thread_table->setRowCount(0);
    UpdateThreadContext({});

    Core::CPUThreadGuard guard(Core::System::GetInstance());
    UpdateThreadCallstack(guard, {});
  }
  if (emu_state != Core::State::Paused)
    return;

  const auto format_hex = [](u32 value) {
    return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
  };
  const auto format_hex_from = [&format_hex](const Core::CPUThreadGuard& guard, u32 addr) {
    addr =
        PowerPC::MMU::HostIsRAMAddress(guard, addr) ? PowerPC::MMU::HostRead_U32(guard, addr) : 0;
    return format_hex(addr);
  };
  const auto get_state = [](u16 thread_state) {
    QString state_name;
    switch (thread_state)
    {
    case 1:
      state_name = tr("READY");
      break;
    case 2:
      state_name = tr("RUNNING");
      break;
    case 4:
      state_name = tr("WAITING");
      break;
    case 8:
      state_name = tr("MORIBUND");
      break;
    default:
      state_name = tr("UNKNOWN");
    }
    return QStringLiteral("%1 (%2)").arg(QString::number(thread_state), state_name);
  };
  const auto get_priority = [](u16 base, u16 effective) {
    return QStringLiteral("%1 (%2)").arg(QString::number(base), QString::number(effective));
  };
  const auto get_stack = [](u32 end, u32 start) {
    return QStringLiteral("%1\n%2")
        .arg(end, 8, 16, QLatin1Char('0'))
        .arg(start, 8, 16, QLatin1Char('0'));
  };

  {
    Core::CPUThreadGuard guard(Core::System::GetInstance());

    // YAGCD - Section 4.2.1.4 Dolphin OS Globals
    m_current_context->setText(format_hex_from(guard, 0x800000D4));
    m_current_thread->setText(format_hex_from(guard, 0x800000E4));
    m_default_thread->setText(format_hex_from(guard, 0x800000D8));

    m_queue_head->setText(format_hex_from(guard, 0x800000DC));
    m_queue_tail->setText(format_hex_from(guard, 0x800000E0));

    // Thread group
    m_threads = guard.GetSystem().GetPowerPC().GetDebugInterface().GetThreads(guard);

    int i = 0;
    m_thread_table->setRowCount(i);
    for (const auto& thread : m_threads)
    {
      m_thread_table->insertRow(i);
      m_thread_table->setItem(i, 0, new QTableWidgetItem(format_hex(thread->GetAddress())));
      m_thread_table->setItem(i, 1, new QTableWidgetItem(get_state(thread->GetState())));
      m_thread_table->setItem(i, 2, new QTableWidgetItem(QString::number(thread->IsDetached())));
      m_thread_table->setItem(i, 3, new QTableWidgetItem(QString::number(thread->IsSuspended())));
      m_thread_table->setItem(i, 4,
                              new QTableWidgetItem(get_priority(thread->GetBasePriority(),
                                                                thread->GetEffectivePriority())));
      m_thread_table->setItem(
          i, 5, new QTableWidgetItem(get_stack(thread->GetStackEnd(), thread->GetStackStart())));
      m_thread_table->setItem(i, 6, new QTableWidgetItem(QString::number(thread->GetErrno())));
      m_thread_table->setItem(
          i, 7, new QTableWidgetItem(QString::fromStdString(thread->GetSpecific(guard))));
      i += 1;
    }
  }

  m_thread_table->resizeColumnsToContents();
  m_thread_table->resizeRowsToContents();

  // Thread's context group
  UpdateThreadContext({});
}

void ThreadWidget::UpdateThreadContext(const Common::Debug::PartialContext& context)
{
  const auto format_hex = [](const std::optional<u32>& value) {
    if (!value)
      return QString{};
    return QStringLiteral("%1").arg(*value, 8, 16, QLatin1Char('0'));
  };
  const auto format_hex_idx = [](const auto& table, std::size_t index) {
    if (!table || index >= table->size())
      return QString{};
    return QStringLiteral("%1").arg(table->at(index), 8, 16, QLatin1Char('0'));
  };
  const auto format_f64_as_u64_idx = [](const auto& table, std::size_t index) {
    if (!table || index >= table->size())
      return QString{};
    return QStringLiteral("%1").arg(Common::BitCast<u64>(table->at(index)), 16, 16,
                                    QLatin1Char('0'));
  };

  m_context_table->setRowCount(0);
  for (int i = 0; i < 32; i++)
  {
    m_context_table->insertRow(i);
    m_context_table->setItem(i, 0, new QTableWidgetItem(QStringLiteral("GPR%1").arg(i)));
    m_context_table->setItem(i, 1, new QTableWidgetItem(format_hex_idx(context.gpr, i)));
    m_context_table->setItem(i, 2, new QTableWidgetItem(QStringLiteral("FPR%1").arg(i)));
    m_context_table->setItem(i, 3, new QTableWidgetItem(format_f64_as_u64_idx(context.fpr, i)));
    m_context_table->setItem(i, 4, new QTableWidgetItem(QStringLiteral("PSF%1").arg(i)));
    m_context_table->setItem(i, 5, new QTableWidgetItem(format_f64_as_u64_idx(context.psf, i)));

    if (i < 8)
    {
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("GQR%1").arg(i)));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex_idx(context.gqr, i)));
      continue;
    }
    switch (i)
    {
    case 8:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("CR")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.cr)));
      break;
    case 9:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("LR")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.lr)));
      break;
    case 10:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("CTR")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.ctr)));
      break;
    case 11:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("XER")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.xer)));
      break;
    case 12:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("FPSCR")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.fpscr)));
      break;
    case 13:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("SRR0")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.srr0)));
      break;
    case 14:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("SRR1")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.srr1)));
      break;
    case 15:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("DUMMY")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.dummy)));
      break;
    case 16:
      m_context_table->setItem(i, 6, new QTableWidgetItem(QStringLiteral("STATE")));
      m_context_table->setItem(i, 7, new QTableWidgetItem(format_hex(context.state)));
      break;
    default:
      for (int j = 6; j <= 7; j++)
      {
        auto* disabled_item = new QTableWidgetItem();
        auto flags = disabled_item->flags();
        flags &= ~Qt::ItemIsSelectable;
        flags &= ~Qt::ItemIsEditable;
        flags &= ~Qt::ItemIsEnabled;
        disabled_item->setFlags(flags);
        disabled_item->setBackground(Qt::gray);
        m_context_table->setItem(i, j, disabled_item);
      }
    }
  }
  m_context_table->resizeColumnsToContents();
}

void ThreadWidget::UpdateThreadCallstack(const Core::CPUThreadGuard& guard,
                                         const Common::Debug::PartialContext& context)
{
  m_callstack_table->setRowCount(0);

  if (!context.gpr)
    return;

  const auto format_hex = [](u32 value) {
    return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
  };

  u32 sp = context.gpr->at(1);
  for (int i = 0; i < 16; i++)
  {
    if (sp == 0 || sp == 0xffffffff || !PowerPC::MMU::HostIsRAMAddress(guard, sp))
      break;
    m_callstack_table->insertRow(i);
    m_callstack_table->setItem(i, 0, new QTableWidgetItem(format_hex(sp)));
    if (PowerPC::MMU::HostIsRAMAddress(guard, sp + 4))
    {
      const u32 lr_save = PowerPC::MMU::HostRead_U32(guard, sp + 4);
      m_callstack_table->setItem(i, 2, new QTableWidgetItem(format_hex(lr_save)));
      m_callstack_table->setItem(
          i, 3,
          new QTableWidgetItem(QString::fromStdString(
              guard.GetSystem().GetPowerPC().GetDebugInterface().GetDescription(lr_save))));
    }
    else
    {
      m_callstack_table->setItem(i, 2, new QTableWidgetItem(QStringLiteral("--------")));
    }
    sp = PowerPC::MMU::HostRead_U32(guard, sp);
    m_callstack_table->setItem(i, 1, new QTableWidgetItem(format_hex(sp)));
  }
}

void ThreadWidget::OnSelectionChanged(int row)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  Common::Debug::PartialContext context;

  if (row >= 0 && size_t(row) < m_threads.size())
    context = m_threads[row]->GetContext(guard);

  UpdateThreadContext(context);
  UpdateThreadCallstack(guard, context);
}
