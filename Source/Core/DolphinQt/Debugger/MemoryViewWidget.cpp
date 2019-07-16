// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/MemoryViewWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

#include <cctype>
#include <cmath>

#include "Core/Core.h"
#include "Core/HW/AddressSpace.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// "Most mouse types work in steps of 15 degrees, in which case the delta value is a multiple of
// 120; i.e., 120 units * 1/8 = 15 degrees." (http://doc.qt.io/qt-5/qwheelevent.html#angleDelta)
constexpr double SCROLL_FRACTION_DEGREES = 15.;

MemoryViewWidget::MemoryViewWidget(QWidget* parent) : QTableWidget(parent)
{
  horizontalHeader()->hide();
  verticalHeader()->hide();
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setShowGrid(false);

  setFont(Settings::Instance().GetDebugFont());

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &QWidget::setFont);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] { Update(); });
  connect(this, &MemoryViewWidget::customContextMenuRequested, this,
          &MemoryViewWidget::OnContextMenu);
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &MemoryViewWidget::Update);

  setContextMenuPolicy(Qt::CustomContextMenu);

  Update();
}

static int GetColumnCount(MemoryViewWidget::Type type)
{
  switch (type)
  {
  case MemoryViewWidget::Type::ASCII:
  case MemoryViewWidget::Type::U8:
    return 16;
  case MemoryViewWidget::Type::U16:
    return 8;
  case MemoryViewWidget::Type::U32:
  case MemoryViewWidget::Type::Float32:
    return 4;
  default:
    return 0;
  }
}

void MemoryViewWidget::Update()
{
  clearSelection();

  setColumnCount(3 + GetColumnCount(m_type));

  if (rowCount() == 0)
    setRowCount(1);

  setRowHeight(0, 24);

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);

  // Calculate (roughly) how many rows will fit in our table
  int rows = std::round((height() / static_cast<float>(rowHeight(0))) - 0.25);

  setRowCount(rows);

  for (int i = 0; i < rows; i++)
  {
    setRowHeight(i, 24);

    u32 addr = m_address - ((rowCount() / 2) * 16) + i * 16;

    auto* bp_item = new QTableWidgetItem;
    bp_item->setFlags(Qt::ItemIsEnabled);
    bp_item->setData(Qt::UserRole, addr);

    setItem(i, 0, bp_item);

    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    addr_item->setData(Qt::UserRole, addr);
    addr_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    setItem(i, 1, addr_item);

    if (addr == m_address)
      addr_item->setSelected(true);

    if (Core::GetState() != Core::State::Paused || !accessors->IsValidAddress(addr))
    {
      for (int c = 2; c < columnCount(); c++)
      {
        auto* item = new QTableWidgetItem(QStringLiteral("-"));
        item->setFlags(Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, addr);

        setItem(i, c, item);
      }

      continue;
    }

    if (m_address_space == AddressSpace::Type::Effective)
    {
      auto* description_item = new QTableWidgetItem(
          QString::fromStdString(PowerPC::debug_interface.GetDescription(addr)));

      description_item->setForeground(Qt::blue);
      description_item->setFlags(Qt::ItemIsEnabled);

      setItem(i, columnCount() - 1, description_item);
    }
    bool row_breakpoint = true;

    auto update_values = [&](auto value_to_string) {
      for (int c = 0; c < GetColumnCount(m_type); c++)
      {
        auto* hex_item = new QTableWidgetItem;
        hex_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        const u32 address = addr + c * (16 / GetColumnCount(m_type));

        if (m_address_space == AddressSpace::Type::Effective &&
            PowerPC::memchecks.OverlapsMemcheck(address, 16 / GetColumnCount(m_type)))
        {
          hex_item->setBackground(Qt::red);
        }
        else
        {
          row_breakpoint = false;
        }
        setItem(i, 2 + c, hex_item);

        if (accessors->IsValidAddress(address))
        {
          hex_item->setText(value_to_string(address));
          hex_item->setData(Qt::UserRole, address);
        }
        else
        {
          hex_item->setFlags(0);
          hex_item->setText(QStringLiteral("-"));
        }
      }
    };
    switch (m_type)
    {
    case Type::U8:
      update_values([&accessors](u32 address) {
        const u8 value = accessors->ReadU8(address);
        return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0'));
      });
      break;
    case Type::ASCII:
      update_values([&accessors](u32 address) {
        const char value = accessors->ReadU8(address);
        return std::isprint(value) ? QString{QChar::fromLatin1(value)} : QStringLiteral(".");
      });
      break;
    case Type::U16:
      update_values([&accessors](u32 address) {
        const u16 value = accessors->ReadU16(address);
        return QStringLiteral("%1").arg(value, 4, 16, QLatin1Char('0'));
      });
      break;
    case Type::U32:
      update_values([&accessors](u32 address) {
        const u32 value = accessors->ReadU32(address);
        return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
      });
      break;
    case Type::Float32:
      update_values(
          [&accessors](u32 address) { return QString::number(accessors->ReadF32(address)); });
      break;
    }

    if (row_breakpoint)
    {
      bp_item->setData(Qt::DecorationRole,
                       Resources::GetScaledThemeIcon("debugger_breakpoint").pixmap(QSize(24, 24)));
    }
  }

  setColumnWidth(0, 24 + 5);
  for (int i = 1; i < columnCount(); i++)
  {
    resizeColumnToContents(i);
    // Add some extra spacing because the default width is too small in most cases
    setColumnWidth(i, columnWidth(i) * 1.1);
  }

  viewport()->update();
  update();
}

void MemoryViewWidget::SetAddressSpace(AddressSpace::Type address_space)
{
  if (m_address_space == address_space)
  {
    return;
  }

  m_address_space = address_space;
  Update();
}

AddressSpace::Type MemoryViewWidget::GetAddressSpace() const
{
  return m_address_space;
}

void MemoryViewWidget::SetType(Type type)
{
  if (m_type == type)
    return;

  m_type = type;
  Update();
}

void MemoryViewWidget::SetBPType(BPType type)
{
  m_bp_type = type;
}

void MemoryViewWidget::SetAddress(u32 address)
{
  if (m_address == address)
    return;

  m_address = address;
  Update();
}

void MemoryViewWidget::SetBPLoggingEnabled(bool enabled)
{
  m_do_log = enabled;
}

void MemoryViewWidget::resizeEvent(QResizeEvent*)
{
  Update();
}

void MemoryViewWidget::keyPressEvent(QKeyEvent* event)
{
  switch (event->key())
  {
  case Qt::Key_Up:
    m_address -= 16;
    Update();
    return;
  case Qt::Key_Down:
    m_address += 16;
    Update();
    return;
  case Qt::Key_PageUp:
    m_address -= rowCount() * 16;
    Update();
    return;
  case Qt::Key_PageDown:
    m_address += rowCount() * 16;
    Update();
    return;
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

u32 MemoryViewWidget::GetContextAddress() const
{
  return m_context_address;
}

void MemoryViewWidget::ToggleRowBreakpoint(bool row)
{
  TMemCheck check;

  const u32 addr = row ? GetContextAddress() & 0xFFFFFFF0 : GetContextAddress();
  const auto length = row ? 16 : (16 / GetColumnCount(m_type));

  if (m_address_space == AddressSpace::Type::Effective)
  {
    if (!PowerPC::memchecks.OverlapsMemcheck(addr, length))
    {
      check.start_address = addr;
      check.end_address = check.start_address + length - 1;
      check.is_ranged = length > 0;
      check.is_break_on_read = (m_bp_type == BPType::ReadOnly || m_bp_type == BPType::ReadWrite);
      check.is_break_on_write = (m_bp_type == BPType::WriteOnly || m_bp_type == BPType::ReadWrite);
      check.log_on_hit = m_do_log;
      check.break_on_hit = true;

      PowerPC::memchecks.Add(check);
    }
    else
    {
      PowerPC::memchecks.Remove(addr);
    }
  }

  emit BreakpointsChanged();
  Update();
}

void MemoryViewWidget::ToggleBreakpoint()
{
  ToggleRowBreakpoint(false);
}

void MemoryViewWidget::wheelEvent(QWheelEvent* event)
{
  auto delta =
      -static_cast<int>(std::round((event->angleDelta().y() / (SCROLL_FRACTION_DEGREES * 8))));

  if (delta == 0)
    return;

  m_address += delta * 16;
  Update();
}

void MemoryViewWidget::mousePressEvent(QMouseEvent* event)
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
      ToggleRowBreakpoint(true);
    else
      SetAddress(addr & 0xFFFFFFF0);

    Update();
    break;
  default:
    break;
  }
}

void MemoryViewWidget::OnCopyAddress()
{
  u32 addr = GetContextAddress();
  QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
}

void MemoryViewWidget::OnCopyHex()
{
  u32 addr = GetContextAddress();

  const auto length = 16 / GetColumnCount(m_type);

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);
  u64 value = accessors->ReadU64(addr);

  QApplication::clipboard()->setText(
      QStringLiteral("%1").arg(value, length * 2, 16, QLatin1Char('0')).left(length * 2));
}

void MemoryViewWidget::OnContextMenu()
{
  auto* menu = new QMenu(this);

  menu->addAction(tr("Copy Address"), this, &MemoryViewWidget::OnCopyAddress);

  auto* copy_hex = menu->addAction(tr("Copy Hex"), this, &MemoryViewWidget::OnCopyHex);

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);
  copy_hex->setEnabled(Core::GetState() != Core::State::Uninitialized &&
                       accessors->IsValidAddress(GetContextAddress()));

  menu->addSeparator();

  menu->addAction(tr("Show in code"), this, [this] { emit ShowCode(GetContextAddress()); });

  menu->addSeparator();

  menu->addAction(tr("Toggle Breakpoint"), this, &MemoryViewWidget::ToggleBreakpoint);

  menu->exec(QCursor::pos());
}
