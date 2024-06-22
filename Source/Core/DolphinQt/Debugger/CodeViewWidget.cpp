// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/CodeViewWidget.h"

#include <algorithm>
#include <cmath>

#include <fmt/format.h>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>
#include <QWheelEvent>

#include "Common/Assert.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/Debugger/CodeTrace.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Debugger/AssembleInstructionDialog.h"
#include "DolphinQt/Debugger/PatchInstructionDialog.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/FromStdString.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

struct CodeViewBranch
{
  u32 src_addr = 0;
  u32 dst_addr = 0;
  u32 indentation = 0;
  bool is_link = false;
};

constexpr u32 WIDTH_PER_BRANCH_ARROW = 16;

class BranchDisplayDelegate : public QStyledItemDelegate
{
public:
  BranchDisplayDelegate(CodeViewWidget* parent) : QStyledItemDelegate(parent), m_parent(parent) {}

private:
  CodeViewWidget* m_parent;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    QStyledItemDelegate::paint(painter, option, index);

    painter->save();
    painter->setClipRect(option.rect);
    painter->setPen(m_parent->palette().text().color());

    constexpr u32 x_offset_in_branch_for_vertical_line = 10;
    const u32 addr = m_parent->AddressForRow(index.row());
    for (const CodeViewBranch& branch : m_parent->m_branches)
    {
      const int y_center = option.rect.top() + option.rect.height() / 2;
      const int x_left = option.rect.left() + WIDTH_PER_BRANCH_ARROW * branch.indentation;
      const int x_right = x_left + x_offset_in_branch_for_vertical_line;

      if (branch.is_link)
      {
        // just draw an arrow pointing right from the branch instruction for link branches, they
        // rarely are close enough to actually see the target and are just visual noise otherwise
        if (addr == branch.src_addr)
        {
          painter->drawLine(x_left, y_center, x_right, y_center);
          painter->drawLine(x_right, y_center, x_right - 6, y_center - 3);
          painter->drawLine(x_right, y_center, x_right - 6, y_center + 3);
        }
      }
      else
      {
        const u32 addr_lower = std::min(branch.src_addr, branch.dst_addr);
        const u32 addr_higher = std::max(branch.src_addr, branch.dst_addr);
        const bool in_range = addr >= addr_lower && addr <= addr_higher;

        if (in_range)
        {
          const bool is_lowest = addr == addr_lower;
          const bool is_highest = addr == addr_higher;
          const int top = (is_lowest ? y_center : option.rect.top());
          const int bottom = (is_highest ? y_center : option.rect.bottom());

          // draw vertical part of the branch line
          painter->drawLine(x_right, top, x_right, bottom);

          if (is_lowest || is_highest)
          {
            // draw horizontal part of the branch line if this is either the source or destination
            painter->drawLine(x_left, y_center, x_right, y_center);
          }

          if (addr == branch.dst_addr)
          {
            // draw arrow if this is the destination address
            painter->drawLine(x_left, y_center, x_left + 6, y_center - 3);
            painter->drawLine(x_left, y_center, x_left + 6, y_center + 3);
          }
        }
      }
    }

    painter->restore();
  }
};

// "Most mouse types work in steps of 15 degrees, in which case the delta value is a multiple of
// 120; i.e., 120 units * 1/8 = 15 degrees." (http://doc.qt.io/qt-5/qwheelevent.html#angleDelta)
constexpr double SCROLL_FRACTION_DEGREES = 15.;

constexpr size_t VALID_BRANCH_LENGTH = 10;

constexpr int CODE_VIEW_COLUMN_BREAKPOINT = 0;
constexpr int CODE_VIEW_COLUMN_ADDRESS = 1;
constexpr int CODE_VIEW_COLUMN_INSTRUCTION = 2;
constexpr int CODE_VIEW_COLUMN_PARAMETERS = 3;
constexpr int CODE_VIEW_COLUMN_DESCRIPTION = 4;
constexpr int CODE_VIEW_COLUMN_BRANCH_ARROWS = 5;
constexpr int CODE_VIEW_COLUMNCOUNT = 6;

CodeViewWidget::CodeViewWidget()
    : m_system(Core::System::GetInstance()), m_ppc_symbol_db(m_system.GetPPCSymbolDB())
{
  setColumnCount(CODE_VIEW_COLUMNCOUNT);
  setShowGrid(false);
  setContextMenuPolicy(Qt::CustomContextMenu);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectRows);

  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  verticalHeader()->hide();
  horizontalHeader()->setSectionResizeMode(CODE_VIEW_COLUMN_BREAKPOINT, QHeaderView::Fixed);
  horizontalHeader()->setStretchLastSection(true);
  setHorizontalHeaderItem(CODE_VIEW_COLUMN_BREAKPOINT, new QTableWidgetItem());
  setHorizontalHeaderItem(CODE_VIEW_COLUMN_ADDRESS, new QTableWidgetItem(tr("Address")));
  setHorizontalHeaderItem(CODE_VIEW_COLUMN_INSTRUCTION, new QTableWidgetItem(tr("Instr.")));
  setHorizontalHeaderItem(CODE_VIEW_COLUMN_PARAMETERS, new QTableWidgetItem(tr("Parameters")));
  setHorizontalHeaderItem(CODE_VIEW_COLUMN_DESCRIPTION, new QTableWidgetItem(tr("Symbols")));
  setHorizontalHeaderItem(CODE_VIEW_COLUMN_BRANCH_ARROWS, new QTableWidgetItem(tr("Branches")));

  setFont(Settings::Instance().GetDebugFont());
  setItemDelegateForColumn(CODE_VIEW_COLUMN_BRANCH_ARROWS, new BranchDisplayDelegate(this));

  FontBasedSizing();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme colorScheme) { OnSelectionChanged(); });
#endif

  connect(this, &CodeViewWidget::customContextMenuRequested, this, &CodeViewWidget::OnContextMenu);
  connect(this, &CodeViewWidget::itemSelectionChanged, this, &CodeViewWidget::OnSelectionChanged);
  connect(&Settings::Instance(), &Settings::DebugFontChanged, this,
          &CodeViewWidget::OnDebugFontChanged);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] {
    m_address = m_system.GetPPCState().pc;
    Update();
  });
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, [this] {
    m_address = m_system.GetPPCState().pc;
    Update();
  });
  connect(Host::GetInstance(), &Host::PPCSymbolsChanged, this,
          qOverload<>(&CodeViewWidget::Update));

  connect(&Settings::Instance(), &Settings::ThemeChanged, this,
          qOverload<>(&CodeViewWidget::Update));
}

CodeViewWidget::~CodeViewWidget() = default;

static u32 GetBranchFromAddress(const Core::CPUThreadGuard& guard, u32 addr)
{
  std::string disasm = guard.GetSystem().GetPowerPC().GetDebugInterface().Disassemble(&guard, addr);
  size_t pos = disasm.find("->0x");

  if (pos == std::string::npos)
    return 0;

  std::string hex = disasm.substr(pos + 2);
  return std::stoul(hex, nullptr, 16);
}

void CodeViewWidget::FontBasedSizing()
{
  // just text width is too small with some fonts, so increase by a bit
  constexpr int extra_text_width = 8;

  const QFontMetrics fm(font());

  const int rowh = fm.height() + 1;
  verticalHeader()->setMaximumSectionSize(rowh);
  horizontalHeader()->setMinimumSectionSize(rowh + 5);
  setColumnWidth(CODE_VIEW_COLUMN_BREAKPOINT, rowh + 5);
  setColumnWidth(CODE_VIEW_COLUMN_ADDRESS,
                 fm.boundingRect(QStringLiteral("80000000")).width() + extra_text_width);

  // The longest instruction is technically 'ps_merge00' (0x10000420u), but those instructions are
  // very rare and would needlessly increase the column size, so let's go with 'rlwinm.' instead.
  // Similarly, the longest parameter set is 'rtoc, rtoc, r10, 10, 10 (00000800)' (0x5c425294u),
  // but one is unlikely to encounter that in practice, so let's use a slightly more reasonable
  // 'r31, r31, 16, 16, 31 (ffff0000)'. The user can resize the columns as necessary anyway.
  const std::string disas = Common::GekkoDisassembler::Disassemble(0x57ff843fu, 0);
  const auto split = disas.find('\t');
  const std::string ins = (split == std::string::npos ? disas : disas.substr(0, split));
  const std::string param = (split == std::string::npos ? "" : disas.substr(split + 1));
  setColumnWidth(CODE_VIEW_COLUMN_INSTRUCTION,
                 fm.boundingRect(QString::fromStdString(ins)).width() + extra_text_width);
  setColumnWidth(CODE_VIEW_COLUMN_PARAMETERS,
                 fm.boundingRect(QString::fromStdString(param)).width() + extra_text_width);
  setColumnWidth(CODE_VIEW_COLUMN_DESCRIPTION,
                 fm.boundingRect(QChar(u'0')).width() * 25 + extra_text_width);

  Update();
}

u32 CodeViewWidget::AddressForRow(int row) const
{
  // m_address is defined as the center row of the table, so we have rowCount/2 instructions above
  // it; an instruction is 4 bytes long on GC/Wii so we increment 4 bytes per row
  const u32 row_zero_address = m_address - ((rowCount() / 2) * 4);
  return row_zero_address + row * 4;
}

static bool IsBranchInstructionWithLink(std::string_view ins)
{
  return ins.ends_with('l') || ins.ends_with("la") || ins.ends_with("l+") || ins.ends_with("la+") ||
         ins.ends_with("l-") || ins.ends_with("la-");
}

static bool IsInstructionLoadStore(std::string_view ins)
{
  // Could add check for context address being near PC, because we need gprs to be correct for the
  // load/store.
  return (ins.starts_with('l') && !ins.starts_with("li")) || ins.starts_with("st") ||
         ins.starts_with("psq_l") || ins.starts_with("psq_s");
}

void CodeViewWidget::Update()
{
  if (!isVisible())
    return;

  if (m_updating)
    return;

  if (Core::GetState(m_system) == Core::State::Paused)
  {
    Core::CPUThreadGuard guard(m_system);
    Update(&guard);
  }
  else
  {
    // If the core is running, blank out the view of memory instead of reading anything.
    Update(nullptr);
  }
}

void CodeViewWidget::Update(const Core::CPUThreadGuard* guard)
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

  auto& power_pc = m_system.GetPowerPC();
  auto& debug_interface = power_pc.GetDebugInterface();

  const std::optional<u32> pc =
      guard ? std::make_optional(power_pc.GetPPCState().pc) : std::nullopt;

  const bool dark_theme = Settings::Instance().IsThemeDark();

  m_branches.clear();

  for (int i = 0; i < rowCount(); i++)
  {
    const u32 addr = AddressForRow(i);
    const u32 color = debug_interface.GetColor(guard, addr);
    auto* bp_item = new QTableWidgetItem;
    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    std::string disas = debug_interface.Disassemble(guard, addr);
    auto split = disas.find('\t');

    std::string ins = (split == std::string::npos ? disas : disas.substr(0, split));
    std::string param = (split == std::string::npos ? "" : disas.substr(split + 1));
    const std::string_view desc = debug_interface.GetDescription(addr);

    // Adds whitespace and a minimum size to ins and param. Helps to prevent frequent resizing while
    // scrolling.
    const QString ins_formatted =
        QStringLiteral("%1").arg(QString::fromStdString(ins), -7, QLatin1Char(' '));
    const QString param_formatted =
        QStringLiteral("%1").arg(QString::fromStdString(param), -19, QLatin1Char(' '));
    const QString desc_formatted = QStringLiteral("%1   ").arg(QtUtils::FromStdString(desc));

    auto* ins_item = new QTableWidgetItem(ins_formatted);
    auto* param_item = new QTableWidgetItem(param_formatted);
    auto* description_item = new QTableWidgetItem(desc_formatted);
    auto* branch_item = new QTableWidgetItem();

    for (auto* item : {bp_item, addr_item, ins_item, param_item, description_item, branch_item})
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
        item->setBackground(dark_theme ? QColor(color).darker(400) : QColor(color));
      }
    }

    // look for hex strings to decode branches
    std::string hex_str;
    size_t pos = param.find("0x");
    if (pos != std::string::npos)
    {
      hex_str = param.substr(pos);
    }

    if (guard && hex_str.length() == VALID_BRANCH_LENGTH && desc != "---")
    {
      u32 branch_addr = GetBranchFromAddress(*guard, addr);
      CodeViewBranch& branch = m_branches.emplace_back();
      branch.src_addr = addr;
      branch.dst_addr = branch_addr;
      branch.is_link = IsBranchInstructionWithLink(ins);

      description_item->setText(
          tr("--> %1").arg(QtUtils::FromStdString(debug_interface.GetDescription(branch_addr))));
      param_item->setForeground(dark_theme ? QColor(255, 135, 255) : Qt::magenta);
    }

    if (ins == "blr")
      ins_item->setForeground(dark_theme ? QColor(0xa0FFa0) : Qt::darkGreen);

    if (debug_interface.IsBreakpoint(addr))
    {
      auto icon = Resources::GetThemeIcon("debugger_breakpoint").pixmap(QSize(rowh - 2, rowh - 2));
      if (!power_pc.GetBreakPoints().IsBreakPointEnable(addr))
      {
        QPixmap disabled_icon(icon.size());
        disabled_icon.fill(Qt::transparent);
        QPainter p(&disabled_icon);
        p.setOpacity(0.20);
        p.drawPixmap(0, 0, icon);
        p.end();
        icon = disabled_icon;
      }
      bp_item->setData(Qt::DecorationRole, icon);
    }

    setItem(i, CODE_VIEW_COLUMN_BREAKPOINT, bp_item);
    setItem(i, CODE_VIEW_COLUMN_ADDRESS, addr_item);
    setItem(i, CODE_VIEW_COLUMN_INSTRUCTION, ins_item);
    setItem(i, CODE_VIEW_COLUMN_PARAMETERS, param_item);
    setItem(i, CODE_VIEW_COLUMN_DESCRIPTION, description_item);
    setItem(i, CODE_VIEW_COLUMN_BRANCH_ARROWS, branch_item);

    if (addr == GetAddress())
    {
      selectRow(addr_item->row());
    }
  }

  CalculateBranchIndentation();

  m_ppc_symbol_db.FillInCallers();

  repaint();
  m_updating = false;
}

void CodeViewWidget::CalculateBranchIndentation()
{
  const u32 rows = rowCount();
  const size_t columns = m_branches.size();
  if (rows < 1 || columns < 1)
    return;

  // process in order of how much vertical space the drawn arrow would take up
  // so shorter arrows go further to the left
  const auto priority = [](const CodeViewBranch& b) {
    return b.is_link ? 0 : (std::max(b.src_addr, b.dst_addr) - std::min(b.src_addr, b.dst_addr));
  };
  std::stable_sort(m_branches.begin(), m_branches.end(),
                   [&priority](const CodeViewBranch& lhs, const CodeViewBranch& rhs) {
                     return priority(lhs) < priority(rhs);
                   });

  // build a 2D lookup table representing the columns and rows the arrow could be drawn in
  // and try to place all branch arrows in it as far left as possible
  std::vector<bool> arrow_space_used(columns * rows, false);
  const auto index = [&](u32 column, u32 row) {
    ASSERT(row <= rows);
    ASSERT(column <= columns);
    return column * rows + row;
  };

  const auto add_branch_arrow = [&](CodeViewBranch& branch, u32 first_addr, u32 first_row,
                                    u32 last_addr) {
    const u32 arrow_src_addr = branch.src_addr;
    const u32 arrow_dst_addr = branch.is_link ? branch.src_addr : branch.dst_addr;
    const auto [arrow_addr_lower, arrow_addr_higher] = std::minmax(arrow_src_addr, arrow_dst_addr);

    const bool is_visible =
        std::max(arrow_addr_lower, first_addr) <= std::min(arrow_addr_higher, last_addr);
    if (!is_visible)
      return;

    const u32 arrow_first_visible_addr = std::clamp(arrow_addr_lower, first_addr, last_addr);
    const u32 arrow_last_visible_addr = std::clamp(arrow_addr_higher, first_addr, last_addr);
    const u32 arrow_first_visible_row = (arrow_first_visible_addr - first_addr) / 4 + first_row;
    const u32 arrow_last_visible_row = (arrow_last_visible_addr - first_addr) / 4 + first_row;

    const auto free_column = [&]() -> std::optional<u32> {
      for (u32 column = 0; column < columns; ++column)
      {
        const bool column_is_free = [&] {
          for (u32 row = arrow_first_visible_row; row <= arrow_last_visible_row; ++row)
          {
            if (arrow_space_used[index(column, row)])
              return false;
          }
          return true;
        }();
        if (column_is_free)
          return column;
      }
      return std::nullopt;
    }();

    if (!free_column)
      return;

    branch.indentation = *free_column;
    for (u32 row = arrow_first_visible_row; row <= arrow_last_visible_row; ++row)
      arrow_space_used[index(*free_column, row)] = true;
  };

  const u32 first_visible_addr = AddressForRow(0);
  const u32 last_visible_addr = AddressForRow(rows - 1);

  if (first_visible_addr <= last_visible_addr)
  {
    for (CodeViewBranch& branch : m_branches)
      add_branch_arrow(branch, first_visible_addr, 0, last_visible_addr);
  }
  else
  {
    // Scrolling defaults to being centered around address 00000000, which means addresses before
    // the start are visible (e.g. ffffffa8 - 00000050).  We need to do this in two parts, one for
    // first_visible_addr to fffffffc, and the second for 00000000 to last_visible_addr.
    // That means we need to find the row corresponding to 00000000.
    int addr_zero_row = -1;
    for (u32 row = 0; row < rows; row++)
    {
      if (AddressForRow(row) == 0)
      {
        addr_zero_row = row;
        break;
      }
    }
    ASSERT(addr_zero_row != -1);

    for (CodeViewBranch& branch : m_branches)
    {
      add_branch_arrow(branch, first_visible_addr, 0, 0xfffffffc);
      add_branch_arrow(branch, 0x00000000, addr_zero_row, last_visible_addr);
    }
  }
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
  switch (update)
  {
  case SetAddressUpdate::WithoutUpdate:
    return;
  case SetAddressUpdate::WithUpdate:
    // Update the CodeViewWidget
    Update();
    break;
  case SetAddressUpdate::WithDetailedUpdate:
    // Update the CodeWidget's views (code view, function calls/callers, ...)
    emit UpdateCodeWidget();
    break;
  }
}

void CodeViewWidget::ReplaceAddress(u32 address, ReplaceWith replace)
{
  Core::CPUThreadGuard guard(m_system);

  m_system.GetPowerPC().GetDebugInterface().SetPatch(
      guard, address, replace == ReplaceWith::BLR ? 0x4e800020 : 0x60000000);

  Update(&guard);
}

void CodeViewWidget::OnContextMenu()
{
  QMenu* menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose, true);

  const bool running = Core::IsRunning(m_system);
  const bool paused = Core::GetState(m_system) == Core::State::Paused;

  const u32 addr = GetContextAddress();

  const bool has_symbol = m_ppc_symbol_db.GetSymbolFromAddr(addr);

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
  auto* show_target_memory =
      menu->addAction(tr("Show target in memor&y"), this, &CodeViewWidget::OnShowTargetInMemory);
  auto* copy_target_memory =
      menu->addAction(tr("Copy tar&get address"), this, &CodeViewWidget::OnCopyTargetAddress);
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
  auto* assemble_action =
      menu->addAction(tr("Assemble instruction"), this, &CodeViewWidget::OnAssembleInstruction);
  auto* restore_action =
      menu->addAction(tr("Restore instruction"), this, &CodeViewWidget::OnRestoreInstruction);

  QString target;
  bool valid_load_store = false;
  bool follow_branch_enabled = false;
  if (paused)
  {
    Core::CPUThreadGuard guard(m_system);
    const u32 pc = m_system.GetPPCState().pc;
    const std::string disasm = m_system.GetPowerPC().GetDebugInterface().Disassemble(&guard, pc);

    if (addr == pc)
    {
      const auto target_it = std::find(disasm.begin(), disasm.end(), '\t');
      const auto target_end = std::find(target_it, disasm.end(), ',');

      if (target_it != disasm.end() && target_end != disasm.end())
        target = QString::fromStdString(std::string{target_it + 1, target_end});
    }

    valid_load_store = IsInstructionLoadStore(disasm);

    follow_branch_enabled = GetBranchFromAddress(guard, addr);
  }

  auto* run_until_menu = menu->addMenu(tr("Run until (ignoring breakpoints)"));
  // i18n: One of the options shown below "Run until (ignoring breakpoints)"
  run_until_menu->addAction(tr("%1's value is hit").arg(target), this,
                            [this] { AutoStep(CodeTrace::AutoStop::Always); });
  // i18n: One of the options shown below "Run until (ignoring breakpoints)"
  run_until_menu->addAction(tr("%1's value is used").arg(target), this,
                            [this] { AutoStep(CodeTrace::AutoStop::Used); });
  // i18n: One of the options shown below "Run until (ignoring breakpoints)"
  run_until_menu->addAction(tr("%1's value is changed").arg(target),
                            [this] { AutoStep(CodeTrace::AutoStop::Changed); });

  run_until_menu->setEnabled(!target.isEmpty());
  follow_branch_action->setEnabled(follow_branch_enabled);

  for (auto* action :
       {copy_address_action, copy_line_action, copy_hex_action, function_action, ppc_action,
        insert_blr_action, insert_nop_action, replace_action, assemble_action})
  {
    action->setEnabled(running);
  }

  for (auto* action : {symbol_rename_action, symbol_size_action, symbol_end_action})
    action->setEnabled(has_symbol);

  for (auto* action : {copy_target_memory, show_target_memory})
  {
    action->setEnabled(valid_load_store);
  }

  restore_action->setEnabled(running &&
                             m_system.GetPowerPC().GetDebugInterface().HasEnabledPatch(addr));

  menu->exec(QCursor::pos());
  Update();
}

void CodeViewWidget::AutoStep(CodeTrace::AutoStop option)
{
  // Autosteps and follows value in the target (left-most) register. The Used and Changed options
  // silently follows target through reshuffles in memory and registers and stops on use or update.

  Core::CPUThreadGuard guard(m_system);

  CodeTrace code_trace;
  bool repeat = false;

  QMessageBox msgbox(QMessageBox::NoIcon, tr("Run until"), {}, QMessageBox::Cancel);
  QPushButton* run_button = msgbox.addButton(tr("Keep Running"), QMessageBox::AcceptRole);
  // Not sure if we want default to be cancel. Spacebar can let you quickly continue autostepping if
  // Yes.

  do
  {
    // Run autostep then update codeview
    const AutoStepResults results = code_trace.AutoStepping(guard, repeat, option);
    emit Host::GetInstance()->UpdateDisasmDialog();
    repeat = true;

    // Invalid instruction, 0 means no step executed.
    if (results.count == 0)
      return;

    // Status report
    if (results.reg_tracked.empty() && results.mem_tracked.empty())
    {
      QMessageBox::warning(
          this, tr("Overwritten"),
          tr("Target value was overwritten by current instruction.\nInstructions executed:   %1")
              .arg(QString::number(results.count)),
          QMessageBox::Cancel);
      return;
    }
    else if (results.timed_out)
    {
      // Can keep running and try again after a time out.
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
    SetQWidgetWindowDecorations(&msgbox);
    msgbox.exec();

  } while (msgbox.clickedButton() == (QAbstractButton*)run_button);
}

void CodeViewWidget::OnDebugFontChanged(const QFont& font)
{
  setFont(font);
  FontBasedSizing();
}

void CodeViewWidget::OnCopyAddress()
{
  const u32 addr = GetContextAddress();

  QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
}

void CodeViewWidget::OnCopyTargetAddress()
{
  if (Core::GetState(m_system) != Core::State::Paused)
    return;

  const u32 addr = GetContextAddress();

  const std::string code_line = [this, addr] {
    Core::CPUThreadGuard guard(m_system);
    return m_system.GetPowerPC().GetDebugInterface().Disassemble(&guard, addr);
  }();

  if (!IsInstructionLoadStore(code_line))
    return;

  const std::optional<u32> target_addr =
      m_system.GetPowerPC().GetDebugInterface().GetMemoryAddressFromInstruction(code_line);

  if (target_addr)
  {
    QApplication::clipboard()->setText(
        QStringLiteral("%1").arg(*target_addr, 8, 16, QLatin1Char('0')));
  }
}

void CodeViewWidget::OnShowInMemory()
{
  emit ShowMemory(GetContextAddress());
}

void CodeViewWidget::OnShowTargetInMemory()
{
  if (Core::GetState(m_system) != Core::State::Paused)
    return;

  const u32 addr = GetContextAddress();

  const std::string code_line = [this, addr] {
    Core::CPUThreadGuard guard(m_system);
    return m_system.GetPowerPC().GetDebugInterface().Disassemble(&guard, addr);
  }();

  if (!IsInstructionLoadStore(code_line))
    return;

  const std::optional<u32> target_addr =
      m_system.GetPowerPC().GetDebugInterface().GetMemoryAddressFromInstruction(code_line);

  if (target_addr)
    emit ShowMemory(*target_addr);
}

void CodeViewWidget::OnCopyCode()
{
  const u32 addr = GetContextAddress();

  const std::string text = [this, addr] {
    Core::CPUThreadGuard guard(m_system);
    return m_system.GetPowerPC().GetDebugInterface().Disassemble(&guard, addr);
  }();

  QApplication::clipboard()->setText(QString::fromStdString(text));
}

void CodeViewWidget::OnCopyFunction()
{
  const u32 address = GetContextAddress();

  const Common::Symbol* const symbol = m_ppc_symbol_db.GetSymbolFromAddr(address);
  if (!symbol)
    return;

  std::string text = symbol->name + "\r\n";

  {
    Core::CPUThreadGuard guard(m_system);

    // we got a function
    const u32 start = symbol->address;
    const u32 end = start + symbol->size;
    for (u32 addr = start; addr != end; addr += 4)
    {
      const std::string disasm =
          m_system.GetPowerPC().GetDebugInterface().Disassemble(&guard, addr);
      fmt::format_to(std::back_inserter(text), "{:08x}: {}\r\n", addr, disasm);
    }
  }

  QApplication::clipboard()->setText(QString::fromStdString(text));
}

void CodeViewWidget::OnCopyHex()
{
  const u32 addr = GetContextAddress();

  const u32 instruction = [this, addr] {
    Core::CPUThreadGuard guard(m_system);
    return m_system.GetPowerPC().GetDebugInterface().ReadInstruction(guard, addr);
  }();

  QApplication::clipboard()->setText(
      QStringLiteral("%1").arg(instruction, 8, 16, QLatin1Char('0')));
}

void CodeViewWidget::OnRunToHere()
{
  const u32 addr = GetContextAddress();

  m_system.GetPowerPC().GetDebugInterface().SetBreakpoint(addr);
  m_system.GetPowerPC().GetDebugInterface().RunToBreakpoint();
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

  Core::CPUThreadGuard guard(m_system);

  m_ppc_symbol_db.AddFunction(guard, addr);
  emit Host::GetInstance()->PPCSymbolsChanged();
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

  const u32 branch_addr = [this, addr] {
    Core::CPUThreadGuard guard(m_system);
    return GetBranchFromAddress(guard, addr);
  }();

  if (!branch_addr)
    return;

  SetAddress(branch_addr, SetAddressUpdate::WithDetailedUpdate);
}

void CodeViewWidget::OnRenameSymbol()
{
  const u32 addr = GetContextAddress();

  Common::Symbol* const symbol = m_ppc_symbol_db.GetSymbolFromAddr(addr);

  if (!symbol)
    return;

  bool good;
  const QString name =
      QInputDialog::getText(this, tr("Rename symbol"), tr("Symbol name:"), QLineEdit::Normal,
                            QString::fromStdString(symbol->name), &good, Qt::WindowCloseButtonHint);

  if (good && !name.isEmpty())
  {
    symbol->Rename(name.toStdString());
    emit Host::GetInstance()->PPCSymbolsChanged();
  }
}

void CodeViewWidget::OnSelectionChanged()
{
  if (m_address == m_system.GetPPCState().pc)
  {
    setStyleSheet(
        QStringLiteral("QTableView::item:selected {background-color: #00FF00; color: #000000;}"));
  }
  else if (!styleSheet().isEmpty())
  {
    setStyleSheet(QString{});
  }
}

void CodeViewWidget::OnSetSymbolSize()
{
  const u32 addr = GetContextAddress();

  Common::Symbol* const symbol = m_ppc_symbol_db.GetSymbolFromAddr(addr);

  if (!symbol)
    return;

  bool good;
  const int size =
      QInputDialog::getInt(this, tr("Rename symbol"),
                           tr("Set symbol size (%1):").arg(QString::fromStdString(symbol->name)),
                           symbol->size, 1, 0xFFFF, 1, &good, Qt::WindowCloseButtonHint);

  if (!good)
    return;

  Core::CPUThreadGuard guard(m_system);

  PPCAnalyst::ReanalyzeFunction(guard, symbol->address, *symbol, size);
  emit Host::GetInstance()->PPCSymbolsChanged();
}

void CodeViewWidget::OnSetSymbolEndAddress()
{
  const u32 addr = GetContextAddress();

  Common::Symbol* const symbol = m_ppc_symbol_db.GetSymbolFromAddr(addr);

  if (!symbol)
    return;

  bool good;
  const QString name = QInputDialog::getText(
      this, tr("Set symbol end address"),
      tr("Symbol (%1) end address:").arg(QString::fromStdString(symbol->name)), QLineEdit::Normal,
      QStringLiteral("%1").arg(addr + symbol->size, 8, 16, QLatin1Char('0')), &good,
      Qt::WindowCloseButtonHint);

  const u32 address = name.toUInt(&good, 16);

  if (!good)
    return;

  Core::CPUThreadGuard guard(m_system);

  PPCAnalyst::ReanalyzeFunction(guard, symbol->address, *symbol, address - symbol->address);
  emit Host::GetInstance()->PPCSymbolsChanged();
}

void CodeViewWidget::OnReplaceInstruction()
{
  DoPatchInstruction(false);
}

void CodeViewWidget::OnAssembleInstruction()
{
  DoPatchInstruction(true);
}

void CodeViewWidget::DoPatchInstruction(bool assemble)
{
  Core::CPUThreadGuard guard(m_system);
  const u32 addr = GetContextAddress();

  if (!PowerPC::MMU::HostIsInstructionRAMAddress(guard, addr))
    return;

  const PowerPC::TryReadInstResult read_result =
      guard.GetSystem().GetMMU().TryReadInstruction(addr);
  if (!read_result.valid)
    return;

  auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();

  if (assemble)
  {
    AssembleInstructionDialog dialog(this, addr, debug_interface.ReadInstruction(guard, addr));
    SetQWidgetWindowDecorations(&dialog);
    if (dialog.exec() == QDialog::Accepted)
    {
      debug_interface.SetPatch(guard, addr, dialog.GetCode());
      Update(&guard);
    }
  }
  else
  {
    PatchInstructionDialog dialog(this, addr, debug_interface.ReadInstruction(guard, addr));
    SetQWidgetWindowDecorations(&dialog);
    if (dialog.exec() == QDialog::Accepted)
    {
      debug_interface.SetPatch(guard, addr, dialog.GetCode());
      Update(&guard);
    }
  }
}

void CodeViewWidget::OnRestoreInstruction()
{
  Core::CPUThreadGuard guard(m_system);

  const u32 addr = GetContextAddress();

  m_system.GetPowerPC().GetDebugInterface().UnsetPatch(guard, addr);
  Update(&guard);
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
    if (column(item) == CODE_VIEW_COLUMN_BREAKPOINT)
      ToggleBreakpoint();
    else
      SetAddress(addr, SetAddressUpdate::WithDetailedUpdate);

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
  auto& power_pc = m_system.GetPowerPC();
  if (power_pc.GetDebugInterface().IsBreakpoint(GetContextAddress()))
    power_pc.GetBreakPoints().Remove(GetContextAddress());
  else
    power_pc.GetBreakPoints().Add(GetContextAddress());

  emit BreakpointsChanged();
  Update();
}

void CodeViewWidget::AddBreakpoint()
{
  m_system.GetPowerPC().GetBreakPoints().Add(GetContextAddress());

  emit BreakpointsChanged();
  Update();
}

u32 CodeViewWidget::GetContextAddress() const
{
  return m_context_address;
}
