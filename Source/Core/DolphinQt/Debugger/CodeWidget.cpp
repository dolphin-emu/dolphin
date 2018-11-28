// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeWidget.h"

#include <chrono>

#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSplitter>
#include <QTableWidget>
#include <QWidget>

#include "Common/Event.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

CodeWidget::CodeWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Code"));
  setObjectName(QStringLiteral("code"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("codewidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("codewidget/floating")).toBool());

  connect(&Settings::Instance(), &Settings::CodeVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, [this] {
    if (Core::GetState() == Core::State::Paused)
      SetAddress(PowerPC::ppcState.pc, CodeViewWidget::SetAddressUpdate::WithoutUpdate);
    Update();
  });

  connect(Host::GetInstance(), &Host::NotifyMapLoaded, this, &CodeWidget::UpdateSymbols);

  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsCodeVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &CodeWidget::Update);

  setHidden(!Settings::Instance().IsCodeVisible() || !Settings::Instance().IsDebugModeEnabled());

  CreateWidgets();
  ConnectWidgets();

  m_code_splitter->restoreState(
      settings.value(QStringLiteral("codewidget/codesplitter")).toByteArray());
  m_box_splitter->restoreState(
      settings.value(QStringLiteral("codewidget/boxsplitter")).toByteArray());

  Update();
}

CodeWidget::~CodeWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("codewidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("codewidget/floating"), isFloating());
  settings.setValue(QStringLiteral("codewidget/codesplitter"), m_code_splitter->saveState());
  settings.setValue(QStringLiteral("codewidget/boxplitter"), m_box_splitter->saveState());
}

void CodeWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetCodeVisible(false);
}

void CodeWidget::CreateWidgets()
{
  auto* layout = new QGridLayout;

  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(0);

  m_search_address = new QLineEdit;
  m_search_symbols = new QLineEdit;
  m_code_view = new CodeViewWidget;

  m_search_address->setPlaceholderText(tr("Search Address"));
  m_search_symbols->setPlaceholderText(tr("Filter Symbols"));

  // Callstack
  auto* callstack_box = new QGroupBox(tr("Callstack"));
  auto* callstack_layout = new QVBoxLayout;
  m_callstack_list = new QListWidget;

  callstack_box->setLayout(callstack_layout);
  callstack_layout->addWidget(m_callstack_list);

  // Symbols
  auto* symbols_box = new QGroupBox(tr("Symbols"));
  auto* symbols_layout = new QVBoxLayout;
  m_symbols_list = new QListWidget;

  symbols_box->setLayout(symbols_layout);
  symbols_layout->addWidget(m_symbols_list);

  // Function calls
  auto* function_calls_box = new QGroupBox(tr("Function calls"));
  auto* function_calls_layout = new QVBoxLayout;
  m_function_calls_list = new QListWidget;

  function_calls_box->setLayout(function_calls_layout);
  function_calls_layout->addWidget(m_function_calls_list);

  // Function callers
  auto* function_callers_box = new QGroupBox(tr("Function callers"));
  auto* function_callers_layout = new QVBoxLayout;
  m_function_callers_list = new QListWidget;

  function_callers_box->setLayout(function_callers_layout);
  function_callers_layout->addWidget(m_function_callers_list);

  m_box_splitter = new QSplitter(Qt::Vertical);

  m_box_splitter->addWidget(callstack_box);
  m_box_splitter->addWidget(symbols_box);
  m_box_splitter->addWidget(function_calls_box);
  m_box_splitter->addWidget(function_callers_box);

  m_code_splitter = new QSplitter(Qt::Horizontal);

  m_code_splitter->addWidget(m_box_splitter);
  m_code_splitter->addWidget(m_code_view);

  layout->addWidget(m_search_address, 0, 0);
  layout->addWidget(m_search_symbols, 0, 1);
  layout->addWidget(m_code_splitter, 1, 0, -1, -1);

  QWidget* widget = new QWidget(this);
  widget->setLayout(layout);
  setWidget(widget);
}

void CodeWidget::ConnectWidgets()
{
  connect(m_search_address, &QLineEdit::textChanged, this, &CodeWidget::OnSearchAddress);
  connect(m_search_symbols, &QLineEdit::textChanged, this, &CodeWidget::OnSearchSymbols);

  connect(m_symbols_list, &QListWidget::itemClicked, this, &CodeWidget::OnSelectSymbol);
  connect(m_callstack_list, &QListWidget::itemSelectionChanged, this,
          &CodeWidget::OnSelectCallstack);
  connect(m_function_calls_list, &QListWidget::itemSelectionChanged, this,
          &CodeWidget::OnSelectFunctionCalls);
  connect(m_function_callers_list, &QListWidget::itemSelectionChanged, this,
          &CodeWidget::OnSelectFunctionCallers);

  connect(m_code_view, &CodeViewWidget::SymbolsChanged, this, &CodeWidget::UpdateSymbols);
  connect(m_code_view, &CodeViewWidget::BreakpointsChanged, this,
          [this] { emit BreakpointsChanged(); });
  connect(m_code_view, &CodeViewWidget::SendSearchValue, this,
          [this](u32 addr) { emit SendSearchValue(addr); });
  connect(m_code_view, &CodeViewWidget::RequestPPCComparison, this,
          &CodeWidget::RequestPPCComparison);
}

void CodeWidget::OnSearchAddress()
{
  bool good = true;
  u32 address = m_search_address->text().toUInt(&good, 16);

  QPalette palette;
  QFont font;

  if (!good && !m_search_address->text().isEmpty())
  {
    font.setBold(true);
    palette.setColor(QPalette::Text, Qt::red);
  }

  m_search_address->setPalette(palette);
  m_search_address->setFont(font);

  if (good)
    m_code_view->SetAddress(address, CodeViewWidget::SetAddressUpdate::WithUpdate);

  Update();

  m_search_address->setFocus();
}

void CodeWidget::OnSearchSymbols()
{
  m_symbol_filter = m_search_symbols->text();
  UpdateSymbols();
}

void CodeWidget::OnSelectSymbol()
{
  const auto items = m_symbols_list->selectedItems();
  if (items.isEmpty())
    return;

  const u32 address = items[0]->data(Qt::UserRole).toUInt();
  const Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(address);

  m_code_view->SetAddress(address, CodeViewWidget::SetAddressUpdate::WithUpdate);
  UpdateCallstack();
  UpdateFunctionCalls(symbol);
  UpdateFunctionCallers(symbol);

  m_code_view->setFocus();
}

void CodeWidget::OnSelectCallstack()
{
  const auto items = m_callstack_list->selectedItems();
  if (items.isEmpty())
    return;

  m_code_view->SetAddress(items[0]->data(Qt::UserRole).toUInt(),
                          CodeViewWidget::SetAddressUpdate::WithUpdate);
  Update();
}

void CodeWidget::OnSelectFunctionCalls()
{
  const auto items = m_function_calls_list->selectedItems();
  if (items.isEmpty())
    return;

  m_code_view->SetAddress(items[0]->data(Qt::UserRole).toUInt(),
                          CodeViewWidget::SetAddressUpdate::WithUpdate);
  Update();
}

void CodeWidget::OnSelectFunctionCallers()
{
  const auto items = m_function_callers_list->selectedItems();
  if (items.isEmpty())
    return;

  m_code_view->SetAddress(items[0]->data(Qt::UserRole).toUInt(),
                          CodeViewWidget::SetAddressUpdate::WithUpdate);
  Update();
}

void CodeWidget::SetAddress(u32 address, CodeViewWidget::SetAddressUpdate update)
{
  m_code_view->SetAddress(address, update);
}

void CodeWidget::Update()
{
  const Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(m_code_view->GetAddress());

  UpdateCallstack();

  m_code_view->Update();
  m_code_view->setFocus();

  if (!symbol)
    return;

  UpdateFunctionCalls(symbol);
  UpdateFunctionCallers(symbol);
}

void CodeWidget::UpdateCallstack()
{
  if (Core::GetState() == Core::State::Starting)
    return;

  m_callstack_list->clear();

  std::vector<Dolphin_Debugger::CallstackEntry> stack;

  bool success = Dolphin_Debugger::GetCallstack(stack);

  if (!success)
  {
    m_callstack_list->addItem(tr("Invalid callstack"));
    return;
  }

  for (const auto& frame : stack)
  {
    auto* item =
        new QListWidgetItem(QString::fromStdString(frame.Name.substr(0, frame.Name.length() - 1)));
    item->setData(Qt::UserRole, frame.vAddress);

    m_callstack_list->addItem(item);
  }
}

void CodeWidget::UpdateSymbols()
{
  QString selection = m_symbols_list->selectedItems().isEmpty() ?
                          QStringLiteral("") :
                          m_symbols_list->selectedItems()[0]->text();
  m_symbols_list->clear();

  for (const auto& symbol : g_symbolDB.Symbols())
  {
    QString name = QString::fromStdString(symbol.second.name);

    auto* item = new QListWidgetItem(name);
    if (name == selection)
      item->setSelected(true);

    // Disable non-function symbols as you can't do anything with them.
    if (symbol.second.type != Common::Symbol::Type::Function)
      item->setFlags(Qt::NoItemFlags);

    item->setData(Qt::UserRole, symbol.second.address);

    if (name.toUpper().indexOf(m_symbol_filter.toUpper()) != -1)
      m_symbols_list->addItem(item);
  }

  m_symbols_list->sortItems();
}

void CodeWidget::UpdateFunctionCalls(const Common::Symbol* symbol)
{
  m_function_calls_list->clear();

  for (const auto& call : symbol->calls)
  {
    const u32 addr = call.function;
    const Common::Symbol* call_symbol = g_symbolDB.GetSymbolFromAddr(addr);

    if (call_symbol)
    {
      auto* item = new QListWidgetItem(
          QString::fromStdString(StringFromFormat("> %s (%08x)", call_symbol->name.c_str(), addr)));
      item->setData(Qt::UserRole, addr);

      m_function_calls_list->addItem(item);
    }
  }
}

void CodeWidget::UpdateFunctionCallers(const Common::Symbol* symbol)
{
  m_function_callers_list->clear();

  for (const auto& caller : symbol->callers)
  {
    const u32 addr = caller.call_address;
    const Common::Symbol* caller_symbol = g_symbolDB.GetSymbolFromAddr(addr);

    if (caller_symbol)
    {
      auto* item = new QListWidgetItem(QString::fromStdString(
          StringFromFormat("< %s (%08x)", caller_symbol->name.c_str(), addr)));
      item->setData(Qt::UserRole, addr);

      m_function_callers_list->addItem(item);
    }
  }
}

void CodeWidget::Step()
{
  if (!CPU::IsStepping())
    return;

  Common::Event sync_event;

  PowerPC::CoreMode old_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
  PowerPC::breakpoints.ClearAllTemporary();
  CPU::StepOpcode(&sync_event);
  sync_event.WaitFor(std::chrono::milliseconds(20));
  PowerPC::SetMode(old_mode);
  Core::DisplayMessage(tr("Step successful!").toStdString(), 2000);
  // Will get a UpdateDisasmDialog(), don't update the GUI here.
}

void CodeWidget::StepOver()
{
  if (!CPU::IsStepping())
    return;

  UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
  if (inst.LK)
  {
    PowerPC::breakpoints.ClearAllTemporary();
    PowerPC::breakpoints.Add(PC + 4, true);
    CPU::EnableStepping(false);
    Core::DisplayMessage(tr("Step over in progress...").toStdString(), 2000);
  }
  else
  {
    Step();
  }
}

// Returns true on a rfi, blr or on a bclr that evaluates to true.
static bool WillInstructionReturn(UGeckoInstruction inst)
{
  // Is a rfi instruction
  if (inst.hex == 0x4C000064u)
    return true;
  bool counter = (inst.BO_2 >> 2 & 1) != 0 || (CTR != 0) != ((inst.BO_2 >> 1 & 1) != 0);
  bool condition = inst.BO_2 >> 4 != 0 || PowerPC::GetCRBit(inst.BI_2) == (inst.BO_2 >> 3 & 1);
  bool isBclr = inst.OPCD_7 == 0b010011 && (inst.hex >> 1 & 0b10000) != 0;
  return isBclr && counter && condition && !inst.LK_3;
}

void CodeWidget::StepOut()
{
  if (!CPU::IsStepping())
    return;

  CPU::PauseAndLock(true, false);
  PowerPC::breakpoints.ClearAllTemporary();

  // Keep stepping until the next return instruction or timeout after five seconds
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(5);
  PowerPC::CoreMode old_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);

  // Loop until either the current instruction is a return instruction with no Link flag
  // or a breakpoint is detected so it can step at the breakpoint. If the PC is currently
  // on a breakpoint, skip it.
  UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
  do
  {
    if (WillInstructionReturn(inst))
    {
      PowerPC::SingleStep();
      break;
    }

    if (inst.LK)
    {
      // Step over branches
      u32 next_pc = PC + 4;
      do
      {
        PowerPC::SingleStep();
      } while (PC != next_pc && clock::now() < timeout &&
               !PowerPC::breakpoints.IsAddressBreakPoint(PC));
    }
    else
    {
      PowerPC::SingleStep();
    }

    inst = PowerPC::HostRead_Instruction(PC);
  } while (clock::now() < timeout && !PowerPC::breakpoints.IsAddressBreakPoint(PC));

  PowerPC::SetMode(old_mode);
  CPU::PauseAndLock(false, false);

  emit Host::GetInstance()->UpdateDisasmDialog();

  if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
    Core::DisplayMessage(tr("Breakpoint encountered! Step out aborted.").toStdString(), 2000);
  else if (clock::now() >= timeout)
    Core::DisplayMessage(tr("Step out timed out!").toStdString(), 2000);
  else
    Core::DisplayMessage(tr("Step out successful!").toStdString(), 2000);
}

void CodeWidget::Skip()
{
  PC += 4;
  ShowPC();
}

void CodeWidget::ShowPC()
{
  m_code_view->SetAddress(PC, CodeViewWidget::SetAddressUpdate::WithUpdate);
  Update();
}

void CodeWidget::SetPC()
{
  PC = m_code_view->GetAddress();
  Update();
}

void CodeWidget::ToggleBreakpoint()
{
  m_code_view->ToggleBreakpoint();
}

void CodeWidget::AddBreakpoint()
{
  m_code_view->AddBreakpoint();
}
