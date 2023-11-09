// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/TraceWidget.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <regex>
#include <vector>

#include <fmt/format.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/Debugger/CodeTrace.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

constexpr int ADDRESS_ROLE = Qt::UserRole;
constexpr int MEM_ADDRESS_ROLE = Qt::UserRole + 1;
constexpr int INDEX_ROLE = Qt::UserRole + 2;

TraceWidget::TraceWidget(QWidget* parent)
    : QDockWidget(parent), m_system(Core::System::GetInstance())
{
  setWindowTitle(tr("Trace"));
  setObjectName(QStringLiteral("trace"));

  setHidden(!Settings::Instance().IsTraceVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("tracewidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("tracewidget/floating")).toBool());

  connect(&Settings::Instance(), &Settings::TraceVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });
  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsTraceVisible()); });

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &TraceWidget::UpdateFont);

  ConnectWidgets();
  UpdateFont();
  UpdateBreakpoints();
}

TraceWidget::~TraceWidget()
{
  auto& settings = Settings::GetQSettings();
  // Not sure if we want to clear it.
  // ClearAll();
  settings.setValue(QStringLiteral("tracewidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("tracewidget/floating"), isFloating());
}

void TraceWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetTraceVisible(false);
}

void TraceWidget::resizeEvent(QResizeEvent* event)
{
  // Basically just for the help message.
  m_output_table->horizontalHeader()->setMaximumSectionSize(m_output_table->width());
  if (m_output_table->columnCount() == 1)
  {
    m_output_table->resizeColumnsToContents();
    m_output_table->resizeRowsToContents();
  }
}

void TraceWidget::UpdateFont()
{
  // Probably don't require font width, because resizeColumnsToContents works here.
  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  m_font_vspace = fm.lineSpacing();
  m_output_table->setFont(Settings::Instance().GetDebugFont());

  if (m_output_table->columnCount() > 1)
    m_output_table->verticalHeader()->setMaximumSectionSize(m_font_vspace);

  m_output_table->resizeColumnsToContents();
  m_output_table->resizeRowsToContents();

  update();
}

const QString TraceWidget::ElideText(const QString& text) const
{
  return fontMetrics().elidedText(text, Qt::ElideRight,
                                  m_record_stop_addr->lineEdit()->rect().width() - 5);
}

void TraceWidget::OnSetColor(QColor* text_color)
{
  *text_color = QColorDialog::getColor(*text_color);
}

void TraceWidget::CreateWidgets()
{
  auto* sidebar = new QWidget;

  // Sidebar top menu
  QMenuBar* menubar = new QMenuBar(sidebar);
  menubar->setNativeMenuBar(false);
  QMenu* menu_actions = new QMenu(tr("&View"));

  menu_actions->addAction(tr("Change color of &tracked items"), this,
                          [this]() { OnSetColor(&m_tracked_color); });
  menu_actions->addAction(tr("Change color of &overwritten items"), this,
                          [this]() { OnSetColor(&m_overwritten_color); });
  menu_actions->addAction(tr("Change color of registers displayed as &values"), this,
                          [this]() { OnSetColor(&m_value_color); });

  menubar->addMenu(menu_actions);
  auto* input_layout = new QVBoxLayout;

  m_record_stop_addr = new QComboBox();
  m_record_stop_addr->setEditable(true);
  m_record_stop_addr->lineEdit()->setPlaceholderText(tr("Stop at BP or address"));
  m_record_btn = new QPushButton(tr("Record Trace"));
  m_record_btn->setCheckable(true);

  auto* record_options_box = new QGroupBox(tr("Recording options"));
  auto* record_options_layout = new QGridLayout;
  QLabel* record_stop_addr_label = new QLabel(tr("Stop recording at:"));
  m_record_limit_label = new QLabel(tr("Timeout (seconds)"));
  m_record_limit_input = new QSpinBox();
  m_record_limit_input->setMinimum(1);
  m_record_limit_input->setMaximum(30);
  m_record_limit_input->setValue(4);
  m_record_limit_input->setSingleStep(1);
  m_record_limit_input->setMinimumSize(40, 0);
  m_clear_on_loop = new QCheckBox(tr("Reset on loopback"));

  record_options_layout->addWidget(m_record_limit_label, 0, 0);
  record_options_layout->addWidget(m_record_limit_input, 0, 1);
  record_options_layout->addWidget(record_stop_addr_label, 1, 0);
  record_options_layout->addWidget(m_record_stop_addr, 2, 0, 1, 2);
  record_options_layout->addWidget(m_clear_on_loop, 3, 0, 1, 2);
  record_options_box->setLayout(record_options_layout);

  auto* results_options_box = new QGroupBox(tr("Result Options"));
  auto* results_options_layout = new QGridLayout;
  m_filter_btn = new QPushButton(tr("Apply and Update"));

  QLabel* trace_target_label = new QLabel(tr("Trace value in"));
  m_trace_target = new QLineEdit();
  m_trace_target->setPlaceholderText(tr("gpr, fpr, mem"));

  m_results_limit_label = new QLabel(tr("Maximum results"));
  m_results_limit_input = new QSpinBox();
  m_results_limit_input->setMinimum(100);
  m_results_limit_input->setMaximum(10000);
  m_results_limit_input->setValue(1000);
  m_results_limit_input->setSingleStep(250);
  m_results_limit_input->setMinimumSize(50, 0);

  m_backtrace = new QCheckBox(tr("Backtrace"));
  m_show_values = new QCheckBox(tr("Show values"));
  m_change_range = new QCheckBox(tr("Change Range"));
  m_change_range->setDisabled(true);

  // Two QLineEdits next to each other aren't managed correctly by QGridLayout, so they are put in a
  // HLayout first.
  auto* range_layout = new QHBoxLayout;
  m_range_start = new QLineEdit();
  m_range_end = new QLineEdit();
  m_range_start->setPlaceholderText(tr("Start From"));
  m_range_end->setPlaceholderText(tr("End At"));
  m_range_start->setDisabled(true);
  m_range_end->setDisabled(true);
  range_layout->addWidget(m_range_start);
  range_layout->addWidget(m_range_end);

  results_options_layout->addWidget(trace_target_label, 0, 0);
  results_options_layout->addWidget(m_trace_target, 0, 1);
  results_options_layout->addWidget(m_results_limit_label, 1, 0);
  results_options_layout->addWidget(m_results_limit_input, 1, 1);
  results_options_layout->addWidget(m_backtrace, 2, 0);
  results_options_layout->addWidget(m_show_values, 2, 1);
  results_options_layout->addWidget(m_change_range, 3, 0);
  results_options_layout->addLayout(range_layout, 4, 0, 1, 2);
  results_options_layout->addWidget(m_filter_btn, 5, 0, 1, 2);

  results_options_box->setLayout(results_options_layout);

  auto* filter_options_box = new QGroupBox(tr("Result Filters"));
  auto* filter_options_layout = new QGridLayout;
  m_filter_overwrite = new QCheckBox(tr("Overwritten"));
  m_filter_move = new QCheckBox(tr("Move"));
  m_filter_loadstore = new QCheckBox(tr("Memory Op"));
  m_filter_pointer = new QCheckBox(tr("Pointer"));
  m_filter_passive = new QCheckBox(tr("Passive"));
  m_filter_active = new QCheckBox(tr("Active"));

  for (auto* action : {m_filter_overwrite, m_filter_active, m_filter_loadstore, m_filter_move,
                       m_filter_pointer, m_filter_passive})
  {
    action->setChecked(true);
  }

  filter_options_layout->addWidget(m_filter_active, 0, 0);
  filter_options_layout->addWidget(m_filter_loadstore, 0, 1);
  filter_options_layout->addWidget(m_filter_passive, 1, 0);
  filter_options_layout->addWidget(m_filter_pointer, 1, 1);
  filter_options_layout->addWidget(m_filter_overwrite, 2, 0);
  filter_options_layout->addWidget(m_filter_move, 2, 1);

  filter_options_box->setLayout(filter_options_layout);

  m_help_btn = new QPushButton(tr("Help"));
  m_help_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  input_layout->setSpacing(1);
  input_layout->addItem(new QSpacerItem(1, 20));
  input_layout->addWidget(m_record_btn);
  input_layout->addWidget(record_options_box);
  input_layout->addItem(new QSpacerItem(1, 32));
  input_layout->addWidget(results_options_box);
  input_layout->addItem(new QSpacerItem(1, 32));
  input_layout->addWidget(filter_options_box);
  input_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
  input_layout->addWidget(m_help_btn);

  m_output_table = new QTableWidget();
  m_output_table->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_output_table->setShowGrid(false);
  m_output_table->verticalHeader()->setVisible(false);
  m_output_table->setColumnCount(7);
  m_output_table->setRowCount(100);
  m_output_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_output_table->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
  m_output_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_output_table->setHorizontalHeaderLabels({tr("Address"), tr("Instr"), tr("Reg"), tr("Arg1"),
                                             tr("Arg2"), tr("Arg3 / Info"), tr("Symbol")});

  auto* sidebar_scroll = new QScrollArea;
  sidebar->setLayout(input_layout);
  sidebar_scroll->setWidget(sidebar);
  sidebar_scroll->setWidgetResizable(true);
  // Need to make this variable without letting the sidebar become too big.
  sidebar_scroll->setFixedWidth(235);

  auto* splitter = new QSplitter(Qt::Horizontal);
  splitter->addWidget(m_output_table);
  splitter->addWidget(sidebar_scroll);

  auto* layout = new QHBoxLayout();
  auto* widget = new QWidget;
  layout->addWidget(splitter);
  widget->setLayout(layout);
  setWidget(widget);
  update();

  if (Core::GetState() != Core::State::Paused)
    DisableButtons(true);
}

void TraceWidget::ConnectWidgets()
{
  // Capturing the state was causing bugs with the wrong state being reported.
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this]() { DisableButtons(Core::GetState() != Core::State::Paused); });
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          [this] { DisableButtons(Core::GetState() != Core::State::Paused); });
  connect(m_help_btn, &QPushButton::pressed, this, &TraceWidget::InfoDisp);
  connect(m_record_btn, &QPushButton::clicked, [this](bool record) {
    if (record)
      OnRecordTrace();
    else
      ClearAll();
  });
  connect(m_filter_btn, &QPushButton::clicked, this, &TraceWidget::DisplayTrace);
  connect(m_change_range, &QCheckBox::stateChanged, this, [this](bool checked) {
    m_range_start->setEnabled(checked);
    m_range_end->setEnabled(checked);
  });
  // When clicking on an item, we want the code widget to update without hiding the trace widget.
  // Useful when both widgets are visible. There's also a right-click option to switch to the code
  // tab.
  connect(m_output_table, &QTableWidget::itemClicked, [this](QTableWidgetItem* item) {
    if (m_record_btn->isChecked() && item->column() == 0)
    {
      emit ShowCode(item->data(ADDRESS_ROLE).toUInt());
      raise();
      activateWindow();
    }
  });
  connect(m_output_table, &TraceWidget::customContextMenuRequested, this,
          &TraceWidget::OnContextMenu);
}

void TraceWidget::DisableButtons(bool enabled)
{
  // Will queue one click on a disabled button without this.
  qApp->processEvents();

  m_record_btn->setDisabled(enabled);
  m_filter_btn->setDisabled(enabled);

  // Required for UI update if function takes a long time to execute.
  repaint();
  qApp->processEvents();
}

void TraceWidget::ClearAll()
{
  std::vector<TraceOutput>().swap(m_code_trace);
  m_output_table->clear();
  m_output_table->setHorizontalHeaderLabels({tr("Address"), tr("Instr"), tr("Reg"), tr("Arg1"),
                                             tr("Arg2"), tr("Arg3 / Info"), tr("Symbol")});
  m_output_table->setWordWrap(true);
  m_record_stop_addr->setEnabled(true);
  m_record_stop_addr->setCurrentText(tr("Stop at BP or address"));
  m_change_range->setChecked(false);
  m_change_range->setDisabled(true);
  m_record_btn->setText(tr("Record Trace"));
  m_record_btn->setChecked(false);
  m_record_limit_input->setDisabled(false);
  m_record_limit_label->setText(tr("Maximum to record"));
  m_results_limit_label->setText(tr("Maximum results"));
  UpdateBreakpoints();
}

void TraceWidget::LogCreated(std::optional<QString> target_register)
{
  const u32 end_addr = m_code_trace.back().address;
  QString instr = QString::fromStdString(m_code_trace.back().instruction);
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));

  m_record_stop_addr->insertItem(
      0,
      ElideText(QStringLiteral("End %1 : %2").arg((end_addr), 8, 16, QLatin1Char('0')).arg(instr)),
      end_addr);
  m_record_stop_addr->setCurrentIndex(0);

  // Update UI
  m_record_stop_addr->setDisabled(true);
  m_change_range->setEnabled(true);
  m_record_btn->setChecked(true);
  m_record_btn->setText(tr("Delete log and reset"));
  m_record_limit_input->setDisabled(true);
  m_output_table->setWordWrap(false);

  // Set register from autostepping
  if (target_register)
    m_trace_target->setText(target_register.value());

  m_running = false;
  DisplayTrace();
}

u32 TraceWidget::GetVerbosity() const
{
  u32 flags = 0;
  if (m_filter_overwrite->isChecked())
    flags |= static_cast<u32>(HitType::OVERWRITE);
  if (m_filter_move->isChecked())
    flags |= static_cast<u32>(HitType::MOVED);
  if (m_filter_pointer->isChecked())
    flags |= static_cast<u32>(HitType::POINTER);
  if (m_filter_loadstore->isChecked())
    flags |= static_cast<u32>(HitType::LOADSTORE);
  if (m_filter_passive->isChecked())
    flags |= static_cast<u32>(HitType::PASSIVE);
  if (m_filter_active->isChecked())
    flags |= static_cast<u32>(HitType::ACTIVE) | static_cast<u32>(HitType::UPDATED);

  return flags;
}

void TraceWidget::AutoStep(CodeTrace::AutoStop option, const std::string reg)
{
  // Autosteps and follows value in the target (left-most) register. The Used and Changed options
  // silently follows target through reshuffles in memory and registers and stops on use or update.
  // Note: This records a log of all instructions executed, which is later re-processed to display
  // the register trace. No log is created if one already exists.

  if (!m_system.GetCPU().IsStepping() || m_running)
    return;

  DisableButtons(true);
  m_running = true;

  CodeTrace code_trace;
  bool repeat = false;
  const bool log_trace = m_code_trace.empty();
  auto* log_output = log_trace ? &m_code_trace : nullptr;
  const QString target_register = QString::fromStdString(reg);
  code_trace.SetRegTracked(reg);

  QMessageBox msgbox(QMessageBox::NoIcon, tr("Run until"), {}, QMessageBox::Cancel);
  QPushButton* run_button = msgbox.addButton(tr("Keep Running"), QMessageBox::AcceptRole);
  // Not sure if we want default to be cancel. Spacebar can let you quickly continue autostepping if
  // Yes.

  // Does this go inside the loop?
  Core::CPUThreadGuard guard(m_system);

  do
  {
    // Run autostep then update codeview
    const AutoStepResults results = code_trace.AutoStepping(guard, log_output, repeat, option);
    emit Host::GetInstance()->UpdateDisasmDialog();
    repeat = true;

    // Invalid instruction, 0 means no step executed.
    if (results.count == 0)
      break;

    // Status report
    if (results.reg_tracked.empty() && results.mem_tracked.empty())
    {
      QMessageBox::warning(
          this, tr("Overwritten"),
          tr("Target value was overwritten by current instruction.\nInstructions executed:   %1")
              .arg(QString::number(results.count)),
          QMessageBox::Cancel);
      break;
    }
    else if (results.timed_out)
    {
      // Can keep running and try again after a time out.
      // Issue: if ran from register widget, will lose first_hit logic on continue.
      msgbox.setText(
          tr("<font color='#ff0000'>AutoStepping timed out. Current instruction is irrelevant."));
    }
    else
    {
      msgbox.setText(tr("Value tracked to current instruction."));
    }

    // Mem_tracked needs to track each byte individually, so a tracked word-sized value would have
    // four entries. The displayed memory list needs to be shortened so it's not a huge list of
    // bytes. Assumes adjacent bytes represent a word or half-word and removes the redundant bytes.
    std::set<u32> mem_out;
    auto iter = results.mem_tracked.begin();

    while (iter != results.mem_tracked.end())
    {
      const u32 address = *iter;
      mem_out.insert(address);

      for (u32 i = 1; i <= 3; i++)
      {
        if (results.mem_tracked.count(address + i))
          iter++;
        else
          break;
      }

      iter++;
    }

    const QString msgtext =
        tr("Instructions executed:   %1\nValue contained in:\nRegisters:   %2\nMemory:   %3")
            .arg(QString::number(results.count))
            .arg(QString::fromStdString(fmt::format("{}", fmt::join(results.reg_tracked, ", "))))
            .arg(QString::fromStdString(fmt::format("{:#x}", fmt::join(mem_out, ", "))));

    msgbox.setInformativeText(msgtext);
    msgbox.exec();

  } while (msgbox.clickedButton() == (QAbstractButton*)run_button);

  if (log_trace)
  {
    LogCreated(target_register);
  }
  else
  {
    DisableButtons(false);
    m_running = false;
  }
}

void TraceWidget::OnRecordTrace()
{
  m_record_btn->setChecked(false);

  if (!m_system.GetCPU().IsStepping() || m_running)
    return;

  DisableButtons(true);
  m_running = true;

  // Try to get end_bp based on editable input text, then on combo box selection.
  bool good;
  u32 end_bp = m_record_stop_addr->currentText().toUInt(&good, 16);
  if (!good)
    end_bp = m_record_stop_addr->currentData().toUInt(&good);
  if (!good)
    end_bp = 0;

  CodeTrace codetrace;

  Core::CPUThreadGuard guard(m_system);
  bool timed_out = codetrace.RecordTrace(guard, &m_code_trace, m_record_limit_input->value(),
                                         end_bp, m_clear_on_loop->isChecked());
  emit Host::GetInstance()->UpdateDisasmDialog();

  // Errors
  m_error_msg.clear();

  if (m_code_trace.empty())
    m_error_msg.emplace_back(tr("Record failed to run."));
  else if (timed_out)
    m_error_msg.emplace_back(tr("Record trace ran out of time. Backtrace won't be correct."));

  LogCreated();
}

std::vector<TraceResults> TraceWidget::CodePath(const u32 start, u32 range, const bool backtrace)
{
  // Shows entire trace (limited by results limit) without filtering if target input is blank.
  std::vector<TraceResults> tmp_results;
  const u32 result_limit = m_results_limit_input->value();

  if (range > result_limit)
  {
    range = result_limit - 1;
    m_error_msg.emplace_back(tr("Output exceeds results limit. Not all lines will be shown."));
  }

  u32 index = start;
  for (u32 i = 0; i <= range; i++)
  {
    tmp_results.emplace_back(TraceResults{index, HitType::ACTIVE, {}});
    backtrace ? index-- : index++;
  }

  return tmp_results;
}

std::vector<TraceResults> TraceWidget::MakeTraceFromLog()
{
  u32 start = 0;
  const u32 size = static_cast<u32>(m_code_trace.size());
  u32 end = size - 1;

  if (m_change_range->isChecked())
  {
    // Returns 0 if invalid or unset, which is effectively correct
    start = GetCustomIndex(m_range_start->text());
    bool find_last = true;
    end = GetCustomIndex(m_range_end->text(), find_last);

    if (end <= start)
    {
      end = size - 1;
      m_error_msg.emplace_back(tr("Custom range places the end before the start. Ignoring."));
    }
  }

  const bool backtrace = m_backtrace->isChecked();
  const u32 range = end - start;
  if (backtrace)
    std::swap(start, end);

  // If no trace target, display the recorded log without tracing
  if (m_trace_target->text().isEmpty())
    return CodePath(start, range, backtrace);

  CodeTrace codetrace;

  // Setup memory or register to track
  if (m_trace_target->text().size() == 8)
  {
    bool ok;
    const u32 mem = m_trace_target->text().toUInt(&ok, 16);

    if (!ok)
    {
      m_error_msg.emplace_back(tr("Memory Address input error"));
      return std::vector<TraceResults>();
    }

    codetrace.SetMemTracked(mem);
  }
  else if (m_trace_target->text().size() < 5)
  {
    QString reg_tmp = m_trace_target->text();
    reg_tmp.replace(QStringLiteral("sp"), QStringLiteral("r1"), Qt::CaseInsensitive);
    reg_tmp.replace(QStringLiteral("rtoc"), QStringLiteral("r2"), Qt::CaseInsensitive);
    codetrace.SetRegTracked(reg_tmp.toStdString());
  }
  else
  {
    m_error_msg.emplace_back(tr("Target register or memory input error"));
    return std::vector<TraceResults>();
  }

  // Loop the log through TraceLogic to follow the target. Does not need to be efficient or fast.
  std::vector<TraceResults> trace_results;
  HitType type;
  const u32 verbosity_flags = GetVerbosity();
  const u32 results_limit = m_results_limit_input->value();
  u32 result_count = 0;

  // If the first instance of a tracked target is it being destroyed, we probably wanted to track
  // it from that point onwards. Make the first hit a special exclusion case.
  bool first_hit = true;
  u32 index = start;

  for (u32 i = 0; i <= range; i++)
  {
    const auto& current = m_code_trace[index];

    TraceResults tmp_results;
    type = codetrace.TraceLogic(current, first_hit, &tmp_results.regs, backtrace);

    if (type != HitType::SKIP && type != HitType::STOP)
    {
      if (static_cast<u32>(type) & verbosity_flags)
      {
        tmp_results.index = index;
        tmp_results.type = type;
        trace_results.push_back(std::move(tmp_results));
        result_count++;
      }
      first_hit = false;
    }

    // Stop if we run out of things to track
    if (type == HitType::STOP || result_count >= results_limit)
      break;

    backtrace ? index-- : index++;
  }

  return trace_results;
}

void TraceWidget::DisplayTrace()
{
  if (m_code_trace.empty() || m_running)
    return;

  DisableButtons(true);
  m_running = true;

  // Clear old data
  m_output_table->setRowCount(0);

  const std::vector<TraceResults> trace_results = MakeTraceFromLog();

  m_error_msg.emplace_back(
      QStringLiteral("Recorded %1 instructions").arg(QString::number(m_code_trace.size())));

  if (trace_results.size() >= static_cast<size_t>(m_results_limit_input->value()))
    m_error_msg.emplace_back(tr("Max table size reached."));

  // Update UI
  m_results_limit_label->setText(
      QStringLiteral("Results: %1 of").arg(QString::number(trace_results.size())));

  // The {num, num} may not be required.
  std::regex reg("(\\S*)\\s+(?:(\\S{0,12})\\s*)?(?:(\\S{0,8})\\s*)?(?:(\\S{0,8})\\s*)?(.*)");

  std::smatch match;
  m_output_table->setRowCount(static_cast<int>(trace_results.size() + 1));

  int row = 0;

  for (auto& result : trace_results)
  {
    auto& out = m_code_trace[result.index];
    const QString fix_sym = QString::fromStdString(g_symbolDB.GetDescription(out.address))
                                .replace(QStringLiteral("\t"), QStringLiteral("  "));

    std::regex_search(out.instruction, match, reg);

    // Fixup if needed
    std::string match4 = match.str(4);
    std::string str_end;

    if (out.memory_target)
    {
      if (!m_show_values->isChecked())
        str_end = fmt::format("{:08x}", *out.memory_target);

      // There's an extra comma for psq read/writes.
      if (match4.find(',') != std::string::npos)
        match4.pop_back();
    }
    else
    {
      str_end = match.str(5);
    }

    auto* addr_item =
        new QTableWidgetItem(QString::fromStdString(fmt::format(" {:08x} ", out.address)));
    auto* instr_item =
        new QTableWidgetItem(QString::fromStdString(fmt::format("{:<7}", match.str(1))));
    auto* target_item =
        new QTableWidgetItem(QString::fromStdString(fmt::format("{:<5}", match.str(2))));
    auto* arg1_item =
        new QTableWidgetItem(QString::fromStdString(fmt::format("{:<5}", match.str(3))));
    auto* arg2_item =
        new QTableWidgetItem(QString::fromStdString(fmt::format("{:<5}", match.str(4))));
    auto* arg3_item = new QTableWidgetItem(QString::fromStdString(fmt::format("{: <10}", str_end)));
    auto* sym_item = new QTableWidgetItem(fix_sym);

    // Allow ->0x80123456 to overflow into next column.
    if (match.str(2).length() == 12)
      m_output_table->setSpan(row, 2, 1, 2);

    // Colors and show values.
    for (auto* item : {target_item, arg1_item, arg2_item, arg3_item})
    {
      bool colored = false;

      for (auto& r : result.regs)
      {
        const int index = item->text().indexOf(QString::fromStdString(r));

        // Be careful r2 doesn't match r21.
        if (index != -1)
        {
          if ((r.length() == 3 || !item->text()[index + 2].isDigit()))
          {
            colored = true;
            if (result.type == HitType::OVERWRITE && item == target_item)
              item->setForeground(m_overwritten_color);
            else
              item->setForeground(m_tracked_color);
          }
        }
      }

      if (m_show_values->isChecked())
      {
        // Replace registers with values.
        for (auto& regval : out.regdata)
        {
          const int index = item->text().indexOf(QString::fromStdString(regval.reg));
          const size_t length = regval.reg.length();

          // Skip if r2 matches r21
          if (index != -1 && (length == 3 || !item->text()[index + length].isDigit()))
          {
            if (item->text()[index] == QLatin1Char('f') || item->text()[index] == QLatin1Char('p'))
            {
              // If nan, could display hex, but would be unsure if it's a double or single float.
              // Displaying a double would be too long probably.
              const QString value = QString::number(regval.value, 'g', 4);
              item->setText(item->text().replace(index, length, value));
            }
            else
            {
              // Assuming hex.
              const QString value = QString::number(static_cast<u32>(regval.value), 16);
              item->setText(item->text().replace(index, length, value));
            }
            if (!colored)
              item->setForeground(m_value_color);
          }
        }
      }

      // Align to first digit.
      if (!item->text().startsWith(QLatin1Char('-')))
        item->setText(item->text().prepend(QLatin1Char(' ')));
    }

    // Just manage all data in column 0. Add 1 to index so position 0 reads as 1 in the UI. Possibly
    // move +1 to menu actions if index data is used for other things.
    addr_item->setData(ADDRESS_ROLE, out.address);
    addr_item->setData(INDEX_ROLE, result.index + 1);
    if (out.memory_target)
      addr_item->setData(MEM_ADDRESS_ROLE, *out.memory_target);

    m_output_table->setItem(row, 0, addr_item);
    m_output_table->setItem(row, 1, instr_item);
    m_output_table->setItem(row, 2, target_item);
    m_output_table->setItem(row, 3, arg1_item);
    m_output_table->setItem(row, 4, arg2_item);
    m_output_table->setItem(row, 5, arg3_item);
    m_output_table->setItem(row, 6, sym_item);

    row++;
  }

  // Unlike other TableWidgets, new rows will not be created while scrolling, so resizing to
  // contents works here.
  m_output_table->resizeColumnsToContents();

  // ResizeColumnsToContents() has problems with sizing spans, so adding error msg last.
  for (auto& error : m_error_msg)
  {
    auto* error_item = new QTableWidgetItem(error);
    m_output_table->insertRow(0);
    m_output_table->setItem(0, 0, error_item);
    m_output_table->setSpan(0, 0, 1, m_output_table->columnCount());
    row++;
  }

  m_error_msg.clear();
  m_output_table->resizeRowsToContents();
  m_output_table->verticalHeader()->setMaximumSectionSize(m_font_vspace);
  DisableButtons(false);
  m_running = false;
}

void TraceWidget::UpdateBreakpoints()
{
  // Leave the recorded end intact.
  if (m_record_btn->isChecked())
  {
    for (int i = m_record_stop_addr->count(); i > 1; i--)
      m_record_stop_addr->removeItem(1);
    return;
  }
  else
  {
    m_record_stop_addr->clear();
  }
  auto& power_pc = m_system.GetPowerPC();
  auto& breakpoints = power_pc.GetBreakPoints();
  Core::CPUThreadGuard guard(m_system);

  // Allow no breakpoint
  m_record_stop_addr->addItem(QStringLiteral(""));

  int index = 0;
  for (const auto& bp : breakpoints.GetBreakPoints())
  {
    QString instr =
        QString::fromStdString(power_pc.GetDebugInterface().Disassemble(&guard, bp.address));
    instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
    m_record_stop_addr->addItem(
        ElideText(QStringLiteral("%1 : %2").arg(bp.address, 8, 16, QLatin1Char('0')).arg(instr)),
        bp.address);
    index++;
  }

  // User typically wants the most recently placed breakpoint.
  m_record_stop_addr->setCurrentIndex(index);
}

u32 TraceWidget::GetCustomIndex(const QString& str, const bool find_last)
{
  // #number input starts and ends from those lines of the recording. Inputting addresses searches
  // for the first/last occurrence of the address for start/end.
  u32 index = 0;

  if (!str.isEmpty())
  {
    bool good;

    if (str.startsWith(QLatin1Char('#')))
    {
      index = str.mid(1).toUInt(&good, 10);
      good = (index <= m_code_trace.size()) && good;

      // index 1 in the UI is position 0. TraceResults stores position.
      if (index > 0)
        index--;
    }
    else if (find_last)
    {
      // Find last occurrence
      u32 addr = str.toUInt(&good, 16);
      auto it = find_if(m_code_trace.rbegin(), m_code_trace.rend(), [addr](const TraceOutput& t) {
                  return t.address == addr;
                }).base();
      good = it != m_code_trace.begin();
      index = it - m_code_trace.begin();
    }
    else
    {
      u32 addr = str.toUInt(&good, 16);
      auto it = find_if(m_code_trace.begin(), m_code_trace.end(),
                        [addr](const TraceOutput& line) { return line.address == addr; });
      good = it != m_code_trace.end();
      index = it - m_code_trace.begin();
    }

    if (!good)
    {
      m_error_msg.emplace_back(tr("Input error with starting address."));
      index = 0;
    }
  }

  return index;
}

void TraceWidget::InfoDisp()
{
  ModalMessageBox::information(
      this, tr("Trace Widget Help"),
      tr("Provides recording and filtering of executed instructions.  Filtering is completely "
         "separate from recording; it won't affect what's recorded and it can be applied then "
         "removed without losing any recorded data. Filtering can trace a value starting in a "
         "register through multiple registers and memory addresses.\n"
         "Recording a normal, forward trace:\n"
         "Example: Paused on ' mr  r23, r3 '. Trace where the value in r3 is going\n"
         "\n"
         "Simple:\n"
         "Go to register widget, right click r3, and click Run until...\n"
         "Value is hit: next occurrence of value showing up.\n"
         "Value is used: value does something other than move around (math, compare, pointer).\n"
         "Value is changed:  Value either gets updated or overwritten.\n"
         "Information will also appear in the Trace Widget.\n"
         "\n"
         "Using the Trace Widget, where it can record multiple hits without stopping:\n"
         "1. Pause the emulator on the instruction.\n"
         "1b. (optional) Set a breakpoint where the recording should stop. Select it in the "
         "\"Stop Recording\" box. If not set, it will run until a limit is reached.\n"
         "2. Hit Record Trace. Once it finishes, the recording log is locked in and won't change "
         "until \"Delete Log and Reset\" is clicked.\n"
         "3. This step can be done at any time. Place r3 in the Trace Value box, then click "
         "\"Apply and update\" if needed. Instructions where the value in r3 show up in bold. As "
         "it is moving between registers, the register number may change, and it may show up in "
         "RAM addresses too.\n"
         "\n"
         "Backtracing:\n"
         "Example: r3 = r4 + r5;  Find where the value in r5 came from. To do this, earlier "
         "instructions must be recorded.\n"
         "\n"
         "1. Set a breakpoint at the instruction to backtrace. Select it in \"Stop recording "
         "at\". "
         "With a backtrace, the target instruction must be the last recorded.\n"
         "2. Set a breakpoint somewhere earlier than the instruction, where the recording will "
         "likely be able to capture instructions related to r5. Usually going back 1-3 times in "
         "the Callstack (code widget) and breakpointing on the branch will work.\n"
         "3. Pause the game on the breakpoint in step 2.\n"
         "4. Record Trace. If successful, the game will pause on the instruction Breakpoint in "
         "step 1.\n"
         "4b. If the game is not stopped on the instruction to backtrace, then too many "
         "instructions caused the trace to stop early. Return to step 2 and set a closer "
         "breakpoint.\n"
         "5. This step can be done at any time. Check Backtrace in the filter menu, and input r5 "
         "in \"Trace value in\"."));

  // i18n: Here, PC is an acronym for program counter, not personal computer.
  ModalMessageBox::information(
      this, tr("Trace Widget Help"),
      tr("PC = Current instruction game is paused on.\n"
         "\n"
         "Record Trace: Records each executed instruction while stepping from PC to selected "
         "Breakpoint. If backtracing, set PC to where trace should begin from and breakpoint the "
         "instruction to backtrace, so the recording will end on it.\n"
         "\n"
         "Stop recording at: Instruction to stop recording and pause at. Can be left blank to "
         "use "
         "the recording limit. If backtracing, must be set to instruction being backtraced.\n"
         "\n"
         "Timeout: How long in seconds before recording forcefully stops.\n"
         "\n"
         "Result options and filters: Filters information displayed. Does not affect actual "
         "recording or recorded logs. Can be changed or undone at any time.\n"
         "\n"
         "Trace value in: Traces value (target) in given location from its first appearance in "
         "the trace log.\n"
         "\n"
         "Change range: Changes the effective start and end address of a trace log. Instead of "
         "creating a new recording, this can be used to change the instruction for the \"Trace "
         "value in\" target, or to change the backtracing target. Using #number points at a "
         "specific log index. i.e. #100 is the 100th instruction that ran. Right-click a table "
         "entry to copy its index to change range.\n"
         "\n"
         "Maximum Results: Number of results to show in the table.\n"
         "\n"
         "Show Values: Shows the values in the registers, instead of the register's number.\n"
         "\n"
         "Result Filters: Hides certain results from cluttering the table. Only applicable when "
         "tracing a target.\n"
         "Active: Target is changed.\n"
         "Passive: Target is used but not changed (compares, math)\n"
         "Overwritten: Not an actual hit, shows where target was overwritten by an unrelated "
         "value.\n"
         "Memory Op: Target was loaded from memory or stored to it.\n"
         "Pointer: Target used as memory pointer.\n"
         "Move: Target moves registers.\n"));
}

void TraceWidget::OnContextMenu()
{
  // Use column 0 to manage data, everything in the row does the same thing.
  auto* item = m_output_table->item(m_output_table->currentItem()->row(), 0);
  const u32 addr = item->data(ADDRESS_ROLE).toUInt();
  const u32 mem_addr = item->data(MEM_ADDRESS_ROLE).toUInt();
  const u32 index = item->data(INDEX_ROLE).toUInt();
  QMenu* menu = new QMenu(this);

  if (addr != 0)
  {
    menu->addAction(tr("Copy &address"), this, [addr]() {
      QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
    });
    menu->addAction(tr("Show &code address"), this, [this, addr]() { emit ShowCode(addr); });

    if (mem_addr != 0)
    {
      menu->addAction(tr("Copy &memory address"), this, [mem_addr]() {
        QApplication::clipboard()->setText(
            QStringLiteral("%1").arg(mem_addr, 8, 16, QLatin1Char('0')));
      });
      menu->addAction(tr("&Show memory address"), this,
                      [this, mem_addr]() { emit ShowMemory(mem_addr); });
    }

    menu->addAction(tr("Set to start of range"), this, [this, index]() {
      m_range_start->setText(QLatin1Char('#') + QString::number(index));
    });
    menu->addAction(tr("Set to end of range."), this, [this, index]() {
      m_range_end->setText(QLatin1Char('#') + QString::number(index));
    });
  }

  menu->exec(QCursor::pos());
}
