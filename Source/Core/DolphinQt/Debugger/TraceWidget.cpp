// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/TraceWidget.h"

#include <optional>
#include <regex>
#include <vector>

#include <fmt/format.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/Debug/CodeTrace.h"
#include "Common/StringUtil.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

constexpr int ADDRESS_ROLE = Qt::UserRole;
constexpr int MEM_ADDRESS_ROLE = Qt::UserRole + 1;
#define ElidedText(text)                                                                           \
  fontMetrics().elidedText(text, Qt::ElideRight, m_bp2->lineEdit()->rect().width() - 5)

TraceWidget::TraceWidget(QWidget* parent) : QDockWidget(parent)
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

  // connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &TraceWidget::Update);

  ConnectWidgets();
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

void TraceWidget::CreateWidgets()
{
  auto* input_layout = new QVBoxLayout;

  m_bp1 = new QComboBox();
  m_bp1->setEditable(true);
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  m_bp1->setCurrentText(tr("Uses PC as trace starting point."));
  m_bp1->setDisabled(true);
  m_bp2 = new QComboBox();
  m_bp2->setEditable(true);
  m_bp2->setCurrentText(tr("Stop BP or address"));
  m_record_trace = new QPushButton(tr("Record Trace"));
  m_record_trace->setCheckable(true);

  auto* record_options_box = new QGroupBox(tr("Recording options"));
  auto* record_options_layout = new QGridLayout;
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
  record_options_layout->addWidget(m_clear_on_loop, 1, 0, 1, 2);
  record_options_box->setLayout(record_options_layout);

  auto* trace_target_layout = new QHBoxLayout;
  m_reprocess = new QPushButton(tr("Track Target"));
  m_trace_target = new QLineEdit();
  m_trace_target->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_trace_target->setPlaceholderText(tr("Reg or Mem"));
  trace_target_layout->addWidget(m_reprocess);
  trace_target_layout->addWidget(m_trace_target);

  auto* results_options_box = new QGroupBox(tr("Output Options"));
  auto* results_options_layout = new QGridLayout;
  m_results_limit_label = new QLabel(tr("Maximum results"));
  m_results_limit_input = new QSpinBox();
  m_results_limit_input->setMinimum(100);
  m_results_limit_input->setMaximum(10000);
  m_results_limit_input->setValue(1000);
  m_results_limit_input->setSingleStep(250);
  m_results_limit_input->setMinimumSize(50, 0);

  m_backtrace = new QCheckBox(tr("Backtrace"));
  m_verbose = new QCheckBox(tr("Verbose"));
  m_change_range = new QCheckBox(tr("Change Range"));
  m_change_range->setDisabled(true);

  results_options_layout->addWidget(m_results_limit_label, 0, 0);
  results_options_layout->addWidget(m_results_limit_input, 0, 1, 1, 2);
  results_options_layout->addWidget(m_backtrace, 1, 0);
  results_options_layout->addWidget(m_verbose, 1, 1);
  results_options_layout->addWidget(m_change_range, 2, 0);

  results_options_box->setLayout(results_options_layout);

  input_layout->setSpacing(1);
  input_layout->addWidget(m_bp1);
  input_layout->addWidget(m_bp2);
  input_layout->addItem(new QSpacerItem(1, 32));
  input_layout->addWidget(m_record_trace);
  input_layout->addWidget(record_options_box);
  input_layout->addItem(new QSpacerItem(1, 32));
  input_layout->addLayout(trace_target_layout);
  input_layout->addWidget(results_options_box);
  input_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));

  m_output_list = new QListWidget();
  m_output_list->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_output_list->setSpacing(1);
  m_output_list->setWordWrap(true);

  // Fixed width font to max table line up.
  QFont fixedfont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixedfont.setPointSize(11);
  m_output_list->setFont(fixedfont);
  m_output_list->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* splitter = new QSplitter(Qt::Horizontal);
  auto* side_bar_widget = new QWidget;
  auto* sidebar_scroll = new QScrollArea;
  side_bar_widget->setLayout(input_layout);
  sidebar_scroll->setWidget(side_bar_widget);
  sidebar_scroll->setWidgetResizable(true);
  sidebar_scroll->setFixedWidth(225);

  auto* layout = new QHBoxLayout();
  splitter->addWidget(m_output_list);
  splitter->addWidget(sidebar_scroll);
  layout->addWidget(splitter);

  InfoDisp();

  auto* widget = new QWidget;
  widget->setLayout(layout);
  setWidget(widget);
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
  connect(m_change_range, &QCheckBox::clicked, this, &TraceWidget::OnChangeRange);
  // When clicking on an item, we want the code widget to update without hiding the trace widget.
  // Useful when both widgets are visible. There's also a right-click option to switch to the code
  // tab.
  connect(m_output_list, &QListWidget::itemClicked, [this](QListWidgetItem* item) {
    if (m_record_trace->isChecked())
    {
      emit ShowCode(item->data(ADDRESS_ROLE).toUInt());
      raise();
      activateWindow();
    }
  });
  connect(m_output_list, &TraceWidget::customContextMenuRequested, this,
          &TraceWidget::OnContextMenu);
}

void TraceWidget::ClearAll()
{
  std::vector<TraceOutput>().swap(m_code_trace);
  m_output_list->clear();
  m_output_list->setWordWrap(true);
  m_bp1->clear();
  m_bp1->setDisabled(true);
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  m_bp1->setCurrentText(tr("Uses PC as trace starting point."));
  m_bp2->setEnabled(true);
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

void TraceWidget::OnRecordTrace(bool checked)
{
  m_record_trace->setChecked(false);

  if (!CPU::IsStepping() || m_recording)
    return;

  // Try to get end_bp based on editable input text, then on combo box selection.
  bool good;
  u32 start_bp = PC;
  u32 end_bp = m_bp2->currentText().toUInt(&good, 16);
  if (!good)
    end_bp = m_bp2->currentData().toUInt(&good);
  if (!good)
    return;

  m_recording = true;
  m_record_trace->setDisabled(true);
  m_reprocess->setDisabled(true);

  m_record_limit = m_record_limit_input->value();

  bool timed_out =
      CT.RecordCodeTrace(&m_code_trace, m_record_limit, 10, end_bp, m_clear_on_loop->isChecked());

  // UpdateDisasmDialog causes a crash now. We're using CPU::StepOpcode(&sync_event) which doesn't
  // appear to require an UpdateDisasmDialog anyway.
  // emit Host::GetInstance()->UpdateDisasmDialog();

  // Errors
  m_error_msg.clear();

  if (timed_out && m_code_trace.empty())
    new QListWidgetItem(tr("Record failed to run."), m_output_list);
  else if (timed_out)
    m_error_msg = tr("Record trace ran out of time. Backtrace won't be correct.");

  // Record actual start and end into combo boxes.
  m_bp1->setDisabled(false);
  m_bp1->clear();

  QString instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(start_bp));
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));

  m_bp1->addItem(
      ElidedText(QStringLiteral("Start %1 : %2").arg((end_bp), 8, 16, QLatin1Char('0')).arg(instr)),
      start_bp);
  m_bp1->setDisabled(true);

  end_bp = m_code_trace.back().address;
  instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(end_bp));
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));

  m_bp2->insertItem(
      0,
      ElidedText(QStringLiteral("End %1 : %2").arg((end_bp), 8, 16, QLatin1Char('0')).arg(instr)),
      end_bp);
  m_bp2->setCurrentIndex(0);
  m_bp2->setDisabled(true);

  // Update UI
  m_change_range->setEnabled(true);
  m_record_trace->setDisabled(false);
  m_reprocess->setDisabled(false);
  m_recording = false;
  m_record_trace->setChecked(true);
  m_record_trace->setText(tr("Reset All"));
  m_record_limit_input->setDisabled(true);
  m_output_list->setWordWrap(false);

  TraceWidget::DisplayTrace();
}

const std::vector<TraceOutput> TraceWidget::CodePath(u32 start, u32 end, u32 results_limit)
{
  // Shows entire trace without filtering if target input is blank.
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
      new QListWidgetItem(tr("Change Range using invalid addresses. Using full range."),
                          m_output_list);
    }
    else
    {
      begin_itr = begin_itr_temp;
      end_itr = end_itr_temp;
      trace_size = std::distance(begin_itr, end_itr);
    }
  }

  if (m_backtrace->isChecked())
  {
    auto rend_itr = std::reverse_iterator(begin_itr);
    auto rbegin_itr = std::reverse_iterator(end_itr);

    if (results_limit < trace_size)
      rend_itr = std::next(rbegin_itr, results_limit);

    return std::vector<TraceOutput>(rbegin_itr, rend_itr);
  }
  else
  {
    if (results_limit < trace_size)
      end_itr = std::next(begin_itr, results_limit);

    return std::vector<TraceOutput>(begin_itr, end_itr);
  }
}

const std::vector<TraceOutput> TraceWidget::GetTraceResults()
{
  // Setup start and end for a changed range, 0 means use full range.
  u32 start = 0;
  u32 end = 0;

  if (m_change_range->isChecked())
  {
    bool good;
    start = m_bp1->currentText().toUInt(&good, 16);
    if (!good)
      start = m_bp1->currentData().toUInt(&good);
    if (!good)
    {
      start = 0;
      new QListWidgetItem(tr("Input error with starting address."), m_output_list);
    }

    end = m_bp2->currentText().toUInt(&good, 16);
    if (!good)
      end = m_bp2->currentData().toUInt(&good);
    if (!good)
    {
      end = 0;
      new QListWidgetItem(tr("Input error with ending address."), m_output_list);
    }
  }

  // Setup memory or register to track
  std::optional<std::string> track_reg = std::nullopt;
  std::optional<u32> track_mem = std::nullopt;

  if (m_trace_target->text().size() == 8)
  {
    bool ok;
    track_mem = m_trace_target->text().toUInt(&ok, 16);

    if (!ok)
    {
      new QListWidgetItem(tr("Memory Address input error"), m_output_list);
      return std::vector<TraceOutput>();
    }
  }
  else if (m_trace_target->text().size() < 5)
  {
    QString reg_tmp = m_trace_target->text();
    reg_tmp.replace(QStringLiteral("sp"), QStringLiteral("r1"), Qt::CaseInsensitive);
    reg_tmp.replace(QStringLiteral("rtoc"), QStringLiteral("r2"), Qt::CaseInsensitive);
    track_reg = reg_tmp.toStdString();
  }
  else
  {
    new QListWidgetItem(tr("Register input error"), m_output_list);
    return std::vector<TraceOutput>();
  }

  // Either use CodePath to display the full trace (limited by results_limit) or track a value
  // through the full trace using a Forward/Back Trace.
  u32 results_limit = m_results_limit_input->value();
  bool verbose = m_verbose->isChecked();

  if (m_trace_target->text().isEmpty())
    return CodePath(start, end, results_limit);
  else if (m_backtrace->isChecked())
    return CT.Backtrace(&m_code_trace, track_reg, track_mem, start, end, results_limit, verbose);
  else
    return CT.ForwardTrace(&m_code_trace, track_reg, track_mem, start, end, results_limit, verbose);
}

void TraceWidget::DisplayTrace()
{
  if (m_code_trace.empty())
    return;

  m_output_list->clear();

  const std::vector<TraceOutput> trace_out = GetTraceResults();

  // Errors to display
  if (!m_error_msg.isEmpty())
    new QListWidgetItem(m_error_msg, m_output_list);

  if (m_code_trace.size() >= m_record_limit)
    new QListWidgetItem(tr("Trace max limit reached, backtrace won't work."), m_output_list);

  if (trace_out.size() >= m_results_limit_input->value())
    new QListWidgetItem(tr("Max output size reached, stopped early"), m_output_list);

  // Update UI
  m_record_limit_label->setText(
      QStringLiteral("Recorded: %1 of").arg(QString::number(m_code_trace.size())));
  m_results_limit_label->setText(
      QStringLiteral("Results: %1 of").arg(QString::number(trace_out.size())));

  // Cleanup and prepare output, then send to Qlistwidget.
  std::regex reg("(\\S*)\\s+(?:(\\S{0,6})\\s*)?(?:(\\S{0,8})\\s*)?(?:(\\S{0,8})\\s*)?(.*)");
  std::smatch match;
  std::string is_mem;

  for (auto out : trace_out)
  {
    QString fix_sym = QString::fromStdString(g_symbolDB.GetDescription(out.address));
    fix_sym.replace(QStringLiteral("\t"), QStringLiteral("  "));

    std::regex_search(out.instruction, match, reg);
    // Prep to potentially modify this string.
    std::string match4 = match.str(4);

    if (out.memory_target)
    {
      is_mem = fmt::format("{:08x}", out.memory_target);

      // There's an extra comma for psq read/writes.
      if (match4.find(',') != std::string::npos)
        match4.pop_back();
    }
    else
    {
      is_mem = match.str(5);
    }

    auto* item =
        new QListWidgetItem(QString::fromStdString(fmt::format(
                                "{:08x} : {:<11}{:<6}{:<8}{:<8}{:<18}", out.address, match.str(1),
                                match.str(2), match.str(3), match4, is_mem)) +
                            fix_sym);

    item->setData(ADDRESS_ROLE, out.address);
    if (out.memory_target)
      item->setData(MEM_ADDRESS_ROLE, out.memory_target);
    m_output_list->addItem(item);
  }
}

void TraceWidget::OnChangeRange()
{
  if (!m_change_range->isChecked())
  {
    m_bp1->setCurrentIndex(0);
    m_bp2->setCurrentIndex(0);
    m_bp1->setEnabled(false);
    m_bp2->setEnabled(false);
    return;
  }

  u32 bp1 = m_bp1->currentData().toUInt();
  u32 bp2 = m_bp2->currentData().toUInt();

  m_bp1->setEnabled(true);
  m_bp2->setEnabled(true);

  m_bp1->setEditText(QStringLiteral("%1").arg(bp1, 8, 16, QLatin1Char('0')));
  m_bp2->setEditText(QStringLiteral("%1").arg(bp2, 8, 16, QLatin1Char('0')));
}

void TraceWidget::UpdateBreakpoints()
{
  // Leave the recorded start and end range intact.
  if (m_record_trace->isChecked())
  {
    for (int i = m_bp2->count(); i > 1; i--)
      m_bp2->removeItem(1);
    for (int i = m_bp1->count(); i > 1; i--)
      m_bp1->removeItem(1);
  }
  else
  {
    m_bp2->clear();
  }

  auto bp_vec = PowerPC::breakpoints.GetBreakPoints();
  int index = -1;

  for (auto& i : bp_vec)
  {
    QString instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(i.address));
    instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
    if (m_record_trace->isChecked())
    {
      m_bp1->addItem(
          ElidedText(QStringLiteral("%1 : %2").arg(i.address, 8, 16, QLatin1Char('0')).arg(instr)),
          i.address);
    }
    m_bp2->addItem(
        ElidedText(QStringLiteral("%1 : %2").arg(i.address, 8, 16, QLatin1Char('0')).arg(instr)),
        i.address);
    index++;
  }

  // User typically wants the most recently placed breakpoint.
  if (!m_record_trace->isChecked())
    m_bp2->setCurrentIndex(index);
}

void TraceWidget::InfoDisp()
{
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  new QListWidgetItem(
      QStringLiteral(
          "Used to track a target register or memory address and its uses.\n\nRecord Trace: "
          "Records "
          "each executed instruction while stepping from "
          "PC to selected Breakpoint. Required before tracking a target. If backtracing, set "
          "PC "
          "to how far back you want to trace to and breakpoint the instruction you want to "
          "trace backwards.\n\nRegister: Input "
          "examples: "
          "r5, f31, use f for ps registers or 80000000 for memory. Only takes one value at a "
          "time. Leave blank "
          "to "
          "view complete "
          "code path.\n\nStarting Address: "
          "Used to change range before tracking a value. Record Trace's starting address "
          "is always "
          "the "
          "PC."
          " Can change freely after recording trace.\n\nEnding breakpoint: "
          "Where "
          "the trace will stop. If backtracing, should be the line you want to backtrace "
          "from.\n\nBacktrace: A reverse trace that shows where a value came from, the first "
          "output "
          "line "
          "is the most recent executed.\n\nVerbose: Will record all references to what is being "
          "tracked, rather than just where it is moving to or from.\n\nReset on loopback: Will "
          "clear "
          "the "
          "trace "
          "if starting address is looped through, ensuring only the final loop to the end "
          "breakpoint is recorded.\n\nChange Range: Change the start and end points of the trace "
          "for tracking. Loops may make certain ranges buggy.\n\nTrack target: Follows the "
          "register or memory value through the recorded trace. You don't "
          "have "
          "to "
          "record a trace multiple times if "
          "the "
          "first trace recorded the area of code you need. You can change any value or option "
          "and "
          "press track target again. Changing the second "
          "breakpoint"
          "will let you backtrace from a new location."),
      m_output_list);
}

void TraceWidget::OnContextMenu()
{
  QMenu* menu = new QMenu(this);
  menu->addAction(tr("Copy &address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->addAction(tr("Show &code address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(ADDRESS_ROLE).toUInt();
    emit ShowCode(addr);
  });
  menu->addAction(tr("Copy &memory address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(MEM_ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->addAction(tr("&Show memory address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(MEM_ADDRESS_ROLE).toUInt();
    emit ShowMemory(addr);
  });
  menu->exec(QCursor::pos());
}
