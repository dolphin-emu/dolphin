// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/TraceWidget.h"

#include <optional>
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
#include "DolphinQt/Settings.h"

constexpr int ADDRESS_ROLE = Qt::UserRole;
constexpr int MEM_ADDRESS_ROLE = Qt::UserRole + 1;
#define ElidedText(text)                                                                           \
  fontMetrics().elidedText(text, Qt::ElideRight, m_record_stop_addr->lineEdit()->rect().width() - 5)

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
  m_record_trace = new QPushButton(tr("Record Trace"));
  m_record_trace->setCheckable(true);

  auto* record_options_box = new QGroupBox(tr("Recording options"));
  auto* record_options_layout = new QGridLayout;
  QLabel* record_stop_addr_label = new QLabel(tr("Stop recording at:"));
  m_record_limit_label = new QLabel(tr("Maximum to record"));
  m_record_limit_input = new QSpinBox();
  m_record_limit_input->setMinimum(1000);
  m_record_limit_input->setMaximum(200000);
  m_record_limit_input->setValue(10000);
  m_record_limit_input->setSingleStep(10000);
  m_record_limit_input->setMinimumSize(70, 0);
  m_clear_on_loop = new QCheckBox(tr("Reset on loopback"));

  record_options_layout->addWidget(m_record_limit_label, 0, 0);
  record_options_layout->addWidget(m_record_limit_input, 0, 1);
  record_options_layout->addWidget(record_stop_addr_label, 1, 0);
  record_options_layout->addWidget(m_record_stop_addr, 2, 0, 1, 2);
  record_options_layout->addWidget(m_clear_on_loop, 3, 0, 1, 2);
  record_options_box->setLayout(record_options_layout);

  auto* results_options_box = new QGroupBox(tr("Result Options"));
  auto* results_options_layout = new QGridLayout;
  m_reprocess = new QPushButton(tr("Apply and Update"));

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
  results_options_layout->addWidget(m_reprocess, 5, 0, 1, 2);

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

  input_layout->setSpacing(1);
  input_layout->addItem(new QSpacerItem(1, 20));
  input_layout->addWidget(m_record_trace);
  input_layout->addWidget(record_options_box);
  input_layout->addItem(new QSpacerItem(1, 32));
  input_layout->addWidget(results_options_box);
  input_layout->addItem(new QSpacerItem(1, 32));
  input_layout->addWidget(filter_options_box);
  input_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));

  m_output_table = new QTableWidget();
  m_output_table->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_output_table->setShowGrid(false);
  m_output_table->verticalHeader()->setVisible(false);
  m_output_table->setColumnCount(1);
  m_output_table->setRowCount(1);
  m_output_table->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* splitter = new QSplitter(Qt::Horizontal);
  auto* sidebar_scroll = new QScrollArea;
  sidebar->setLayout(input_layout);
  sidebar_scroll->setWidget(sidebar);
  sidebar_scroll->setWidgetResizable(true);
  // Need to make this variable without letting the sidebar become too big.
  sidebar_scroll->setFixedWidth(225);

  auto* layout = new QHBoxLayout();
  splitter->addWidget(m_output_table);
  splitter->addWidget(sidebar_scroll);
  layout->addWidget(splitter);

  InfoDisp();

  auto* widget = new QWidget;
  widget->setLayout(layout);
  setWidget(widget);
  update();
}

void TraceWidget::ConnectWidgets()
{
  connect(m_record_trace, &QPushButton::clicked, [this](bool record) {
    if (record)
      OnRecordTrace(record);
    else
      ClearAll();
  });
  connect(m_reprocess, &QPushButton::clicked, this, &TraceWidget::DisplayTrace);
  connect(m_change_range, &QCheckBox::stateChanged, this, [this](bool checked) {
    m_range_start->setEnabled(checked);
    m_range_end->setEnabled(checked);
  });
  // When clicking on an item, we want the code widget to update without hiding the trace widget.
  // Useful when both widgets are visible. There's also a right-click option to switch to the code
  // tab.
  connect(m_output_table, &QTableWidget::itemClicked, [this](QTableWidgetItem* item) {
    if (m_record_trace->isChecked())
    {
      emit ShowCode(item->data(ADDRESS_ROLE).toUInt());
      raise();
      activateWindow();
    }
  });
  connect(m_output_table, &TraceWidget::customContextMenuRequested, this,
          &TraceWidget::OnContextMenu);
}

void TraceWidget::ClearAll()
{
  std::vector<TraceOutput>().swap(m_code_trace);
  m_output_table->clear();
  m_output_table->setWordWrap(true);
  m_output_table->setColumnCount(1);
  m_output_table->setRowCount(1);
  m_record_stop_addr->setEnabled(true);
  m_record_stop_addr->setCurrentText(tr("Stop at BP or address"));
  m_change_range->setChecked(false);
  m_change_range->setDisabled(true);
  m_record_trace->setText(tr("Record Trace"));
  m_record_trace->setChecked(false);
  m_record_limit_input->setDisabled(false);
  m_record_limit_label->setText(tr("Maximum to record"));
  m_results_limit_label->setText(tr("Maximum results"));
  UpdateBreakpoints();
  InfoDisp();
}

void TraceWidget::LogCreated(std::optional<QString> target_register)
{
  const u32 end_addr = m_code_trace.back().address;
  QString instr = QString::fromStdString(m_code_trace.back().instruction);
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));

  m_record_stop_addr->insertItem(
      0,
      ElidedText(QStringLiteral("End %1 : %2").arg((end_addr), 8, 16, QLatin1Char('0')).arg(instr)),
      end_addr);
  m_record_stop_addr->setCurrentIndex(0);

  // Update UI
  m_record_stop_addr->setDisabled(true);
  m_change_range->setEnabled(true);
  m_record_trace->setDisabled(false);
  m_reprocess->setDisabled(false);
  m_recording = false;
  m_record_trace->setChecked(true);
  m_record_trace->setText(tr("Delete log and reset"));
  m_record_limit_input->setDisabled(true);
  m_output_table->setWordWrap(false);
  m_output_table->setColumnCount(7);
  m_output_table->setRowCount(100);

  // Set register from autostepping
  if (target_register)
    m_trace_target->setText(target_register.value());

  TraceWidget::DisplayTrace();
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
  if (!m_system.GetCPU().IsStepping() || m_recording)
    return;

  m_recording = true;

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

    // Stop recording if the record limit surpassed. The limit is allow to overflow once.
    if (m_code_trace.size() >= m_record_limit)
      log_output = nullptr;

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
    LogCreated(target_register);
  else
    m_recording = false;
}

void TraceWidget::OnRecordTrace(bool checked)
{
  m_record_trace->setChecked(false);

  if (!m_system.GetCPU().IsStepping() || m_recording)
    return;

  // Try to get end_bp based on editable input text, then on combo box selection.
  bool good;
  u32 end_bp = m_record_stop_addr->currentText().toUInt(&good, 16);
  if (!good)
    end_bp = m_record_stop_addr->currentData().toUInt(&good);
  if (!good)
    end_bp = 0;

  CodeTrace codetrace;
  m_recording = true;
  m_record_trace->setDisabled(true);
  m_reprocess->setDisabled(true);

  m_record_limit = m_record_limit_input->value();

  Core::CPUThreadGuard guard(m_system);
  bool timed_out = codetrace.RecordTrace(guard, &m_code_trace, m_record_limit, 10, end_bp,
                                         m_clear_on_loop->isChecked());
  emit Host::GetInstance()->UpdateDisasmDialog();

  // Errors
  m_error_msg.clear();

  if (m_code_trace.empty())
    m_error_msg.emplace_back(tr("Record failed to run."));
  else if (timed_out)
    m_error_msg.emplace_back(tr("Record trace ran out of time. Backtrace won't be correct."));

  LogCreated();
}

std::vector<TraceResults> TraceWidget::CodePath(u32 start, u32 end, size_t results_limit)
{
  // Shows entire trace (limited by results limit) without filtering if target input is blank.
  std::vector<TraceOutput> tmp_out;
  auto begin_itr = m_code_trace.begin();
  auto end_itr = m_code_trace.end();
  size_t trace_size = m_code_trace.size();

  if (m_change_range->isChecked())
  {
    auto begin_itr_temp = find_if(m_code_trace.begin(), m_code_trace.end(),
                                  [start](const TraceOutput& t) { return t.address == start; });
    auto end_itr_temp =
        find_if(m_code_trace.rbegin(), m_code_trace.rend(), [end](const TraceOutput& t) {
          return t.address == end;
        }).base();

    if (begin_itr_temp == m_code_trace.end() || end_itr_temp == m_code_trace.begin())
    {
      m_error_msg.emplace_back(tr("Change Range using invalid addresses. Using full range."));
    }
    else
    {
      begin_itr = begin_itr_temp;
      end_itr = end_itr_temp;
      trace_size = std::distance(begin_itr, end_itr);
    }
  }

  std::vector<TraceResults> tmp_results;
  if (m_backtrace->isChecked())
  {
    auto rend_itr = std::reverse_iterator(begin_itr);
    auto rbegin_itr = std::reverse_iterator(end_itr);

    if (results_limit < trace_size)
      rend_itr = std::next(rbegin_itr, results_limit);

    for (auto& i = rbegin_itr; i != rend_itr; i++)
      tmp_results.emplace_back(TraceResults{*i, HitType::ACTIVE, {}});

    return tmp_results;
  }
  else
  {
    if (results_limit < trace_size)
      end_itr = std::next(begin_itr, results_limit);

    for (auto& i = begin_itr; i != end_itr; i++)
      tmp_results.emplace_back(TraceResults{*i, HitType::ACTIVE, {}});

    return tmp_results;
  }
}

std::vector<TraceResults> TraceWidget::MakeTraceFromLog()
{
  // Setup start and end for a changed range, 0 means use full range.
  u32 start = 0;
  u32 end = 0;

  if (m_change_range->isChecked())
  {
    if (!m_range_start->text().isEmpty())
    {
      bool good;
      start = m_range_start->text().toUInt(&good, 16);

      if (!good)
        m_error_msg.emplace_back(tr("Input error with starting address."));
    }
    if (!m_range_end->text().isEmpty())
    {
      bool good;
      end = m_range_end->text().toUInt(&good, 16);

      if (!good)
        m_error_msg.emplace_back(tr("Input error with ending address."));
    }
  }

  const size_t results_limit = static_cast<size_t>(m_results_limit_input->value());

  // If no trace target, display the recorded log without tracing
  if (m_trace_target->text().isEmpty())
    return CodePath(start, end, results_limit);

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
  bool trace_running = false;
  const bool backtrace = m_backtrace->isChecked();

  if (backtrace)
    std::reverse(m_code_trace.begin(), m_code_trace.end());

  if (start == 0)
    trace_running = true;

  // If the first instance of a tracked target is it being destroyed, we probably wanted to track
  // it from that point onwards. Make the first hit a special exclusion case.
  bool first_hit = true;

  for (TraceOutput& current : m_code_trace)
  {
    if (!trace_running)
    {
      if (current.address != start)
        continue;
      else
        trace_running = true;
    }

    TraceResults tmp_results;

    type = codetrace.TraceLogic(current, first_hit, &tmp_results.regs, backtrace);

    if (type != HitType::SKIP && type != HitType::STOP)
    {
      if (static_cast<u32>(type) & verbosity_flags)
      {
        tmp_results.trace_output = current;
        tmp_results.type = type;
        trace_results.emplace_back(tmp_results);
      }
      first_hit = false;
    }

    // Stop if we run out of things to track
    if (type == HitType::STOP || trace_results.size() >= results_limit || current.address == end)
      break;
  }

  if (backtrace)
    std::reverse(m_code_trace.begin(), m_code_trace.end());

  return trace_results;
}

void TraceWidget::DisplayTrace()
{
  if (m_code_trace.empty())
    return;

  m_output_table->clear();

  const std::vector<TraceResults> trace_results = MakeTraceFromLog();

  if (m_code_trace.size() >= m_record_limit)
    m_error_msg.emplace_back(tr("Max recorded lines reached, tracing stopped."));

  if (trace_results.size() >= static_cast<size_t>(m_results_limit_input->value()))
    m_error_msg.emplace_back(tr("Max output size reached, stopped early"));

  // Update UI
  m_record_limit_label->setText(
      QStringLiteral("Recorded: %1 of").arg(QString::number(m_code_trace.size())));
  m_results_limit_label->setText(
      QStringLiteral("Results: %1 of").arg(QString::number(trace_results.size())));

  // The {num, num} may not be required.
  std::regex reg("(\\S*)\\s+(?:(\\S{0,12})\\s*)?(?:(\\S{0,8})\\s*)?(?:(\\S{0,8})\\s*)?(.*)");

  std::smatch match;
  m_output_table->setColumnCount(7);
  m_output_table->setRowCount(static_cast<int>(trace_results.size() + m_error_msg.size() + 1));

  int row = 0;

  for (auto& error : m_error_msg)
  {
    auto* error_item = new QTableWidgetItem(error);
    m_output_table->setItem(row, 0, error_item);
    m_output_table->setSpan(row, 0, 1, m_output_table->columnCount());
    row++;
  }

  if (!m_error_msg.empty())
    row++;

  m_error_msg.clear();

  for (auto& result : trace_results)
  {
    auto out = result.trace_output;
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

    addr_item->setData(ADDRESS_ROLE, out.address);
    if (out.memory_target)
      arg3_item->setData(MEM_ADDRESS_ROLE, *out.memory_target);

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

    m_output_table->setItem(row, 0, addr_item);
    m_output_table->setItem(row, 1, instr_item);
    m_output_table->setItem(row, 2, target_item);
    m_output_table->setItem(row, 3, arg1_item);
    m_output_table->setItem(row, 4, arg2_item);
    m_output_table->setItem(row, 5, arg3_item);
    m_output_table->setItem(row, 6, sym_item);

    row++;
  }

  // Unlike other TableWidgets, new columns will not be created while scrolling, so resizing to
  // contents works here.
  m_output_table->verticalHeader()->setMaximumSectionSize(m_font_vspace);
  m_output_table->resizeColumnsToContents();
  m_output_table->resizeRowsToContents();
}

void TraceWidget::UpdateBreakpoints()
{
  // Leave the recorded end intact.
  if (m_record_trace->isChecked())
  {
    for (int i = m_record_stop_addr->count(); i > 1; i--)
      m_record_stop_addr->removeItem(1);
  }
  else
  {
    m_record_stop_addr->clear();
  }
  auto& power_pc = m_system.GetPowerPC();
  auto& breakpoints = power_pc.GetBreakPoints();
  Core::CPUThreadGuard guard(m_system);

  int index = -1;

  for (const auto& bp : breakpoints.GetBreakPoints())
  {
    QString instr =
        QString::fromStdString(power_pc.GetDebugInterface().Disassemble(&guard, bp.address));
    instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
    m_record_stop_addr->addItem(
        ElidedText(QStringLiteral("%1 : %2").arg(bp.address, 8, 16, QLatin1Char('0')).arg(instr)),
        bp.address);
    index++;
  }

  // User typically wants the most recently placed breakpoint.
  if (!m_record_trace->isChecked())
    m_record_stop_addr->setCurrentIndex(index);
}

void TraceWidget::InfoDisp()
{
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  auto* info = new QTableWidgetItem(QStringLiteral(
      "Used to track the value in a target register or memory address and its uses.\n\n"
      "Record Trace: Records each executed instruction while stepping from PC to selected "
      "Breakpoint. Required before tracking a target. If backtracing, set PC to how far back you "
      "want to trace to and breakpoint the instruction you want to trace backwards.\n\n"
      "Register: Input examples: r5, f31, use f for ps registers or 80000000 for memory. Only "
      "takes one value at a time. Leave blank to view complete code path.\n\n"
      "Starting Address: Used to change range before tracking a value. Record Trace's starting "
      "address is always the PC. Can change freely after recording trace.\n\n"
      "Ending breakpoint: Where the trace will stop. If backtracing, should be the line you want "
      "to backtrace from.\n\n"
      "Backtrace: A reverse trace that shows where a value came from, the first output line is "
      "the "
      "most recent executed.\n\n"
      "Verbose: Will record all references to what is being tracked, rather than just where it "
      "is "
      "moving to or from.\n\n"
      "Reset on loopback: Will clear the trace if starting address is looped through, ensuring "
      "only the final loop to the end breakpoint is recorded.\n\n"
      "Change Range: Change the start and end points of the trace for tracking. Loops may make "
      "certain ranges buggy.\n\n"
      "Track target: Follows the register or memory value through the recorded trace. You don't "
      "have to record a trace multiple times if the first trace recorded the area of code you "
      "need. You can change any value or option and press track target again. Changing the "
      "second "
      "breakpoint will let you backtrace from a new location."));
  // QFont fixedfont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  // fixedfont.setPointSize(11);
  // info->setFont(fixedfont);
  m_output_table->setItem(0, 0, info);
  m_output_table->verticalHeader()->setMaximumSectionSize(-1);
  m_output_table->horizontalHeader()->setMaximumSectionSize(m_output_table->width());
  m_output_table->resizeColumnsToContents();
  m_output_table->resizeRowsToContents();
  m_output_table->setWordWrap(true);
}

void TraceWidget::OnContextMenu()
{
  QMenu* menu = new QMenu(this);
  menu->addAction(tr("Copy &address"), this, [this]() {
    const u32 addr = m_output_table->currentItem()->data(ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->addAction(tr("Show &code address"), this, [this]() {
    const u32 addr = m_output_table->currentItem()->data(ADDRESS_ROLE).toUInt();
    emit ShowCode(addr);
  });
  menu->addAction(tr("Copy &memory address"), this, [this]() {
    const u32 addr = m_output_table->currentItem()->data(MEM_ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->addAction(tr("&Show memory address"), this, [this]() {
    const u32 addr = m_output_table->currentItem()->data(MEM_ADDRESS_ROLE).toUInt();
    emit ShowMemory(addr);
  });
  menu->exec(QCursor::pos());
}
