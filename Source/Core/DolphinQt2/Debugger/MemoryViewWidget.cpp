// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Debugger/MemoryViewWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <cmath>

#include "Core/Core.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"

MemoryViewWidget::MemoryViewWidget(QWidget* parent) : QTableWidget(parent)
{
  horizontalHeader()->hide();
  verticalHeader()->hide();
  verticalScrollBar()->setHidden(true);
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

    if (PowerPC::memchecks.OverlapsMemcheck(addr, 16))
    {
      bp_item->setData(Qt::DecorationRole,
                       Resources::GetScaledThemeIcon("debugger_breakpoint").pixmap(QSize(24, 24)));
    }

    setItem(i, 0, bp_item);

    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    addr_item->setData(Qt::UserRole, addr);
    addr_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    setItem(i, 1, addr_item);

    if (addr == m_address)
      addr_item->setSelected(true);

    if (Core::GetState() != Core::State::Paused || !PowerPC::HostIsRAMAddress(addr))
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

    auto* description_item =
        new QTableWidgetItem(QString::fromStdString(PowerPC::debug_interface.GetDescription(addr)));

    description_item->setForeground(Qt::blue);
    description_item->setFlags(Qt::ItemIsEnabled);

    setItem(i, columnCount() - 1, description_item);

    auto update_values = [this, i, addr](auto value_to_string) {
      for (int c = 0; c < GetColumnCount(m_type); c++)
      {
        auto* hex_item = new QTableWidgetItem;
        const u32 address = addr + c * (16 / GetColumnCount(m_type));
        if (PowerPC::HostIsRAMAddress(address))
        {
          hex_item->setText(value_to_string(address));
          hex_item->setFlags(Qt::ItemIsEnabled);
          hex_item->setData(Qt::UserRole, addr);
        }
        else
        {
          hex_item->setFlags(0);
          hex_item->setText(QStringLiteral("-"));
        }
        setItem(i, 2 + c, hex_item);
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
      update_values([](u32 address) {
        const char value = PowerPC::HostRead_U8(address);
        return std::isprint(value) ? QString{QChar::fromLatin1(value)} : QStringLiteral(".");
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
      update_values([](u32 address) { return QString::number(PowerPC::HostRead_F32(address)); });
      break;
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
    m_address -= 3 * 16;
    Update();
    return;
  case Qt::Key_Down:
    m_address += 3 * 16;
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

void MemoryViewWidget::ToggleBreakpoint()
{
  u32 addr = GetContextAddress();

  if (!PowerPC::memchecks.OverlapsMemcheck(addr, 16))
  {
    TMemCheck check;

    check.start_address = addr;
    check.end_address = check.start_address + 15;
    check.is_ranged = true;
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

void MemoryViewWidget::wheelEvent(QWheelEvent* event)
{
  int delta = event->delta() > 0 ? -1 : 1;

  m_address += delta * 3 * 16;
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
      ToggleBreakpoint();
    else
      SetAddress(addr);

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

  u64 a = PowerPC::HostRead_U64(addr);
  u64 b = PowerPC::HostRead_U64(addr + sizeof(u64));

  QApplication::clipboard()->setText(
      QStringLiteral("%1%2").arg(a, 16, 16, QLatin1Char('0')).arg(b, 16, 16, QLatin1Char('0')));
}

void MemoryViewWidget::OnContextMenu()
{
  auto* menu = new QMenu(this);

  AddAction(menu, tr("Copy Address"), this, &MemoryViewWidget::OnCopyAddress);

  auto* copy_hex = AddAction(menu, tr("Copy Hex"), this, &MemoryViewWidget::OnCopyHex);

  copy_hex->setEnabled(Core::GetState() != Core::State::Uninitialized &&
                       PowerPC::HostIsRAMAddress(GetContextAddress()));

  menu->addSeparator();

  AddAction(menu, tr("Toggle Breakpoint"), this, &MemoryViewWidget::ToggleBreakpoint);

  menu->exec(QCursor::pos());
}
