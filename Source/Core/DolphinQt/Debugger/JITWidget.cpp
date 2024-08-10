// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/JITWidget.h"

#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <fmt/format.h>

#include "Common/GekkoDisassembler.h"
#include "Core/Core.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/System.h"
#include "UICommon/Disassembler.h"

#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

JITWidget::JITWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("JIT Blocks"));
  setObjectName(QStringLiteral("jitwidget"));

  setHidden(!Settings::Instance().IsJITVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  const auto& settings = Settings::GetQSettings();

  CreateWidgets();

  restoreGeometry(settings.value(QStringLiteral("jitwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("jitwidget/floating")).toBool());

  m_table_splitter->restoreState(
      settings.value(QStringLiteral("jitwidget/tablesplitter")).toByteArray());
  m_asm_splitter->restoreState(
      settings.value(QStringLiteral("jitwidget/asmsplitter")).toByteArray());

  connect(&Settings::Instance(), &Settings::JITVisibilityChanged, this,
          [this](const bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this,
          [this](const bool enabled) { setHidden(!enabled || !Settings::Instance().IsJITVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &JITWidget::Update);
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &JITWidget::Update);

  ConnectWidgets();

#if defined(_M_X86_64)
  m_disassembler = GetNewDisassembler("x86");
#elif defined(_M_ARM_64)
  m_disassembler = GetNewDisassembler("aarch64");
#else
  m_disassembler = GetNewDisassembler("UNK");
#endif
}

JITWidget::~JITWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("jitwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("jitwidget/floating"), isFloating());
  settings.setValue(QStringLiteral("jitwidget/tablesplitter"), m_table_splitter->saveState());
  settings.setValue(QStringLiteral("jitwidget/asmsplitter"), m_asm_splitter->saveState());
}

void JITWidget::CreateWidgets()
{
  m_table_widget = new QTableWidget;

  m_table_widget->setTabKeyNavigation(false);
  m_table_widget->setColumnCount(7);
  m_table_widget->setHorizontalHeaderLabels(
      {tr("Address"), tr("PPC Size"), tr("Host Size"),
       // i18n: The symbolic name of a code block
       tr("Symbol"),
       // i18n: These are the kinds of flags that a CPU uses (e.g. carry),
       // not the kinds of flags that represent e.g. countries
       tr("Flags"),
       // i18n: The number of times a code block has been executed
       tr("NumExec"),
       // i18n: Performance cost, not monetary cost
       tr("Cost")});

  m_ppc_asm_widget = new QTextBrowser;
  m_host_asm_widget = new QTextBrowser;

  m_table_splitter = new QSplitter(Qt::Vertical);
  m_asm_splitter = new QSplitter(Qt::Horizontal);

  m_refresh_button = new QPushButton(tr("Refresh"));

  m_table_splitter->addWidget(m_table_widget);
  m_table_splitter->addWidget(m_asm_splitter);

  m_asm_splitter->addWidget(m_ppc_asm_widget);
  m_asm_splitter->addWidget(m_host_asm_widget);

  auto widget = new QWidget;
  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(2, 2, 2, 2);
  widget->setLayout(layout);

  layout->addWidget(m_table_splitter);
  layout->addWidget(m_refresh_button);

  setWidget(widget);
}

void JITWidget::ConnectWidgets()
{
  connect(m_refresh_button, &QPushButton::clicked, this, &JITWidget::Update);
}

void JITWidget::Compare(const u32 address)
{
  m_address = address;

  Settings::Instance().ShowJIT();
  raise();
  m_host_asm_widget->setFocus();

  Update();
}

void JITWidget::Update()
{
  if (!isVisible())
    return;

  if (!m_address || (GetState(Core::System::GetInstance()) != Core::State::Paused))
  {
    m_ppc_asm_widget->setHtml(QStringLiteral("<i>%1</i>").arg(tr("(ppc)")));
    m_host_asm_widget->setHtml(QStringLiteral("<i>%1</i>").arg(tr("(host)")));
    return;
  }

  // TODO: Actually do something with the table (Wx doesn't)

  // Get host side code disassembly
  auto [text, entry_address, instruction_count, code_size] =
    DisassembleBlock(m_disassembler.get(), m_address);
  m_address = entry_address;

  m_host_asm_widget->setHtml(
      QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(text)));

  // == Fill in ppc box
  const u32 ppc_addr = m_address;
  PPCAnalyst::CodeBuffer code_buffer(32000);
  PPCAnalyst::BlockStats st;
  PPCAnalyst::BlockRegStats gpa;
  PPCAnalyst::BlockRegStats fpa;
  PPCAnalyst::CodeBlock code_block;
  PPCAnalyst::PPCAnalyzer analyzer;
  Config::IsDebuggingEnabled() ? analyzer.EnableDebugging() : analyzer.DisableDebugging();
  Get(Config::MAIN_JIT_FOLLOW_BRANCH) ?
    analyzer.EnableBranchFollowing() :
    analyzer.DisableBranchFollowing();
  Get(Config::MAIN_FLOAT_EXCEPTIONS) ?
    analyzer.EnableFloatExceptions() :
    analyzer.DisableFloatExceptions();
  Get(Config::MAIN_DIVIDE_BY_ZERO_EXCEPTIONS) ?
    analyzer.EnableDivByZeroExceptions() :
    analyzer.DisableDivByZeroExceptions();
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);

  code_block.m_stats = &st;
  code_block.m_gpa = &gpa;
  code_block.m_fpa = &fpa;

  if (analyzer.Analyze(ppc_addr, &code_block, &code_buffer, code_buffer.size()) != 0xFFFFFFFF)
  {
    std::string ppc_disasm_str;
    const auto ppc_disasm = std::back_inserter(ppc_disasm_str);
    for (u32 i = 0; i < code_block.m_num_instructions; i++)
    {
      const auto& [inst, _opinfo, address, _branchTo, _regsIn, _regsOut, _fregsIn, _fregOut, _crIn,
            _crOut, _branchUsesCtr, _branchIsIdleLoop, _wantsCR, _wantsFPRF, _wantsCA,
            _wantsCAInFlags, _outputCR, _outputFPRF, _outputCA, _canEndBlock, _canCauseException,
            _skipLRStack, _skip, _crInUse, _crDiscardable, _fprInUse, _gprInUse, _gprDiscardable,
            _fprDiscardable, _fprInXmm, _fprIsSingle, _fprIsDuplicated, _fprIsStoreSafeBeforeInst,
            _fprIsStoreSafeAfterInst] = code_buffer[i];
      const std::string opcode = Common::GekkoDisassembler::Disassemble(inst.hex, address);
      fmt::format_to(ppc_disasm, "{:08x} {}\n", address, opcode);
    }

    // Add stats to the end of the ppc box since it's generally the shortest.
    fmt::format_to(ppc_disasm, "\n{} estimated cycles", st.numCycles);
    fmt::format_to(ppc_disasm, "\nNum instr: PPC: {} Host: {}", code_block.m_num_instructions,
                   instruction_count);
    if (code_block.m_num_instructions != 0 && instruction_count != 0)
    {
      fmt::format_to(
          ppc_disasm, " (blowup: {}%)",
          100 * instruction_count / code_block.m_num_instructions - 100);
    }

    fmt::format_to(ppc_disasm, "\nNum bytes: PPC: {} Host: {}", code_block.m_num_instructions * 4,
                   code_size);
    if (code_block.m_num_instructions != 0 && code_size != 0)
    {
      fmt::format_to(
          ppc_disasm, " (blowup: {}%)",
          100 * code_size / (4 * code_block.m_num_instructions) - 100);
    }

    m_ppc_asm_widget->setHtml(
        QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(ppc_disasm_str)));
  }
  else
  {
    m_host_asm_widget->setHtml(
        QStringLiteral("<pre>%1</pre>")
            .arg(QString::fromStdString(fmt::format("(non-code address: {:08x})", m_address))));
    m_ppc_asm_widget->setHtml(QStringLiteral("<i>---</i>"));
  }
}

void JITWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().HideJIT();
}

void JITWidget::showEvent(QShowEvent* event)
{
  Update();
}
