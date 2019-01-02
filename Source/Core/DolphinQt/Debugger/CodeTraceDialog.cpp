// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeTraceDialog.h"

#include <chrono>
#include <regex>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QVBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

#include "Common/StringUtil.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

#include "DolphinQt/Debugger/CodeWidget.h"

constexpr int ADDRESS_ROLE = Qt::UserRole;
constexpr int MEM_ADDRESS_ROLE = Qt::UserRole + 1;

CodeTraceDialog::CodeTraceDialog(CodeWidget* parent) : QDialog(parent), m_parent(parent)
{
  setWindowTitle(tr("Trace"));
  CreateWidgets();
  ConnectWidgets();
  UpdateBreakpoints();
}

void CodeTraceDialog::reject()
{
  // Make sure to free memory and reset info message.
  ClearAll();
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("tracedialog/geometry"), saveGeometry());
  QDialog::reject();
}

void CodeTraceDialog::CreateWidgets()
{  
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("tracedialog/geometry")).toByteArray());
  auto* input_layout = new QHBoxLayout;
  m_trace_target = new QLineEdit();
  m_trace_target->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_trace_target->setPlaceholderText(tr("Register or Mem Address"));
  m_bp1 = new QComboBox();
  m_bp1->setEditable(true);
  m_bp1->setCurrentText(tr("Uses PC as trace starting point."));
  m_bp1->setDisabled(true);
  m_bp2 = new QComboBox();
  m_bp2->setEditable(true);
  m_bp2->setCurrentText(tr("Stop BP or address"));

  input_layout->addWidget(m_trace_target);
  input_layout->addWidget(m_bp1);
  input_layout->addWidget(m_bp2);

  auto* boxes_layout = new QHBoxLayout;
  m_backtrace = new QCheckBox(tr("BackTrace"));
  m_verbose = new QCheckBox(tr("Verbose"));
  m_clear_on_loop = new QCheckBox(tr("Reset on loopback"));
  m_change_range = new QCheckBox(tr("Change Range"));
  m_sizes = new QLabel();

  m_reprocess = new QPushButton(tr("Reprocess"));
  m_run_trace = new QPushButton(tr("Run Trace"));
  m_run_trace->setCheckable(true);

  boxes_layout->addWidget(m_backtrace);
  boxes_layout->addWidget(m_verbose);
  boxes_layout->addWidget(m_clear_on_loop);
  boxes_layout->addWidget(m_change_range);
  boxes_layout->addWidget(m_reprocess);

  boxes_layout->addWidget(m_sizes);

  boxes_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Maximum));
  boxes_layout->addWidget(m_run_trace);

  m_output_list = new QListWidget();
  m_output_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  QFont font(QStringLiteral("Monospace"));
  font.setStyleHint(QFont::TypeWriter);
  m_output_list->setFont(font);
  m_output_list->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* layout = new QVBoxLayout();
  layout->addLayout(input_layout);
  layout->addLayout(boxes_layout);
  layout->addWidget(m_output_list);

  InfoDisp();

  setLayout(layout);
}

void CodeTraceDialog::ConnectWidgets()
{
  connect(m_parent, &CodeWidget::BreakpointsChanged, this, &CodeTraceDialog::UpdateBreakpoints);
  connect(m_run_trace, &QPushButton::toggled, this, &CodeTraceDialog::OnRunTrace);
  connect(m_reprocess, &QPushButton::pressed, this, &CodeTraceDialog::DisplayTrace);
  connect(m_output_list, &QListWidget::itemClicked, m_parent, [this](QListWidgetItem* item) {
    m_parent->SetAddress(item->data(ADDRESS_ROLE).toUInt(),
                         CodeViewWidget::SetAddressUpdate::WithUpdate);
  });
  connect(m_output_list, &CodeTraceDialog::customContextMenuRequested, this,
          &CodeTraceDialog::OnContextMenu);
}

void CodeTraceDialog::ClearAll()
{
  std::vector<CodeTrace>().swap(m_code_trace);
  std::vector<TraceOutput>().swap(m_trace_out);
  m_reg.clear();
  m_mem.clear();
  m_output_list->clear();
  m_bp1->setDisabled(true);
  m_bp1->setCurrentText(tr("Uses PC as trace starting point."));
  UpdateBreakpoints();
  InfoDisp();
}

void CodeTraceDialog::OnRunTrace(bool checked)
{
  if (!checked)
  {
    m_run_trace->setText(tr("Run Trace"));
    ClearAll();
    return;
  }

  m_run_trace->setText(tr("Reset All"));
  m_code_trace.clear();
  m_code_trace.reserve(m_max_code_trace);

  if (!CPU::IsStepping())
    return;

  CPU::PauseAndLock(true, false);
  PowerPC::breakpoints.ClearAllTemporary();

  // Keep stepping until the end_bp or timeout after five seconds
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(10);
  PowerPC::CoreMode old_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);

  // Try to get end_bp based on editable input text, then on combo box selection.
  bool good;
  u32 end_bp = m_bp2->currentText().toUInt(&good, 16);
  if (!good)
    end_bp = m_bp2->currentData().toUInt(&good);
  if (!good)
    return;

  u32 start_bp = PC;

  UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
  SaveInstruction();
  do
  {
    PowerPC::SingleStep();
    SaveInstruction();

    if (PC == start_bp && m_clear_on_loop->isChecked())
      m_code_trace.clear();

  } while (clock::now() < timeout && PC != end_bp && m_code_trace.size() < m_max_code_trace);

  if (clock::now() >= timeout)
    m_error_msg = tr("Trace timed out. Backtrace won't be correct.");

  PowerPC::SetMode(old_mode);
  CPU::PauseAndLock(false, false);

  // Is this needed?
  emit Host::GetInstance()->UpdateDisasmDialog();

  // Record actual start and end into combo boxes.
  m_bp1->setDisabled(false);
  m_bp1->clear();
  QString instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(start_bp));
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
  m_bp1->addItem(
      QStringLiteral("Trace Begin   %1 : %2").arg(start_bp, 8, 16, QLatin1Char('0')).arg(instr),
      start_bp);

  instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(PC));
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
  m_bp2->insertItem(
      0, QStringLiteral("Trace End   %1 : %2").arg(PC, 8, 16, QLatin1Char('0')).arg(instr), PC);
  m_bp2->setCurrentIndex(0);

  CodeTraceDialog::DisplayTrace();
}

void CodeTraceDialog::SaveInstruction()
{
  if (m_code_trace.size() >= m_max_code_trace)
    return;

  CodeTrace tmp_trace;
  std::string tmp = PowerPC::debug_interface.Disassemble(PC);
  tmp_trace.instruction = tmp;
  std::regex replace_sp("(\\W)sp");
  std::regex replace_rtoc("rtoc");
  std::regex replace_ps("(\\W)p(\\d+)");
  tmp = std::regex_replace(tmp, replace_sp, "$1r1");
  tmp = std::regex_replace(tmp, replace_rtoc, "r2");
  tmp = std::regex_replace(tmp, replace_ps, "$1f$2");
  tmp_trace.address = PC;

  // Pull all register numbers out and store them. Limited to Reg0 if ps operation, as ps get
  // too complicated to track easily.
  std::regex regis("\\W([rfp]\\d+)[^r^f]*(?:([rf]\\d+))?[^r^f\\d]*(?:([rf]\\d+))?");
  std::smatch match;

  // ex: add r4, r5, r6 -> Reg0, Reg1, Reg2. Reg0 is always the target register.
  if (std::regex_search(tmp, match, regis))
  {
    tmp_trace.reg0 = match.str(1);
    if (match[2].matched)
      tmp_trace.reg1 = match.str(2);
    if (match[3].matched)
      tmp_trace.reg2 = match.str(3);

    // Get Memory Destination if load/store. The only instructions that start with L are load and
    // load immediate li/lis (excluded).
    if (tmp.compare(0, 2, "st") == 0 || tmp.compare(0, 5, "psq_s") == 0)
    {
      tmp_trace.memory_dest = PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp);
      tmp_trace.is_store = true;
    }
    else if ((tmp.compare(0, 1, "l") == 0 && tmp.compare(1, 1, "i") != 0) ||
             tmp.compare(0, 5, "psq_l") == 0)
    {
      tmp_trace.memory_dest = PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp);
      tmp_trace.is_load = true;
    }
  }

  m_code_trace.push_back(tmp_trace);
}

void CodeTraceDialog::ForwardTrace()
{
  std::vector<std::string> exclude{"dc", "ic", "mt", "c", "fc"};
  TraceOutput tmp_out;
  bool first_hit = true;

  trace_itr_pair p = UpdateIterator();
  auto begin_itr = p.first;
  auto end_itr = p.second;

  for (auto instr = begin_itr; instr != end_itr; instr++)
  {
    // Not an instruction we care about (branches).
    if (instr->reg0.empty())
      continue;

    auto itR = std::find(m_reg.begin(), m_reg.end(), instr->reg0);
    auto itM = std::find(m_mem.begin(), m_mem.end(), instr->memory_dest);
    const bool match_reg12 =
        (std::find(m_reg.begin(), m_reg.end(), instr->reg1) != m_reg.end() &&
         !instr->reg1.empty()) ||
        (std::find(m_reg.begin(), m_reg.end(), instr->reg2) != m_reg.end() && !instr->reg2.empty());
    const bool match_reg0 = (itR != m_reg.end());

    // Exclude a few instruction types, such as compares
    bool hold_continue = false;
    for (auto& s : exclude)
    {
      if (instr->instruction.compare(0, s.length(), s) == 0)
        hold_continue = true;
    }

    if (!m_verbose->isChecked())
    {
      if (hold_continue)
        continue;

      // Output only where tracked items move to.
      if ((match_reg0 && instr->is_store) || (itM != m_mem.end() && instr->is_load) || match_reg12)
      {
        tmp_out.instruction = instr->instruction;
        tmp_out.mem_addr = instr->memory_dest;
        tmp_out.address = instr->address;
        m_trace_out.push_back(tmp_out);
      }
    }
    else if (match_reg12 || match_reg0 || itM != m_mem.end())
    {
      // Output all uses of tracked item.
      tmp_out.instruction = instr->instruction;
      tmp_out.mem_addr = instr->memory_dest;
      tmp_out.address = instr->address;
      m_trace_out.push_back(tmp_out);

      if (hold_continue)
        continue;
    }

    // Update tracking logic.
    // Save/Load
    if (instr->memory_dest)
    {
      // If using tracked memory. Add register to tracked if Load. Remove tracked memory if
      // overwritten with a store.
      if (itM != m_mem.end())
      {
        if (instr->is_load && !match_reg0)
          m_reg.push_back(instr->reg0);
        else if (instr->is_store && !match_reg0)
          m_mem.erase(itM);
      }
      else if (instr->is_store && match_reg0)
      {
        // If store but not using tracked memory & if storing tracked register, track memory
        // location too.
        m_mem.push_back(instr->memory_dest);
      }
      else if (instr->is_load && match_reg0 && !first_hit)
      {
        m_reg.erase(itR);
      }
    }
    else
    {
      // Other instructions
      // Skip if no matches. Happens most often.
      if (!match_reg0 && !match_reg12)
        continue;
      // If tracked register data is being stored in a new register, save new register.
      else if (match_reg12 && !match_reg0)
        m_reg.push_back(instr->reg0);
      // If tracked register is overwritten, stop tracking.
      else if (match_reg0 && !match_reg12 && !first_hit)
        m_reg.erase(itR);
    }

    // First hit will likely be start of value we want to track - not the end. So don't remove itR.
    if (match_reg0 || (match_reg12 && !match_reg0))
      first_hit = false;

    if ((m_reg.empty() && m_mem.empty()) || m_trace_out.size() >= m_max_trace)
      break;
  }
}

void CodeTraceDialog::Backtrace()
{
  std::vector<std::string> exclude{"dc", "ic", "mt", "c", "fc"};
  TraceOutput tmp_out;

  trace_itr_pair p = UpdateIterator();
  auto begin_itr = p.first;
  auto end_itr = p.second;

  for (auto instr = end_itr; instr != begin_itr; instr--)
  {
    // Not an instruction we care about
    if (instr->reg0.empty())
      continue;

    auto itR = std::find(m_reg.begin(), m_reg.end(), instr->reg0);
    auto itM = std::find(m_mem.begin(), m_mem.end(), instr->memory_dest);
    const bool match_reg1 =
        (std::find(m_reg.begin(), m_reg.end(), instr->reg1) != m_reg.end() && !instr->reg1.empty());
    const bool match_reg2 =
        (std::find(m_reg.begin(), m_reg.end(), instr->reg2) != m_reg.end() && !instr->reg2.empty());
    const bool match_reg0 = (itR != m_reg.end());

    // Exclude a few instruction types, such as compares
    bool hold_continue = false;
    for (auto& s : exclude)
    {
      if (instr->instruction.compare(0, s.length(), s) == 0)
        hold_continue = true;
    }

    // Write instructions to output.
    if (!m_verbose->isChecked())
    {
      if (hold_continue)
        continue;

      // Output only where tracked items came from.
      if ((match_reg0 && !instr->is_store) || (itM != m_mem.end() && instr->is_store))
      {
        tmp_out.instruction = instr->instruction;
        tmp_out.mem_addr = instr->memory_dest;
        tmp_out.address = instr->address;
        m_trace_out.push_back(tmp_out);
      }
    }
    else if ((match_reg1 || match_reg2 || match_reg0 || itM != m_mem.end()))
    {
      // Output stuff like compares if they contain a tracked register
      tmp_out.instruction = instr->instruction;
      tmp_out.mem_addr = instr->memory_dest;
      tmp_out.address = instr->address;
      m_trace_out.push_back(tmp_out);

      if (hold_continue)
        continue;
    }

    // Update Trace Logic.
    // Store/Load
    if (instr->memory_dest)
    {
      // Backtrace: what wrote to tracked memory & remove memory track. Else if: what loaded to
      // tracked register & remove register from track.
      if (itM != m_mem.end())
      {
        if (instr->is_store && !match_reg0)
        {
          m_reg.push_back(instr->reg0);
          m_mem.erase(itM);
        }
      }
      else if (instr->is_load && match_reg0)
      {
        m_mem.push_back(instr->memory_dest);
        m_reg.erase(itR);
      }
    }
    else
    {
      // Other instructions
      // Skip if we aren't watching output register. Happens most often.
      // Else: Erase tracked register and save what wrote to it.
      if (!match_reg0)
        continue;
      else if (!match_reg1 && !match_reg2)
        m_reg.erase(itR);

      // If tracked register is written, track r1 / r2.
      if (!match_reg1 && !instr->reg1.empty())
        m_reg.push_back(instr->reg1);
      if (!match_reg2 && !instr->reg2.empty())
        m_reg.push_back(instr->reg2);
    }

    // Stop if we run out of things to track
    if ((m_reg.empty() && m_mem.empty()) || m_trace_out.size() >= m_max_trace)
      break;
  }
}

void CodeTraceDialog::CodePath()
{
  // Shows entire trace without filtering if target input is blank.
  trace_itr_pair p = UpdateIterator();
  auto begin_itr = p.first;
  auto end_itr = p.second;

  TraceOutput tmp_out;
  if (m_backtrace->isChecked())
  {
    for (auto instr = end_itr - 1; instr != begin_itr && m_trace_out.size() < m_max_trace; instr--)
    {
      tmp_out.instruction = instr->instruction;
      tmp_out.mem_addr = instr->memory_dest;
      tmp_out.address = instr->address;
      m_trace_out.push_back(tmp_out);
    }
  }
  else
  {
    for (auto instr = begin_itr; instr != end_itr && m_trace_out.size() < m_max_trace; instr++)
    {
      tmp_out.instruction = instr->instruction;
      tmp_out.mem_addr = instr->memory_dest;
      tmp_out.address = instr->address;
      m_trace_out.push_back(tmp_out);
    }
  }
}

void CodeTraceDialog::DisplayTrace()
{
  m_trace_out.clear();
  m_reg.clear();
  m_mem.clear();
  m_output_list->clear();
  m_trace_out.reserve(m_max_trace);

  // Errors
  if (!m_error_msg.isEmpty())
  {
    new QListWidgetItem(m_error_msg, m_output_list);
    m_error_msg.clear();
  }
  if (m_code_trace.size() >= m_max_code_trace)
    new QListWidgetItem(tr("Trace max limit reached, backtrace won't work."), m_output_list);

  // Setup register to track.
  if (!m_trace_target->text().isEmpty())
  {
    QString reg_tmp = m_trace_target->text();
    reg_tmp.replace(QStringLiteral("sp"), QStringLiteral("r1"), Qt::CaseInsensitive);
    reg_tmp.replace(QStringLiteral("rtoc"), QStringLiteral("r2"), Qt::CaseInsensitive);
    m_reg.push_back(reg_tmp.toStdString());
  }

  if (m_trace_target->text().isEmpty())
    CodePath();
  else if (m_backtrace->isChecked())
    Backtrace();
  else
    ForwardTrace();

  if (m_trace_out.size() >= m_max_trace)
    new QListWidgetItem(tr("Max output size reached, stopped early"), m_output_list);

  m_sizes->setText(QStringLiteral("Trace: %1, List: %2")
                       .arg(QString::number(m_code_trace.size()))
                       .arg(QString::number(m_trace_out.size())));

  // Cleanup and prepare output, then send to Qlistwidget.
  std::regex reg("(\\S*)\\s+(?:(\\S{0,6})\\s*)?(?:(\\S{0,8})\\s*)?(?:(\\S{0,8})\\s*)?(.*)");
  std::smatch match;
  std::string is_mem;

  for (auto out : m_trace_out)
  {
    QString fix_sym = QString::fromStdString(g_symbolDB.GetDescription(out.address));
    fix_sym.replace(QStringLiteral("\t"), QStringLiteral("  "));

    std::regex_search(out.instruction, match, reg);

    if (out.mem_addr)
      is_mem = StringFromFormat("%x", out.mem_addr);
    else
      is_mem = match.str(5);

    auto* item = new QListWidgetItem(
        QString::fromStdString(StringFromFormat(
            "%08x : %-11s%-6s%-8s%-8s%-18s", out.address, match.str(1).c_str(),
            match.str(2).c_str(), match.str(3).c_str(), match.str(4).c_str(), is_mem.c_str())) +
        fix_sym);

    item->setData(ADDRESS_ROLE, out.address);
    if (out.mem_addr)
      item->setData(MEM_ADDRESS_ROLE, out.mem_addr);
    m_output_list->addItem(item);
  }
}

trace_itr_pair CodeTraceDialog::UpdateIterator()
{
  // If the range is changed for reprocessing, this will change the iterator accordingly.
  auto begin_itr = m_code_trace.begin();
  auto end_itr = m_code_trace.end();

  if (m_change_range->isChecked())
  {
    bool good;
    u32 start = m_bp1->currentText().toUInt(&good, 16);
    if (!good)
      start = m_bp1->currentData().toUInt(&good);
    if (!good)
      return std::make_pair(begin_itr, end_itr);

    u32 end = m_bp2->currentText().toUInt(&good, 16);
    if (!good)
      end = m_bp2->currentData().toUInt(&good);
    if (!good)
      return std::make_pair(begin_itr, end_itr);

    begin_itr = find_if(m_code_trace.begin(), m_code_trace.end(),
                        [start](const CodeTrace& t) { return t.address == start; });
    end_itr = find_if(m_code_trace.rbegin(), m_code_trace.rend(),
                      [end](const CodeTrace& t) { return t.address == end; })
                  .base();
  }
  return std::make_pair(begin_itr, end_itr);
}

void CodeTraceDialog::UpdateBreakpoints()
{
  if (m_run_trace->isChecked())
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

  for (auto& i : bp_vec)
  {
    QString instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(i.address));
    instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
    if (m_run_trace->isChecked())
    {
      m_bp1->addItem(QStringLiteral("%1 : %2").arg(i.address, 8, 16, QLatin1Char('0')).arg(instr),
                     i.address);
    }
    m_bp2->addItem(QStringLiteral("%1 : %2").arg(i.address, 8, 16, QLatin1Char('0')).arg(instr),
                   i.address);
  }
}

void CodeTraceDialog::InfoDisp()
{
  new QListWidgetItem(
      QStringLiteral(
          "Used to track a register or memory address and its uses.\nInputs:\nRegister: Input "
          "example "
          "r5, f31, use f for ps regusters..\n    Only takes one value at a time. Leave Blank "
          "to "
          "view complete "
          "code path. \n\nStarting Address: "
          "Used to change range for reprocessing. Run Trace's starting address is always the PC\n "
          "   (the current instruction while paused)."
          "Can change when reprocessing.\n\nEnding breakpoint: "
          "Where "
          "the trace will stop. If backtracing, should be the line you want to backtrace "
          "from.\n\nBacktrace: A reverse trace that shows where a value came from, the first "
          "output "
          "line "
          "is the most recent executed.\n    Backtracing = off will show where a "
          "tracked "
          "item "
          "is going "
          "after the starting point.\n\nVerbose: Will record all references to what is being "
          "tracked, rather than just where it is moving to/from.\n\nReset on loopback: Will clear "
          "the "
          "trace "
          "if starting address is looped through.\n    Insuring only the final loop to the end "
          "breakpoint is recorded.\n\nChange Range: Change the start and end points of the trace "
          "during reprocessing. Loops may make certain points buggy\n\nReprocess: You don't have "
          "to "
          "run a trace multiple times if "
          "the "
          "first trace covered the area of code you need.\nYou can change any value or option "
          "and "
          "do a "
          "retrace. Changing the second "
          "breakpoint\n   "
          "will let you backtrace from a new location."),
      m_output_list);
}

void CodeTraceDialog::OnContextMenu()
{
  QMenu* menu = new QMenu(this);
  menu->addAction(tr("Copy &address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->addAction(tr("Copy &memory address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(MEM_ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->exec(QCursor::pos());
}
