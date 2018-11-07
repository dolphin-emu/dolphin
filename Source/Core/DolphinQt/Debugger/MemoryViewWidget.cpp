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
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/MMU.h"
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
  setAlternatingRowColors(true);

  setFont(Settings::Instance().GetDebugFont());
  QFontMetrics fm(Settings::Instance().GetDebugFont());

  // Row height as function of text size. Less height than default.
  const int fonth = fm.height() + 3;
  verticalHeader()->setMaximumSectionSize(fonth);

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
  case MemoryViewWidget::Type::U32xASCII:
  case MemoryViewWidget::Type::U32xFloat32:
    return 2;
  case MemoryViewWidget::Type::U8:
    return 16;
  case MemoryViewWidget::Type::U16:
    return 8;
  case MemoryViewWidget::Type::U32:
  case MemoryViewWidget::Type::ASCII:
  case MemoryViewWidget::Type::Float32:
    return 4;
  default:
    return 0;
  }
}

void MemoryViewWidget::Update()
{
  if (!isVisible())  // skip all this if the memory window isn't up
    return;

  clearSelection();

  setColumnCount(3 + GetColumnCount(m_type));

  if (rowCount() == 0)
    setRowCount(1);

  setRowHeight(0, 24);

  // Calculate (roughly) how many rows will fit in our table
  int rows = std::round((height() / static_cast<float>(rowHeight(0))) - 0.25);

  setRowCount(rows);

  for (int i = 0; i < rows; i++)
  {
    setRowHeight(i, 24);

    // Two column mode has rows increment by 0x4 instead of 0x10
    u32 rowmod = ((GetColumnCount(m_type) == 2) ? 4 : 16);
    u32 addr = m_address - (rowCount() / 2) * rowmod + i * rowmod;

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

    // Don't update values unless game is started
    if (Core::GetState() == Core::State::Uninitialized || Core::GetState() == Core::State::Starting)
      continue;

    auto* description_item =
        new QTableWidgetItem(QString::fromStdString(PowerPC::debug_interface.GetDescription(addr)));

    description_item->setForeground(Qt::blue);
    description_item->setFlags(Qt::ItemIsEnabled);

    setItem(i, columnCount() - 1, description_item);

    bool row_breakpoint = true;

    auto update_values = [&](auto value_to_string) {
      const int columns = GetColumnCount(m_type);
      for (int c = 0; c < columns; c++)
      {
        auto* hex_item = new QTableWidgetItem;
        hex_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        u32 address = addr + c * (16 / columns);

        // Alternate view requires two columns with the same address
        if (columns == 2)
          address = addr;
        if (PowerPC::memchecks.OverlapsMemcheck(address, 16 / ((columns == 2) ? 4 : columns)))
          hex_item->setBackground(Qt::red);
        else
          row_breakpoint = false;

        setItem(i, 2 + c, hex_item);

        if (PowerPC::HostIsRAMAddress(address))
        {
          hex_item->setText(value_to_string(address));

          // 2 Column mode: Report hex value in first column.
          if (columns == 2 && c == 0)
          {
            hex_item->setText(
                QStringLiteral("%1").arg(PowerPC::HostRead_U32(address), 8, 16, QLatin1Char('0')));
          }

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
      update_values([](u32 address) {
        const u8 value = PowerPC::HostRead_U8(address);
        return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0'));
      });
      break;
    case Type::ASCII:
    case Type::U32xASCII:
      update_values([](u32 address) {
        QString asciistring = QStringLiteral("");
        // Group ASCII in sets of four.
        for (u32 i = 0; i < 4; i++)
        {
          char value = PowerPC::HostRead_U8(address + i);
          asciistring.append(std::isprint(value) ? QChar::fromLatin1(value) : QStringLiteral("."));
        }
        return asciistring;
      });
      break;
    case Type::U16:
      update_values([](u32 address) {
        const u16 value = PowerPC::HostRead_U16(address);
        return QStringLiteral("%1").arg(value, 4, 16, QLatin1Char('0'));
      });
      break;
    case Type::U32:
      update_values([](u32 address) {
        const u32 value = PowerPC::HostRead_U32(address);
        return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
      });
      break;
    case Type::Float32:
    case Type::U32xFloat32:
      update_values([](u32 address) { return QString::number(PowerPC::HostRead_F32(address)); });
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

  // Breakpoints will apply to 4 bytes aligned to 0x4
  const u32 addr = row ? GetContextAddress() & 0xFFFFFFF0 : ((GetContextAddress() >> 2) << 2);
  const auto length = (GetColumnCount(m_type) == 2) ? 4 : (row ? 16 : 4);

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

    if (event->modifiers() & Qt::ShiftModifier)
    {
      QString setaddr = QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0'));
      emit SendSearchValue(setaddr);
    }
    else if (event->modifiers() & Qt::ControlModifier)
    {
      const auto length = 32 / ((GetColumnCount(m_type) == 2) ? 4 : GetColumnCount(m_type));
      u64 value = PowerPC::HostRead_U64(addr);
      QString setvalue = QStringLiteral("%1").arg(value, 16, 16, QLatin1Char('0')).left(length);
      emit SendDataValue(setvalue);
    }
    else if (column(item) == 0)
    {
      ToggleRowBreakpoint(true);
    }
    else
    {
      // Scroll with LClick
      SetAddress(addr & 0xFFFFFFF0);
    }
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

  const auto length = 16 / ((GetColumnCount(m_type) == 2) ? 4 : GetColumnCount(m_type));

  u64 value = PowerPC::HostRead_U64(addr);

  QApplication::clipboard()->setText(
      QStringLiteral("%1").arg(value, length * 2, 16, QLatin1Char('0')).left(length * 2));
}

void MemoryViewWidget::OnContextMenu()
{
  auto* menu = new QMenu(this);

  menu->addAction(tr("Copy Address"), this, &MemoryViewWidget::OnCopyAddress);

  auto* copy_hex = menu->addAction(tr("Copy Hex"), this, &MemoryViewWidget::OnCopyHex);

  copy_hex->setEnabled(Core::GetState() != Core::State::Uninitialized &&
                       PowerPC::HostIsRAMAddress(GetContextAddress()));

  menu->addSeparator();

  menu->addAction(tr("Toggle Breakpoint"), this, &MemoryViewWidget::ToggleBreakpoint);

  menu->exec(QCursor::pos());
}
