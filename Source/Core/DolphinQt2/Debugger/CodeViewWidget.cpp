// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Debugger/CodeViewWidget.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTableWidgetItem>
#include <QWheelEvent>

#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/Host.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt2/Debugger/CodeWidget.h"
#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Settings.h"

constexpr size_t VALID_BRANCH_LENGTH = 10;

CodeViewWidget::CodeViewWidget()
{
  setColumnCount(5);
  setShowGrid(false);
  setContextMenuPolicy(Qt::CustomContextMenu);
  setSelectionMode(QAbstractItemView::SingleSelection);
  verticalScrollBar()->setHidden(true);

  for (int i = 0; i < columnCount(); i++)
  {
    horizontalHeader()->setSectionResizeMode(i, i == 0 ? QHeaderView::Fixed :
                                                         QHeaderView::ResizeToContents);
  }

  verticalHeader()->hide();
  horizontalHeader()->hide();
  horizontalHeader()->setStretchLastSection(true);
  horizontalHeader()->resizeSection(0, 32);

  setFont(Settings::Instance().GetDebugFont());

  Update();

  connect(this, &CodeViewWidget::customContextMenuRequested, this, &CodeViewWidget::OnContextMenu);
  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &QWidget::setFont);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] {
    m_address = PC;
    Update();
  });
}

static u32 GetBranchFromAddress(u32 addr)
{
  std::string disasm = PowerPC::debug_interface.Disassemble(addr);
  size_t pos = disasm.find("->0x");

  if (pos == std::string::npos)
    return 0;

  std::string hex = disasm.substr(pos + 2);
  return std::stoul(hex, nullptr, 16);
}

void CodeViewWidget::Update()
{
  if (m_updating)
    return;

  m_updating = true;

  clearSelection();
  if (rowCount() == 0)
    setRowCount(1);

  // Calculate (roughly) how many rows will fit in our table
  int rows = std::round((height() / static_cast<float>(rowHeight(0))) - 0.25);

  setRowCount(rows);

  for (int i = 0; i < rows; i++)
    setRowHeight(i, 24);

  u32 pc = PowerPC::ppcState.pc;

  if (PowerPC::debug_interface.IsBreakpoint(pc))
    Core::SetState(Core::State::Paused);

  for (int i = 0; i < rowCount(); i++)
  {
    u32 addr = m_address - ((rowCount() / 2) * 4) + i * 4;
    u32 color = PowerPC::debug_interface.GetColor(addr);
    auto* bp_item = new QTableWidgetItem;
    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    std::string disas = PowerPC::debug_interface.Disassemble(addr);
    auto split = disas.find('\t');

    std::string ins = (split == std::string::npos ? disas : disas.substr(0, split));
    std::string param = (split == std::string::npos ? "" : disas.substr(split + 1));
    std::string desc = PowerPC::debug_interface.GetDescription(addr);

    auto* ins_item = new QTableWidgetItem(QString::fromStdString(ins));
    auto* param_item = new QTableWidgetItem(QString::fromStdString(param));
    auto* description_item = new QTableWidgetItem(QString::fromStdString(desc));

    // look for hex strings to decode branches
    std::string hex_str;
    size_t pos = param.find("0x");
    if (pos != std::string::npos)
    {
      hex_str = param.substr(pos);
    }

    if (hex_str.length() == VALID_BRANCH_LENGTH && desc != "---")
    {
      description_item->setText(tr("--> %1").arg(QString::fromStdString(
          PowerPC::debug_interface.GetDescription(GetBranchFromAddress(addr)))));
      param_item->setForeground(Qt::magenta);
    }

    if (ins == "blr")
      ins_item->setForeground(Qt::darkGreen);

    for (auto* item : {bp_item, addr_item, ins_item, param_item, description_item})
    {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setData(Qt::UserRole, addr);

      if (color != 0xFFFFFF)
        item->setBackground(QColor(color).darker(200));

      if (addr == pc && item != bp_item)
      {
        item->setBackground(Qt::darkGreen);
      }
    }

    if (PowerPC::debug_interface.IsBreakpoint(addr))
    {
      bp_item->setBackground(Qt::red);
    }

    setItem(i, 0, bp_item);
    setItem(i, 1, addr_item);
    setItem(i, 2, ins_item);
    setItem(i, 3, param_item);
    setItem(i, 4, description_item);

    if (addr == GetAddress())
    {
      addr_item->setSelected(true);
    }
  }

  g_symbolDB.FillInCallers();

  repaint();
  m_updating = false;
}

u32 CodeViewWidget::GetAddress() const
{
  return m_address;
}

void CodeViewWidget::SetAddress(u32 address)
{
  if (m_address == address)
    return;

  m_address = address;
  Update();
}

void CodeViewWidget::ReplaceAddress(u32 address, bool blr)
{
  auto found = std::find_if(m_repl_list.begin(), m_repl_list.end(),
                            [address](ReplStruct r) { return r.address == address; });

  if (found != m_repl_list.end())
  {
    PowerPC::debug_interface.WriteExtraMemory(0, (*found).old_value, address);
    m_repl_list.erase(found);
  }
  else
  {
    ReplStruct repl;

    repl.address = address;
    repl.old_value = PowerPC::debug_interface.ReadInstruction(address);

    m_repl_list.push_back(repl);

    PowerPC::debug_interface.Patch(address, blr ? 0x60000000 : 0x4e800020);
  }

  Update();
}

void CodeViewWidget::OnContextMenu()
{
  QMenu* menu = new QMenu(this);

  bool running = Core::GetState() != Core::State::Uninitialized;

  const u32 addr = GetContextAddress();

  bool has_symbol = g_symbolDB.GetSymbolFromAddr(addr);

  auto* follow_branch_action =
      AddAction(menu, tr("Follow &branch"), this, &CodeViewWidget::OnFollowBranch);

  menu->addSeparator();

  AddAction(menu, tr("&Copy address"), this, &CodeViewWidget::OnCopyAddress);
  auto* copy_address_action =
      AddAction(menu, tr("Copy &function"), this, &CodeViewWidget::OnCopyFunction);
  auto* copy_line_action =
      AddAction(menu, tr("Copy code &line"), this, &CodeViewWidget::OnCopyCode);
  auto* copy_hex_action = AddAction(menu, tr("Copy &hex"), this, &CodeViewWidget::OnCopyHex);
  menu->addSeparator();

  auto* symbol_rename_action =
      AddAction(menu, tr("&Rename symbol"), this, &CodeViewWidget::OnRenameSymbol);
  auto* symbol_size_action =
      AddAction(menu, tr("Set symbol &size"), this, &CodeViewWidget::OnSetSymbolSize);
  auto* symbol_end_action =
      AddAction(menu, tr("Set symbol &end address"), this, &CodeViewWidget::OnSetSymbolEndAddress);
  menu->addSeparator();

  AddAction(menu, tr("Run &To Here"), this, &CodeViewWidget::OnRunToHere);
  auto* function_action =
      AddAction(menu, tr("&Add function"), this, &CodeViewWidget::OnAddFunction);
  auto* ppc_action = AddAction(menu, tr("PPC vs x86"), this, &CodeViewWidget::OnPPCComparison);
  auto* insert_blr_action = AddAction(menu, tr("&Insert blr"), this, &CodeViewWidget::OnInsertBLR);
  auto* insert_nop_action = AddAction(menu, tr("Insert &nop"), this, &CodeViewWidget::OnInsertNOP);
  auto* replace_action =
      AddAction(menu, tr("Re&place instruction"), this, &CodeViewWidget::OnReplaceInstruction);

  follow_branch_action->setEnabled(running && GetBranchFromAddress(addr));

  for (auto* action : {copy_address_action, copy_line_action, copy_hex_action, function_action,
                       ppc_action, insert_blr_action, insert_nop_action, replace_action})
    action->setEnabled(running);

  for (auto* action : {symbol_rename_action, symbol_size_action, symbol_end_action})
    action->setEnabled(has_symbol);

  menu->exec(QCursor::pos());
  Update();
}

void CodeViewWidget::OnCopyAddress()
{
  const u32 addr = GetContextAddress();

  QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
}

void CodeViewWidget::OnCopyCode()
{
  const u32 addr = GetContextAddress();

  QApplication::clipboard()->setText(
      QString::fromStdString(PowerPC::debug_interface.Disassemble(addr)));
}

void CodeViewWidget::OnCopyFunction()
{
  const u32 address = GetContextAddress();

  Symbol* symbol = g_symbolDB.GetSymbolFromAddr(address);
  if (!symbol)
    return;

  std::string text = symbol->name + "\r\n";
  // we got a function
  u32 start = symbol->address;
  u32 end = start + symbol->size;
  for (u32 addr = start; addr != end; addr += 4)
  {
    std::string disasm = PowerPC::debug_interface.Disassemble(addr);
    text += StringFromFormat("%08x: ", addr) + disasm + "\r\n";
  }

  QApplication::clipboard()->setText(QString::fromStdString(text));
}

void CodeViewWidget::OnCopyHex()
{
  const u32 addr = GetContextAddress();
  const u32 instruction = PowerPC::debug_interface.ReadInstruction(addr);

  QApplication::clipboard()->setText(
      QStringLiteral("%1").arg(instruction, 8, 16, QLatin1Char('0')));
}

void CodeViewWidget::OnRunToHere()
{
  const u32 addr = GetContextAddress();

  PowerPC::debug_interface.SetBreakpoint(addr);
  PowerPC::debug_interface.RunToBreakpoint();
  Update();
}

void CodeViewWidget::OnPPCComparison()
{
  const u32 addr = GetContextAddress();

  emit RequestPPCComparison(addr);
}

void CodeViewWidget::OnAddFunction()
{
  const u32 addr = GetContextAddress();

  g_symbolDB.AddFunction(addr);
  emit SymbolsChanged();
  Update();
}

void CodeViewWidget::OnInsertBLR()
{
  const u32 addr = GetContextAddress();

  ReplaceAddress(addr, 0);
}

void CodeViewWidget::OnInsertNOP()
{
  const u32 addr = GetContextAddress();

  ReplaceAddress(addr, 1);
}

void CodeViewWidget::OnFollowBranch()
{
  const u32 addr = GetContextAddress();

  u32 branch_addr = GetBranchFromAddress(addr);

  if (!branch_addr)
    return;

  SetAddress(branch_addr);
}

void CodeViewWidget::OnRenameSymbol()
{
  const u32 addr = GetContextAddress();

  Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);

  if (!symbol)
    return;

  bool good;
  QString name =
      QInputDialog::getText(this, tr("Rename symbol"), tr("Symbol name:"), QLineEdit::Normal,
                            QString::fromStdString(symbol->name), &good);

  if (good && !name.isEmpty())
  {
    symbol->Rename(name.toStdString());
    emit SymbolsChanged();
    Update();
  }
}

void CodeViewWidget::OnSetSymbolSize()
{
  const u32 addr = GetContextAddress();

  Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);

  if (!symbol)
    return;

  bool good;
  int size =
      QInputDialog::getInt(this, tr("Rename symbol"),
                           tr("Set symbol size (%1):").arg(QString::fromStdString(symbol->name)),
                           symbol->size, 1, 0xFFFF, 1, &good);

  if (!good)
    return;

  PPCAnalyst::ReanalyzeFunction(symbol->address, *symbol, size);
  emit SymbolsChanged();
  Update();
}

void CodeViewWidget::OnSetSymbolEndAddress()
{
  const u32 addr = GetContextAddress();

  Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);

  if (!symbol)
    return;

  bool good;
  QString name = QInputDialog::getText(
      this, tr("Set symbol end address"),
      tr("Symbol (%1) end address:").arg(QString::fromStdString(symbol->name)), QLineEdit::Normal,
      QStringLiteral("%1").arg(addr + symbol->size, 8, 16, QLatin1Char('0')), &good);

  u32 address = name.toUInt(&good, 16);

  if (!good)
    return;

  PPCAnalyst::ReanalyzeFunction(symbol->address, *symbol, address - symbol->address);
  emit SymbolsChanged();
  Update();
}

void CodeViewWidget::OnReplaceInstruction()
{
  const u32 addr = GetContextAddress();

  if (!PowerPC::HostIsInstructionRAMAddress(addr))
    return;

  const PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(addr);
  if (!read_result.valid)
    return;

  bool good;
  QString name = QInputDialog::getText(
      this, tr("Change instruction"), tr("New instruction:"), QLineEdit::Normal,
      QStringLiteral("%1").arg(read_result.hex, 8, 16, QLatin1Char('0')), &good);

  u32 code = name.toUInt(&good, 16);

  if (good)
  {
    PowerPC::debug_interface.Patch(addr, code);
    Update();
  }
}

void CodeViewWidget::resizeEvent(QResizeEvent*)
{
  Update();
}

void CodeViewWidget::keyPressEvent(QKeyEvent* event)
{
  switch (event->key())
  {
  case Qt::Key_Up:
    m_address -= 3 * sizeof(u32);
    Update();
    return;
  case Qt::Key_Down:
    m_address += 3 * sizeof(u32);
    Update();
    return;
  case Qt::Key_PageUp:
    m_address -= rowCount() * sizeof(u32);
    Update();
    return;
  case Qt::Key_PageDown:
    m_address += rowCount() * sizeof(u32);
    Update();
    return;
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

void CodeViewWidget::wheelEvent(QWheelEvent* event)
{
  int delta = event->delta() > 0 ? -1 : 1;

  m_address += delta * 3 * sizeof(u32);
  Update();
}

void CodeViewWidget::mousePressEvent(QMouseEvent* event)
{
  auto* item = itemAt(event->pos());
  if (item == nullptr)
    return;

  const u32 addr = item->data(Qt::UserRole).toUInt();

  m_context_address = addr;

  switch (event->button())
  {
  case Qt::LeftButton:
    if (column(item) == 0)
      ToggleBreakpoint();
    else
      SetAddress(addr);

    Update();
    break;
  default:
    break;
  }
}

void CodeViewWidget::ToggleBreakpoint()
{
  if (PowerPC::debug_interface.IsBreakpoint(GetContextAddress()))
    PowerPC::breakpoints.Remove(GetContextAddress());
  else
    PowerPC::breakpoints.Add(GetContextAddress());

  emit BreakpointsChanged();
  Update();
}

void CodeViewWidget::AddBreakpoint()
{
  PowerPC::breakpoints.Add(GetContextAddress());

  emit BreakpointsChanged();
  Update();
}

u32 CodeViewWidget::GetContextAddress() const
{
  return m_context_address;
}
