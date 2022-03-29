// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/MemoryViewWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <qtimer.h>

#include <cmath>

#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/HW/AddressSpace.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"
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

  setStyleSheet(
      QStringLiteral("QTableView {selection-background-color: #0090FF; selection-color:#FFFFFF}"));

  setContextMenuPolicy(Qt::CustomContextMenu);

  m_timer = new QTimer;
  m_timer->setInterval(600);
  m_auto_update_action = new QAction(tr("Auto update memory values (600ms)"), this);
  m_auto_update_action->setCheckable(true);

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &MemoryViewWidget::UpdateFont);
  connect(this, &MemoryViewWidget::customContextMenuRequested, this,
          &MemoryViewWidget::OnContextMenu);
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &MemoryViewWidget::Update);
  connect(m_auto_update_action, &QAction::toggled, [this](bool checked) {
    if (checked)
      m_timer->start();
    else
      m_timer->stop();
  });
  connect(m_timer, &QTimer::timeout, this, &MemoryViewWidget::UpdateTable);

  UpdateFont();
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

void MemoryViewWidget::UpdateFont()
{
  setFont(Settings::Instance().GetDebugFont());
  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  m_font_vspace = fm.lineSpacing();
  m_font_width = fm.horizontalAdvance(QLatin1Char('0'));
  Update();
}

void MemoryViewWidget::Update()
{
  if (m_updating)
    return;

  m_updating = true;
  clear();
  clearSelection();

  setColumnCount(2 + GetColumnCount(m_type));

  if (rowCount() == 0)
    setRowCount(1);

  // This sets all row heights and determines horizontal ascii spacing.
  verticalHeader()->setDefaultSectionSize(m_font_vspace - 1);
  verticalHeader()->setMinimumSectionSize(m_font_vspace - 1);
  horizontalHeader()->setMinimumSectionSize(m_font_width * 2);

  // Calculate (roughly) how many rows will fit in our table
  int rows = std::round((height() / static_cast<float>(rowHeight(0))) - 0.25);

  setRowCount(rows);

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);

  for (int i = 0; i < rows; i++)
  {
    u32 addr = m_address_view - ((rowCount() / 2) * 16) + i * 16;

    auto* bp_item = new QTableWidgetItem;
    bp_item->setFlags(Qt::ItemIsEnabled);
    bp_item->setData(Qt::UserRole, addr);

    setItem(i, 0, bp_item);

    auto* addr_item = new QTableWidgetItem(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));

    addr_item->setData(Qt::UserRole, addr);
    addr_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    setItem(i, 1, addr_item);

    if (!accessors->IsValidAddress(addr))
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

    if (m_address_space == AddressSpace::Type::Effective &&
        PowerPC::memchecks.OverlapsMemcheck(addr, 16))
    {
      bp_item->setData(Qt::DecorationRole, Resources::GetScaledThemeIcon("debugger_breakpoint")
                                               .pixmap(QSize(rowHeight(0) - 3, rowHeight(0) - 3)));
    }

    // Has never worked, gets overwritten.
    // if (m_address_space == AddressSpace::Type::Effective)
    //{
    //  auto* description_item = new QTableWidgetItem(
    //      QString::fromStdString(PowerPC::debug_interface.GetDescription(addr)));

    //  description_item->setForeground(Qt::blue);
    //  description_item->setFlags(Qt::ItemIsEnabled);

    //  setItem(i, columnCount() - 1, description_item);
    //}
  }

  UpdateTable();

  setColumnWidth(0, rowHeight(0));
  int width = 0;

  switch (m_type)
  {
  case Type::U8:
    width = m_font_width * 3;
    break;
  case Type::ASCII:
    width = m_font_width * 2;
    break;
  case Type::U16:
    width = m_font_width * 5;
    break;
  case Type::U32:
    width = m_font_width * 10;
    break;
  case Type::Float32:
    width = m_font_width * 12;
    break;
  }

  for (int i = 2; i < columnCount(); i++)
  {
    setColumnWidth(i, width);
  }

  viewport()->update();
  update();
  m_updating = false;
}

void MemoryViewWidget::UpdateTable()
{
  // Seems to prevent memory read errors while game is running.
  Core::RunAsCPUThread([&] {
    const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);
    int rows = rowCount();

    for (int i = 0; i < rows; i++)
    {
      u32 addr = m_address_view - ((rowCount() / 2) * 16) + i * 16;

      auto update_values = [&](auto value_to_string) {
        for (int c = 0; c < GetColumnCount(m_type); c++)
        {
          QTableWidgetItem* hex_item;
          if (item(i, c + 2) != 0)
            hex_item = item(i, c + 2);
          else
            hex_item = new QTableWidgetItem;

          hex_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
          const u32 address = addr + c * (16 / GetColumnCount(m_type));

          if (address == m_address_target)
            hex_item->setBackground(QColor(220, 235, 235, 255));
          else if (m_address_space == AddressSpace::Type::Effective &&
                   PowerPC::memchecks.OverlapsMemcheck(address, 16 / GetColumnCount(m_type)))
          {
            hex_item->setBackground(Qt::red);
          }

          setItem(i, 2 + c, hex_item);

          if (accessors->IsValidAddress(address))
          {
            QString value = value_to_string(address);
            QColor bcolor = hex_item->background().color();
            // Color recently changed items.
            if (hex_item->text() != value && !hex_item->text().isEmpty())
            {
              hex_item->setBackground(QColor(0x77FFFF));
            }
            else if (bcolor.red() > 0 && bcolor != QColor(0xFFFFFF) && bcolor != QColor(Qt::red) &&
                     bcolor != QColor(220, 235, 235, 255))
            {
              hex_item->setBackground(hex_item->background().color().lighter(107));
            }

            hex_item->setText(value);
            hex_item->setData(Qt::UserRole, address);
          }
          else
          {
            hex_item->setFlags({});
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
          return IsPrintableCharacter(value) ? QString{QChar::fromLatin1(value)} :
                                               QString{QChar::fromLatin1('.')};
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
        update_values([&accessors](u32 address) {
          QString string = QString::number(accessors->ReadF32(address), 'g', 4);
          // Align to first digit.
          if (!string.startsWith(QLatin1Char('-')))
            string.prepend(QLatin1Char(' '));

          return string;
        });
        break;
      }
    }
  });
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
  if (m_address_view == address)
    return;

  m_address_target = address;
  m_address_view = address;
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
    m_address_view -= 16;
    Update();
    return;
  case Qt::Key_Down:
    m_address_view += 16;
    Update();
    return;
  case Qt::Key_PageUp:
    m_address_view -= rowCount() * 16;
    Update();
    return;
  case Qt::Key_PageDown:
    m_address_view += rowCount() * 16;
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

  m_address_view += delta * 16;

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
      m_address_view = addr & 0xFFFFFFF0;

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
      QStringLiteral("%1").arg(value, sizeof(u64) * 2, 16, QLatin1Char('0')).left(length * 2));
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

  menu->addAction(tr("Add to watch"), this, [this] {
    const u32 address = GetContextAddress();
    const QString name = QStringLiteral("mem_%1").arg(address, 8, 16, QLatin1Char('0'));
    emit RequestWatch(name, address);
  });
  menu->addAction(tr("Toggle Breakpoint"), this, &MemoryViewWidget::ToggleBreakpoint);
  menu->addAction(m_auto_update_action);

  menu->exec(QCursor::pos());
}
