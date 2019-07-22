// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeViewWidget.h"

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
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Debugger/PatchInstructionDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// "Most mouse types work in steps of 15 degrees, in which case the delta value is a multiple of
// 120; i.e., 120 units * 1/8 = 15 degrees." (http://doc.qt.io/qt-5/qwheelevent.html#angleDelta)
constexpr double SCROLL_FRACTION_DEGREES = 15.;

constexpr size_t VALID_BRANCH_LENGTH = 10;

CodeViewWidget::CodeViewWidget()
{
  setColumnCount(5);
  setShowGrid(false);
  setContextMenuPolicy(Qt::CustomContextMenu);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectRows);

  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  verticalHeader()->hide();
  horizontalHeader()->hide();

  setFont(Settings::Instance().GetDebugFont());

  FontBasedSizing();

  connect(this, &CodeViewWidget::customContextMenuRequested, this, &CodeViewWidget::OnContextMenu);
  connect(this, &CodeViewWidget::itemSelectionChanged, this, &CodeViewWidget::OnSelectionChanged);
  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &QWidget::setFont);
  connect(&Settings::Instance(), &Settings::DebugFontChanged, this,
          &CodeViewWidget::FontBasedSizing);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] {
    m_address = PC;
    Update();
  });

  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &CodeViewWidget::Update);
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

void CodeViewWidget::FontBasedSizing()
{
  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  const int rowh = fm.height() + 1;
  verticalHeader()->setMaximumSectionSize(rowh);
  horizontalHeader()->setMinimumSectionSize(rowh + 5);
  setColumnWidth(0, rowh + 5);
  Update();
}

void CodeViewWidget::Update()
{
  if (!isVisible())
    return;

  if (m_updating)
    return;

  m_updating = true;

  clearSelection();
  if (rowCount() == 0)
    setRowCount(1);

  // Calculate (roughly) how many rows will fit in our table
  int rows = std::round((height() / static_cast<float>(rowHeight(0))) - 0.25);

  setRowCount(rows);

  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  const int rowh = fm.height() + 1;

  for (int i = 0; i < rows; i++)
    setRowHeight(i, rowh);

  u32 pc = PowerPC::ppcState.pc;

  if (Core::GetState() != Core::State::Paused && PowerPC::debug_interface.IsBreakpoint(pc))
    Core::SetState(Core::State::Paused);

  const bool dark_theme = qApp->palette().color(QPalette::Base).valueF() < 0.5;

  for (int i = 0; i < rowCount(); i++)
  {
    const u32 addr = m_address - ((rowCount() / 2) * 4) + i * 4;
    const u32 color = PowerPC::debug_interface.GetColor(addr);
    auto* bp_item = new QTableWidgetItem;
    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    std::string disas = PowerPC::debug_interface.Disassemble(addr);
    auto split = disas.find('\t');

    std::string ins = (split == std::string::npos ? disas : disas.substr(0, split));
    std::string param = (split == std::string::npos ? "" : disas.substr(split + 1));
    std::string desc = PowerPC::debug_interface.GetDescription(addr);

    // Adds whitespace and a minimum size to ins and param. Helps to prevent frequent resizing while
    // scrolling.
    const QString ins_formatted =
        QStringLiteral("%1").arg(QString::fromStdString(ins), -7, QLatin1Char(' '));
    const QString param_formatted =
        QStringLiteral("%1").arg(QString::fromStdString(param), -19, QLatin1Char(' '));
    const QString desc_formatted = QStringLiteral("%1   ").arg(QString::fromStdString(desc));

    auto* ins_item = new QTableWidgetItem(ins_formatted);
    auto* param_item = new QTableWidgetItem(param_formatted);
    auto* description_item = new QTableWidgetItem(desc_formatted);

    for (auto* item : {bp_item, addr_item, ins_item, param_item, description_item})
    {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setData(Qt::UserRole, addr);

      if (addr == pc && item != bp_item)
      {
        item->setBackground(QColor(Qt::green));
        item->setForeground(QColor(Qt::black));
      }
      else if (color != 0xFFFFFF)
      {
        item->setBackground(dark_theme ? QColor(color).darker(240) : QColor(color));
      }
    }

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
      ins_item->setForeground(dark_theme ? QColor(0xa0FFa0) : Qt::darkGreen);

    if (PowerPC::debug_interface.IsBreakpoint(addr))
    {
      bp_item->setData(
          Qt::DecorationRole,
          Resources::GetScaledThemeIcon("debugger_breakpoint").pixmap(QSize(rowh - 2, rowh - 2)));
    }

    setItem(i, 0, bp_item);
    setItem(i, 1, addr_item);
    setItem(i, 2, ins_item);
    setItem(i, 3, param_item);
    setItem(i, 4, description_item);

    if (addr == GetAddress())
    {
      selectRow(addr_item->row());
    }
  }

  resizeColumnsToContents();
  g_symbolDB.FillInCallers();

  repaint();
  m_updating = false;
}

u32 CodeViewWidget::GetAddress() const
{
  return m_address;
}

void CodeViewWidget::SetAddress(u32 address, SetAddressUpdate update)
{
  if (m_address == address)
    return;

  m_address = address;
  if (update == SetAddressUpdate::WithUpdate)
    Update();
}

void CodeViewWidget::ReplaceAddress(u32 address, ReplaceWith replace)
{
  PowerPC::debug_interface.UnsetPatch(address);
  PowerPC::debug_interface.SetPatch(address, replace == ReplaceWith::BLR ? 0x4e800020 : 0x60000000);
  Update();
}

void CodeViewWidget::OnContextMenu()
{
  QMenu* menu = new QMenu(this);

  bool running = Core::GetState() != Core::State::Uninitialized;

  const u32 addr = GetContextAddress();

  bool has_symbol = g_symbolDB.GetSymbolFromAddr(addr);

  auto* follow_branch_action =
      menu->addAction(tr("Follow &branch"), this, &CodeViewWidget::OnFollowBranch);

  menu->addSeparator();

  menu->addAction(tr("&Copy address"), this, &CodeViewWidget::OnCopyAddress);
  auto* copy_address_action =
      menu->addAction(tr("Copy &function"), this, &CodeViewWidget::OnCopyFunction);
  auto* copy_line_action =
      menu->addAction(tr("Copy code &line"), this, &CodeViewWidget::OnCopyCode);
  auto* copy_hex_action = menu->addAction(tr("Copy &hex"), this, &CodeViewWidget::OnCopyHex);

  menu->addAction(tr("Show in &memory"), this, &CodeViewWidget::OnShowInMemory);

  menu->addSeparator();

  auto* symbol_rename_action =
      menu->addAction(tr("&Rename symbol"), this, &CodeViewWidget::OnRenameSymbol);
  auto* symbol_size_action =
      menu->addAction(tr("Set symbol &size"), this, &CodeViewWidget::OnSetSymbolSize);
  auto* symbol_end_action =
      menu->addAction(tr("Set symbol &end address"), this, &CodeViewWidget::OnSetSymbolEndAddress);
  menu->addSeparator();

  menu->addAction(tr("Run &To Here"), this, &CodeViewWidget::OnRunToHere);
  auto* function_action =
      menu->addAction(tr("&Add function"), this, &CodeViewWidget::OnAddFunction);
  auto* ppc_action = menu->addAction(tr("PPC vs Host"), this, &CodeViewWidget::OnPPCComparison);
  auto* insert_blr_action = menu->addAction(tr("&Insert blr"), this, &CodeViewWidget::OnInsertBLR);
  auto* insert_nop_action = menu->addAction(tr("Insert &nop"), this, &CodeViewWidget::OnInsertNOP);
  auto* replace_action =
      menu->addAction(tr("Re&place instruction"), this, &CodeViewWidget::OnReplaceInstruction);
  auto* restore_action =
      menu->addAction(tr("Restore instruction"), this, &CodeViewWidget::OnRestoreInstruction);

  follow_branch_action->setEnabled(running && GetBranchFromAddress(addr));

  for (auto* action : {copy_address_action, copy_line_action, copy_hex_action, function_action,
                       ppc_action, insert_blr_action, insert_nop_action, replace_action})
    action->setEnabled(running);

  for (auto* action : {symbol_rename_action, symbol_size_action, symbol_end_action})
    action->setEnabled(has_symbol);

  restore_action->setEnabled(running && PowerPC::debug_interface.HasEnabledPatch(addr));

  menu->exec(QCursor::pos());
  Update();
}

void CodeViewWidget::OnCopyAddress()
{
  const u32 addr = GetContextAddress();

  QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
}

void CodeViewWidget::OnShowInMemory()
{
  emit ShowMemory(GetContextAddress());
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

  const Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(address);
  if (!symbol)
    return;

  std::string text = symbol->name + "\r\n";
  // we got a function
  const u32 start = symbol->address;
  const u32 end = start + symbol->size;
  for (u32 addr = start; addr != end; addr += 4)
  {
    const std::string disasm = PowerPC::debug_interface.Disassemble(addr);
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

  ReplaceAddress(addr, ReplaceWith::BLR);
}

void CodeViewWidget::OnInsertNOP()
{
  const u32 addr = GetContextAddress();

  ReplaceAddress(addr, ReplaceWith::NOP);
}

void CodeViewWidget::OnFollowBranch()
{
  const u32 addr = GetContextAddress();

  u32 branch_addr = GetBranchFromAddress(addr);

  if (!branch_addr)
    return;

  SetAddress(branch_addr, SetAddressUpdate::WithUpdate);
}

void CodeViewWidget::OnRenameSymbol()
{
  const u32 addr = GetContextAddress();

  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);

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

void CodeViewWidget::OnSelectionChanged()
{
  if (m_address == PowerPC::ppcState.pc)
  {
    setStyleSheet(
        QStringLiteral("QTableView::item:selected {background-color: #00FF00; color: #000000;}"));
  }
  else if (!styleSheet().isEmpty())
  {
    setStyleSheet(QStringLiteral(""));
  }
}

void CodeViewWidget::OnSetSymbolSize()
{
  const u32 addr = GetContextAddress();

  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);

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

  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(addr);

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

  PatchInstructionDialog dialog(this, addr, PowerPC::debug_interface.ReadInstruction(addr));

  if (dialog.exec() == QDialog::Accepted)
  {
    PowerPC::debug_interface.UnsetPatch(addr);
    PowerPC::debug_interface.SetPatch(addr, dialog.GetCode());
    Update();
  }
}

void CodeViewWidget::OnRestoreInstruction()
{
  const u32 addr = GetContextAddress();

  PowerPC::debug_interface.UnsetPatch(addr);
  Update();
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
    m_address -= sizeof(u32);
    Update();
    return;
  case Qt::Key_Down:
    m_address += sizeof(u32);
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
  auto delta =
      -static_cast<int>(std::round((event->angleDelta().y() / (SCROLL_FRACTION_DEGREES * 8))));

  if (delta == 0)
    return;

  m_address += delta * sizeof(u32);
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
      SetAddress(addr, SetAddressUpdate::WithUpdate);

    Update();
    break;
  default:
    break;
  }
}

void CodeViewWidget::showEvent(QShowEvent* event)
{
  Update();
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
