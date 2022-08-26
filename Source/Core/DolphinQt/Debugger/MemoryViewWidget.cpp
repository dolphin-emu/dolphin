// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/MemoryViewWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QtGlobal>

#include <cmath>
#include <fmt/printf.h>

#include "Common/Align.h"
#include "Common/FloatUtils.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
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

constexpr auto USER_ROLE_IS_ROW_BREAKPOINT_CELL = Qt::UserRole;
constexpr auto USER_ROLE_CELL_ADDRESS = Qt::UserRole + 1;
constexpr auto USER_ROLE_VALUE_TYPE = Qt::UserRole + 2;

// Numbers for the scrollbar. These affect how much big the draggable part of the scrollbar is, how
// smooth it scrolls, and how much memory it traverses while dragging.
constexpr int SCROLLBAR_MINIMUM = 0;
constexpr int SCROLLBAR_PAGESTEP = 250;
constexpr int SCROLLBAR_MAXIMUM = 20000;
constexpr int SCROLLBAR_CENTER = SCROLLBAR_MAXIMUM / 2;

class MemoryViewTable final : public QTableWidget
{
public:
  explicit MemoryViewTable(MemoryViewWidget* parent) : QTableWidget(parent), m_view(parent)
  {
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setShowGrid(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionMode(SingleSelection);

    connect(this, &MemoryViewTable::customContextMenuRequested, m_view,
            &MemoryViewWidget::OnContextMenu);
    connect(this, &MemoryViewTable::itemChanged, this, &MemoryViewTable::OnItemChanged);
  }

  void resizeEvent(QResizeEvent* event) override
  {
    QTableWidget::resizeEvent(event);
    m_view->Update();
  }

  void keyPressEvent(QKeyEvent* event) override
  {
    switch (event->key())
    {
    case Qt::Key_Up:
      m_view->m_address -= m_view->m_bytes_per_row;
      m_view->Update();
      return;
    case Qt::Key_Down:
      m_view->m_address += m_view->m_bytes_per_row;
      m_view->Update();
      return;
    case Qt::Key_PageUp:
      m_view->m_address -= this->rowCount() * m_view->m_bytes_per_row;
      m_view->Update();
      return;
    case Qt::Key_PageDown:
      m_view->m_address += this->rowCount() * m_view->m_bytes_per_row;
      m_view->Update();
      return;
    default:
      QWidget::keyPressEvent(event);
      break;
    }
  }

  void wheelEvent(QWheelEvent* event) override
  {
    auto delta =
        -static_cast<int>(std::round((event->angleDelta().y() / (SCROLL_FRACTION_DEGREES * 8))));

    if (delta == 0)
      return;

    m_view->m_address += delta * m_view->m_bytes_per_row;
    m_view->Update();
  }

  void mousePressEvent(QMouseEvent* event) override
  {
    if (event->button() != Qt::LeftButton)
      return;

    auto* item = this->itemAt(event->pos());
    if (!item)
      return;

    if (item->data(USER_ROLE_IS_ROW_BREAKPOINT_CELL).toBool())
    {
      const u32 address = item->data(USER_ROLE_CELL_ADDRESS).toUInt();
      m_view->ToggleBreakpoint(address, true);
      m_view->Update();
    }
    else
    {
      QTableWidget::mousePressEvent(event);
    }
  }

  void OnItemChanged(QTableWidgetItem* item)
  {
    QString text = item->text();
    MemoryViewWidget::Type type =
        static_cast<MemoryViewWidget::Type>(item->data(USER_ROLE_VALUE_TYPE).toInt());
    std::vector<u8> bytes = m_view->ConvertTextToBytes(type, text);

    u32 address = item->data(USER_ROLE_CELL_ADDRESS).toUInt();
    u32 end_address = address + static_cast<u32>(bytes.size()) - 1;
    AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_view->GetAddressSpace());

    if (!bytes.empty() && accessors->IsValidAddress(address) &&
        accessors->IsValidAddress(end_address))
    {
      for (const u8 c : bytes)
        accessors->WriteU8(address++, c);
    }

    m_view->Update();
  }

private:
  MemoryViewWidget* m_view;
};

MemoryViewWidget::MemoryViewWidget(QWidget* parent) : QWidget(parent)
{
  auto* layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  m_table = new MemoryViewTable(this);
  layout->addWidget(m_table);

  // Since the Memory View is infinitely long -- it wraps around -- we can't use a normal scroll
  // bar, so this initializes a custom one that is always centered but otherwise still behaves more
  // or less like a regular scrollbar.
  m_scrollbar = new QScrollBar(this);
  m_scrollbar->setRange(SCROLLBAR_MINIMUM, SCROLLBAR_MAXIMUM);
  m_scrollbar->setPageStep(SCROLLBAR_PAGESTEP);
  m_scrollbar->setValue(SCROLLBAR_CENTER);
  connect(m_scrollbar, &QScrollBar::actionTriggered, this,
          &MemoryViewWidget::ScrollbarActionTriggered);
  connect(m_scrollbar, &QScrollBar::sliderReleased, this,
          &MemoryViewWidget::ScrollbarSliderReleased);
  layout->addWidget(m_scrollbar);

  this->setLayout(layout);

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, &MemoryViewWidget::UpdateFont);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] { Update(); });
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &MemoryViewWidget::Update);
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &MemoryViewWidget::Update);

  // Also calls update.
  UpdateFont();
}

void MemoryViewWidget::UpdateFont()
{
  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  m_font_vspace = fm.lineSpacing();
  // BoundingRect is too unpredictable, a custom one would be needed for each view type. Different
  // fonts have wildly different spacing between two characters and horizontalAdvance includes
  // spacing.
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  m_font_width = fm.horizontalAdvance(QLatin1Char('0'));
#else
  m_font_width = fm.width(QLatin1Char('0'));
#endif
  m_table->setFont(Settings::Instance().GetDebugFont());
  Update();
}

constexpr int GetTypeSize(MemoryViewWidget::Type type)
{
  switch (type)
  {
  case MemoryViewWidget::Type::ASCII:
  case MemoryViewWidget::Type::Hex8:
  case MemoryViewWidget::Type::Unsigned8:
  case MemoryViewWidget::Type::Signed8:
    return 1;
  case MemoryViewWidget::Type::Unsigned16:
  case MemoryViewWidget::Type::Signed16:
  case MemoryViewWidget::Type::Hex16:
    return 2;
  case MemoryViewWidget::Type::Hex32:
  case MemoryViewWidget::Type::Unsigned32:
  case MemoryViewWidget::Type::Signed32:
  case MemoryViewWidget::Type::Float32:
    return 4;
  case MemoryViewWidget::Type::Double:
  case MemoryViewWidget::Type::Hex64:
    return 8;
  default:
    return 1;
  }
}

constexpr int GetCharacterCount(MemoryViewWidget::Type type)
{
  // Max number of characters +1 for spacing between columns.
  switch (type)
  {
  case MemoryViewWidget::Type::ASCII:  // A
    return 2;
  case MemoryViewWidget::Type::Hex8:  // Byte = FF
    return 3;
  case MemoryViewWidget::Type::Unsigned8:  // UCHAR_MAX = 255
    return 4;
  case MemoryViewWidget::Type::Hex16:    // 2 Bytes = FFFF
  case MemoryViewWidget::Type::Signed8:  // CHAR_MIN = -128
    return 5;
  case MemoryViewWidget::Type::Unsigned16:  // USHORT_MAX = 65535
    return 6;
  case MemoryViewWidget::Type::Signed16:  // SHORT_MIN = -32768
    return 7;
  case MemoryViewWidget::Type::Hex32:  // 4 Bytes = FFFFFFFF
    return 9;
  case MemoryViewWidget::Type::Float32:     // Rounded and Negative FLT_MAX = -3.403e+38
  case MemoryViewWidget::Type::Unsigned32:  // UINT_MAX = 4294967295
    return 11;
  case MemoryViewWidget::Type::Double:    // Rounded and Negative DBL_MAX = -1.798e+308
  case MemoryViewWidget::Type::Signed32:  // INT_MIN = -2147483648
    return 12;
  case MemoryViewWidget::Type::Hex64:  // For dual_view + Double. 8 Bytes = FFFFFFFFFFFFFFFF
    return 17;
  default:
    return 10;
  }
}

void MemoryViewWidget::Update()
{
  const QSignalBlocker blocker(m_table);

  m_table->clearSelection();

  u32 address = m_address;
  address = Common::AlignDown(address, m_alignment);

  const int data_columns = m_bytes_per_row / GetTypeSize(m_type);

  if (m_dual_view)
    m_table->setColumnCount(2 + 2 * data_columns);
  else
    m_table->setColumnCount(2 + data_columns);

  if (m_table->rowCount() == 0)
    m_table->setRowCount(1);

  // This sets all row heights and determines horizontal ascii spacing.
  m_table->verticalHeader()->setDefaultSectionSize(m_font_vspace - 1);
  m_table->verticalHeader()->setMinimumSectionSize(m_font_vspace - 1);
  m_table->horizontalHeader()->setMinimumSectionSize(m_font_width * 2);
  m_table->setTextElideMode(Qt::TextElideMode::ElideNone);

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);

  // Calculate (roughly) how many rows will fit in our table
  const int rows =
      std::round((m_table->height() / static_cast<float>(m_table->rowHeight(0))) - 0.25);

  m_table->setRowCount(rows);

  for (int i = 0; i < rows; i++)
  {
    u32 row_address = address - ((m_table->rowCount() / 2) * m_bytes_per_row) + i * m_bytes_per_row;

    auto* bp_item = new QTableWidgetItem;
    bp_item->setFlags(Qt::ItemIsEnabled);
    bp_item->setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, true);
    bp_item->setData(USER_ROLE_CELL_ADDRESS, row_address);
    bp_item->setData(USER_ROLE_VALUE_TYPE, static_cast<int>(Type::Null));

    m_table->setItem(i, 0, bp_item);

    auto* row_item =
        new QTableWidgetItem(QStringLiteral("%1").arg(row_address, 8, 16, QLatin1Char('0')));

    row_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    row_item->setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, false);
    row_item->setData(USER_ROLE_CELL_ADDRESS, row_address);
    row_item->setData(USER_ROLE_VALUE_TYPE, static_cast<int>(Type::Null));

    m_table->setItem(i, 1, row_item);

    if (row_address == address)
      row_item->setSelected(true);

    if (Core::GetState() != Core::State::Paused || !accessors->IsValidAddress(row_address))
    {
      for (int c = 2; c < m_table->columnCount(); c++)
      {
        auto* item = new QTableWidgetItem(QStringLiteral("-"));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, false);
        item->setData(USER_ROLE_CELL_ADDRESS, row_address);
        item->setData(USER_ROLE_VALUE_TYPE, static_cast<int>(Type::Null));

        m_table->setItem(i, c, item);
      }

      continue;
    }
  }

  int starting_column = 2;

  if (m_dual_view)
  {
    // Match left columns to number of right columns.
    Type left_type = Type::Hex32;
    if (GetTypeSize(m_type) == 1)
      left_type = Type::Hex8;
    else if (GetTypeSize(m_type) == 2)
      left_type = Type::Hex16;
    else if (GetTypeSize(m_type) == 8)
      left_type = Type::Hex64;

    UpdateColumns(left_type, starting_column);

    const int column_count = m_bytes_per_row / GetTypeSize(left_type);

    // Update column width
    for (int i = starting_column; i < starting_column + column_count - 1; i++)
      m_table->setColumnWidth(i, m_font_width * GetCharacterCount(left_type));

    // Extra spacing between dual views.
    m_table->setColumnWidth(starting_column + column_count - 1,
                            m_font_width * (GetCharacterCount(left_type) + 2));

    starting_column += column_count;
  }

  UpdateColumns(m_type, starting_column);
  UpdateBreakpointTags();

  m_table->setColumnWidth(0, m_table->rowHeight(0));

  for (int i = starting_column; i <= m_table->columnCount(); i++)
    m_table->setColumnWidth(i, m_font_width * GetCharacterCount(m_type));

  m_table->viewport()->update();
  m_table->update();
  update();
}

void MemoryViewWidget::UpdateColumns(Type type, int first_column)
{
  if (Core::GetState() != Core::State::Paused)
    return;

  const int data_columns = m_bytes_per_row / GetTypeSize(type);
  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);

  auto text_alignment = Qt::AlignLeft;
  if (type == Type::Signed32 || type == Type::Unsigned32 || type == Type::Signed16 ||
      type == Type::Unsigned16 || type == Type::Signed8 || type == Type::Unsigned8)
  {
    text_alignment = Qt::AlignRight;
  }

  for (int i = 0; i < m_table->rowCount(); i++)
  {
    u32 row_address = m_table->item(i, 1)->data(USER_ROLE_CELL_ADDRESS).toUInt();
    if (!accessors->IsValidAddress(row_address))
      continue;

    auto update_values = [&](auto value_to_string) {
      for (int c = 0; c < data_columns; c++)
      {
        auto* cell_item = new QTableWidgetItem;
        cell_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        cell_item->setTextAlignment(text_alignment);

        const u32 cell_address = row_address + c * GetTypeSize(type);

        m_table->setItem(i, first_column + c, cell_item);

        if (accessors->IsValidAddress(cell_address))
        {
          cell_item->setText(value_to_string(cell_address));
          cell_item->setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, false);
          cell_item->setData(USER_ROLE_CELL_ADDRESS, cell_address);
          cell_item->setData(USER_ROLE_VALUE_TYPE, static_cast<int>(type));
        }
        else
        {
          cell_item->setText(QStringLiteral("-"));
          cell_item->setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, false);
          cell_item->setData(USER_ROLE_CELL_ADDRESS, cell_address);
          cell_item->setData(USER_ROLE_VALUE_TYPE, static_cast<int>(Type::Null));
        }
      }
    };
    switch (type)
    {
    case Type::Hex8:
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
    case Type::Hex16:
      update_values([&accessors](u32 address) {
        const u16 value = accessors->ReadU16(address);
        return QStringLiteral("%1").arg(value, 4, 16, QLatin1Char('0'));
      });
      break;
    case Type::Hex32:
      update_values([&accessors](u32 address) {
        const u32 value = accessors->ReadU32(address);
        return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
      });
      break;
    case Type::Hex64:
      update_values([&accessors](u32 address) {
        const u64 value = accessors->ReadU64(address);
        return QStringLiteral("%1").arg(value, 16, 16, QLatin1Char('0'));
      });
      break;
    case Type::Unsigned8:
      update_values(
          [&accessors](u32 address) { return QString::number(accessors->ReadU8(address)); });
      break;
    case Type::Unsigned16:
      update_values(
          [&accessors](u32 address) { return QString::number(accessors->ReadU16(address)); });
      break;
    case Type::Unsigned32:
      update_values(
          [&accessors](u32 address) { return QString::number(accessors->ReadU32(address)); });
      break;
    case Type::Signed8:
      update_values([&accessors](u32 address) {
        return QString::number(Common::BitCast<s8>(accessors->ReadU8(address)));
      });
      break;
    case Type::Signed16:
      update_values([&accessors](u32 address) {
        return QString::number(Common::BitCast<s16>(accessors->ReadU16(address)));
      });
      break;
    case Type::Signed32:
      update_values([&accessors](u32 address) {
        return QString::number(Common::BitCast<s32>(accessors->ReadU32(address)));
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
    case Type::Double:
      update_values([&accessors](u32 address) {
        QString string =
            QString::number(Common::BitCast<double>(accessors->ReadU64(address)), 'g', 4);
        // Align to first digit.
        if (!string.startsWith(QLatin1Char('-')))
          string.prepend(QLatin1Char(' '));

        return string;
      });
      break;
    }
  }
}

void MemoryViewWidget::UpdateBreakpointTags()
{
  if (Core::GetState() != Core::State::Paused)
    return;

  for (int i = 0; i < m_table->rowCount(); i++)
  {
    bool row_breakpoint = false;

    for (int c = 2; c < m_table->columnCount(); c++)
    {
      // Pull address from cell itself, helpful for dual column view.
      auto cell = m_table->item(i, c);
      u32 address = cell->data(USER_ROLE_CELL_ADDRESS).toUInt();

      if (address == 0)
      {
        row_breakpoint = false;
        continue;
      }

      // In dual view the only sizes that dont match up on both left and right views are for
      // Double, which uses two or four columns of hex32.
      if (m_address_space == AddressSpace::Type::Effective &&
          PowerPC::memchecks.GetMemCheck(address, GetTypeSize(m_type)) != nullptr)
      {
        row_breakpoint = true;
        cell->setBackground(Qt::red);
      }
    }

    if (row_breakpoint)
    {
      m_table->item(i, 0)->setData(
          Qt::DecorationRole,
          Resources::GetScaledThemeIcon("debugger_breakpoint")
              .pixmap(QSize(m_table->rowHeight(0) - 3, m_table->rowHeight(0) - 3)));
    }
  }
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

std::vector<u8> MemoryViewWidget::ConvertTextToBytes(Type type, QString input_text)
{
  if (type == Type::Null)
    return {};

  bool good = false;
  int radix = 0;

  switch (type)
  {
  case Type::ASCII:
  {
    const QByteArray qbytes = input_text.toUtf8();
    std::vector<u8> bytes;

    for (const char c : qbytes)
      bytes.push_back(static_cast<u8>(c));

    return bytes;
  }
  case Type::Float32:
  {
    const float float_value = input_text.toFloat(&good);

    if (good)
    {
      const u32 value = Common::BitCast<u32>(float_value);
      auto std_array = Common::BitCastToArray<u8>(Common::swap32(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Double:
  {
    const double double_value = input_text.toDouble(&good);

    if (good)
    {
      const u64 value = Common::BitCast<u64>(double_value);
      auto std_array = Common::BitCastToArray<u8>(Common::swap64(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Signed8:
  {
    const short value = input_text.toShort(&good, radix);
    good &= std::numeric_limits<signed char>::min() <= value &&
            value <= std::numeric_limits<signed char>::max();
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap8(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Signed16:
  {
    const short value = input_text.toShort(&good, radix);
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap16(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Signed32:
  {
    const int value = input_text.toInt(&good, radix);
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap32(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Hex8:
    radix = 16;
    [[fallthrough]];
  case Type::Unsigned8:
  {
    const unsigned short value = input_text.toUShort(&good, radix);
    good &= (value & 0xFF00) == 0;
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap8(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Hex16:
    radix = 16;
    [[fallthrough]];
  case Type::Unsigned16:
  {
    const unsigned short value = input_text.toUShort(&good, radix);
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap16(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Hex32:
    radix = 16;
    [[fallthrough]];
  case Type::Unsigned32:
  {
    const u32 value = input_text.toUInt(&good, radix);
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap32(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::Hex64:
  {
    const u64 value = input_text.toULongLong(&good, 16);
    if (good)
    {
      auto std_array = Common::BitCastToArray<u8>(Common::swap64(value));
      return std::vector<u8>(std_array.begin(), std_array.end());
    }
    break;
  }
  case Type::HexString:
  {
    // Confirm it is only hex bytes
    const QRegularExpression is_hex(QStringLiteral("^([0-9A-F]{2})*$"),
                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = is_hex.match(input_text);
    good = match.hasMatch();
    if (good)
    {
      const QByteArray qbytes = QByteArray::fromHex(input_text.toUtf8());
      std::vector<u8> bytes;

      for (const char c : qbytes)
        bytes.push_back(static_cast<u8>(c));

      return bytes;
    }
    break;
  }
  }

  return {};
}

void MemoryViewWidget::SetDisplay(Type type, int bytes_per_row, int alignment, bool dual_view)
{
  m_type = type;
  m_bytes_per_row = bytes_per_row;
  m_dual_view = dual_view;
  if (alignment == 0)
    m_alignment = GetTypeSize(type);
  else
    m_alignment = alignment;

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

void MemoryViewWidget::ToggleBreakpoint(u32 addr, bool row)
{
  if (m_address_space != AddressSpace::Type::Effective)
    return;

  const auto length = GetTypeSize(m_type);
  const int breaks = row ? (m_bytes_per_row / length) : 1;
  bool overlap = false;

  // Row breakpoint should either remove any breakpoint left on the row, or activate all
  // breakpoints.
  if (row && PowerPC::memchecks.OverlapsMemcheck(addr, m_bytes_per_row))
    overlap = true;

  for (int i = 0; i < breaks; i++)
  {
    u32 address = addr + length * i;
    TMemCheck* check_ptr = PowerPC::memchecks.GetMemCheck(address, length);

    if (check_ptr == nullptr && !overlap)
    {
      TMemCheck check;
      check.start_address = address;
      check.end_address = check.start_address + length - 1;
      check.is_ranged = length > 0;
      check.is_break_on_read = (m_bp_type == BPType::ReadOnly || m_bp_type == BPType::ReadWrite);
      check.is_break_on_write = (m_bp_type == BPType::WriteOnly || m_bp_type == BPType::ReadWrite);
      check.log_on_hit = m_do_log;
      check.break_on_hit = true;

      PowerPC::memchecks.Add(check);
    }
    else if (check_ptr != nullptr)
    {
      // Using the pointer fixes misaligned breakpoints (0x11 breakpoint in 0x10 aligned view).
      PowerPC::memchecks.Remove(check_ptr->start_address);
    }
  }

  emit BreakpointsChanged();
  Update();
}

void MemoryViewWidget::OnCopyAddress(u32 addr)
{
  QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
}

void MemoryViewWidget::OnCopyHex(u32 addr)
{
  const auto length = GetTypeSize(m_type);

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);
  u64 value = accessors->ReadU64(addr);

  QApplication::clipboard()->setText(
      QStringLiteral("%1").arg(value, sizeof(u64) * 2, 16, QLatin1Char('0')).left(length * 2));
}

void MemoryViewWidget::OnContextMenu(const QPoint& pos)
{
  auto* item_selected = m_table->itemAt(pos);

  // We don't have a meaningful context menu to show for when the user right-clicks either free
  // space in the table or the row breakpoint cell.
  if (!item_selected || item_selected->data(USER_ROLE_IS_ROW_BREAKPOINT_CELL).toBool())
    return;

  const bool item_has_value =
      item_selected->data(USER_ROLE_VALUE_TYPE).toInt() != static_cast<int>(Type::Null);
  const u32 addr = item_selected->data(USER_ROLE_CELL_ADDRESS).toUInt();

  auto* menu = new QMenu(this);

  menu->addAction(tr("Copy Address"), this, [this, addr] { OnCopyAddress(addr); });

  auto* copy_hex = menu->addAction(tr("Copy Hex"), this, [this, addr] { OnCopyHex(addr); });

  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);
  copy_hex->setEnabled(item_has_value && Core::GetState() != Core::State::Uninitialized &&
                       accessors->IsValidAddress(addr));

  auto* copy_value = menu->addAction(tr("Copy Value"), this, [this, &pos] {
    // Re-fetch the item in case the underlying table has refreshed since the menu was opened.
    auto* item = m_table->itemAt(pos);
    if (item && item->data(USER_ROLE_VALUE_TYPE).toInt() != static_cast<int>(Type::Null))
      QApplication::clipboard()->setText(item->text());
  });
  copy_value->setEnabled(item_has_value);

  menu->addSeparator();

  menu->addAction(tr("Show in code"), this, [this, addr] { emit ShowCode(addr); });

  menu->addSeparator();

  menu->addAction(tr("Add to watch"), this, [this, addr] {
    const QString name = QStringLiteral("mem_%1").arg(addr, 8, 16, QLatin1Char('0'));
    emit RequestWatch(name, addr);
  });

  menu->addAction(tr("Toggle Breakpoint"), this, [this, addr] { ToggleBreakpoint(addr, false); });

  menu->exec(QCursor::pos());
}

void MemoryViewWidget::ScrollbarActionTriggered(int action)
{
  const int difference = m_scrollbar->sliderPosition() - m_scrollbar->value();
  if (difference == 0)
    return;

  if (m_scrollbar->isSliderDown())
  {
    // User is currently dragging the scrollbar.
    // Adjust the memory view by the exact drag difference.
    SetAddress(m_address + difference * m_bytes_per_row);
  }
  else
  {
    if (std::abs(difference) == 1)
    {
      // User clicked the arrows at the top or bottom, go up/down one row.
      SetAddress(m_address + difference * m_bytes_per_row);
    }
    else
    {
      // User clicked the free part of the scrollbar, go up/down one page.
      SetAddress(m_address + (difference < 0 ? -1 : 1) * m_bytes_per_row * m_table->rowCount());
    }

    // Manually reset the draggable part of the bar back to the center.
    m_scrollbar->setSliderPosition(SCROLLBAR_CENTER);
  }
}

void MemoryViewWidget::ScrollbarSliderReleased()
{
  // Reset the draggable part of the bar back to the center.
  m_scrollbar->setValue(SCROLLBAR_CENTER);
}
