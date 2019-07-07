// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/JITWidget.h"

#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "UICommon/Disassembler.h"

#include "DolphinQt/Settings.h"

JITWidget::JITWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("JIT Blocks"));
  setObjectName(QStringLiteral("jitwidget"));

  setHidden(!Settings::Instance().IsJITVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  CreateWidgets();

  restoreGeometry(settings.value(QStringLiteral("jitwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("jitwidget/floating")).toBool());

  m_table_splitter->restoreState(
      settings.value(QStringLiteral("jitwidget/tablesplitter")).toByteArray());
  m_asm_splitter->restoreState(
      settings.value(QStringLiteral("jitwidget/asmsplitter")).toByteArray());

  connect(&Settings::Instance(), &Settings::JITVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsJITVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &JITWidget::Update);

  ConnectWidgets();

#if defined(_M_X86)
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

  QWidget* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(2, 2, 2, 2);
  widget->setLayout(layout);

  layout->addWidget(m_table_splitter);
  layout->addWidget(m_refresh_button);

  setWidget(widget);
}

void JITWidget::ConnectWidgets()
{
  connect(m_refresh_button, &QPushButton::pressed, this, &JITWidget::Update);
}

void JITWidget::Compare(u32 address)
{
  m_address = address;
  Update();
}

void JITWidget::Update()
{
  if (!isVisible())
    return;

  if (!m_address)
  {
    m_ppc_asm_widget->setHtml(QStringLiteral("<i>%1</i>").arg(tr("(ppc)")));
    m_host_asm_widget->setHtml(QStringLiteral("<i>%1</i>").arg(tr("(host)")));
    return;
  }

  // TODO: Actually do something with the table (Wx doesn't)

  // Get host side code disassembly
  u32 host_instructions_count = 0;
  u32 host_code_size = 0;
  std::string host_instructions_disasm;
  host_instructions_disasm =
      DisassembleBlock(m_disassembler.get(), &m_address, &host_instructions_count, &host_code_size);

  m_host_asm_widget->setHtml(
      QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(host_instructions_disasm)));

  // == Fill in ppc box
  u32 ppc_addr = m_address;
  PPCAnalyst::CodeBuffer code_buffer(32000);
  PPCAnalyst::BlockStats st;
  PPCAnalyst::BlockRegStats gpa;
  PPCAnalyst::BlockRegStats fpa;
  PPCAnalyst::CodeBlock code_block;
  PPCAnalyst::PPCAnalyzer analyzer;
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);

  code_block.m_stats = &st;
  code_block.m_gpa = &gpa;
  code_block.m_fpa = &fpa;

  if (analyzer.Analyze(ppc_addr, &code_block, &code_buffer, code_buffer.size()) != 0xFFFFFFFF)
  {
    std::ostringstream ppc_disasm;
    for (u32 i = 0; i < code_block.m_num_instructions; i++)
    {
      const PPCAnalyst::CodeOp& op = code_buffer[i];
      const std::string opcode = Common::GekkoDisassembler::Disassemble(op.inst.hex, op.address);
      ppc_disasm << std::setfill('0') << std::setw(8) << std::hex << op.address;
      ppc_disasm << " " << opcode << std::endl;
    }

    // Add stats to the end of the ppc box since it's generally the shortest.
    ppc_disasm << std::dec << std::endl;

    // Add some generic analysis
    if (st.isFirstBlockOfFunction)
      ppc_disasm << "(first block of function)" << std::endl;
    if (st.isLastBlockOfFunction)
      ppc_disasm << "(last block of function)" << std::endl;

    ppc_disasm << st.numCycles << " estimated cycles" << std::endl;

    ppc_disasm << "Num instr: PPC: " << code_block.m_num_instructions
               << " Host: " << host_instructions_count << " (blowup: "
               << 100 * host_instructions_count / code_block.m_num_instructions - 100 << "%)"
               << std::endl;

    ppc_disasm << "Num bytes: PPC: " << code_block.m_num_instructions * 4
               << " Host: " << host_code_size
               << " (blowup: " << 100 * host_code_size / (4 * code_block.m_num_instructions) - 100
               << "%)" << std::endl;

    m_ppc_asm_widget->setHtml(
        QStringLiteral("<pre>%1</pre>").arg(QString::fromStdString(ppc_disasm.str())));
  }
  else
  {
    m_host_asm_widget->setHtml(
        QStringLiteral("<pre>%1</pre>")
            .arg(QString::fromStdString(StringFromFormat("(non-code address: %08x)", m_address))));
    m_ppc_asm_widget->setHtml(QStringLiteral("<i>---</i>"));
  }
}

void JITWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetJITVisible(false);
}

void JITWidget::showEvent(QShowEvent* event)
{
  Update();
}
