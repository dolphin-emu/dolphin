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
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// "Most mouse types work in steps of 15 degrees, in which case the delta value is a multiple of
// 120; i.e., 120 units * 1/8 = 15 degrees." (http://doc.qt.io/qt-5/qwheelevent.html#angleDelta)
constexpr double SCROLL_FRACTION_DEGREES = 15.;

// Number of columns that don't contain the value of a memory address.
constexpr int MISC_COLUMNS = 2;
constexpr auto USER_ROLE_IS_ROW_BREAKPOINT_CELL = Qt::UserRole;
constexpr auto USER_ROLE_CELL_ADDRESS = Qt::UserRole + 1;
constexpr auto USER_ROLE_VALUE_TYPE = Qt::UserRole + 2;

// Numbers for the scrollbar. These affect how much big the draggable part of the scrollbar is, how
// smooth it scrolls, and how much memory it traverses while dragging.
constexpr int SCROLLBAR_MINIMUM = 0;
constexpr int SCROLLBAR_PAGESTEP = 250;
constexpr int SCROLLBAR_MAXIMUM = 20000;
constexpr int SCROLLBAR_CENTER = SCROLLBAR_MAXIMUM / 2;

const QString INVALID_MEMORY = QStringLiteral("-");

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
    // Selection will be set programmatically. User will still get an outline on clicked items.
    setSelectionMode(NoSelection);
    setTextElideMode(Qt::TextElideMode::ElideNone);

    // Prevent colors from changing based on focus.
    QPalette palette(m_view->palette());
    palette.setBrush(QPalette::Inactive, QPalette::Highlight, palette.brush(QPalette::Highlight));
    palette.setBrush(QPalette::Inactive, QPalette::HighlightedText,
                     palette.brush(QPalette::HighlightedText));
    setPalette(palette);

    setRowCount(30);
    setColumnCount(8);

    connect(this, &MemoryViewTable::customContextMenuRequested, m_view,
            &MemoryViewWidget::OnContextMenu);
    connect(this, &MemoryViewTable::itemChanged, this, &MemoryViewTable::OnItemChanged);
  }

  void resizeEvent(QResizeEvent* event) override
  {
    QTableWidget::resizeEvent(event);
    m_view->CreateTable();
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

    Core::CPUThreadGuard guard(Core::System::GetInstance());

    if (!bytes.empty() && accessors->IsValidAddress(guard, address) &&
        accessors->IsValidAddress(guard, end_address))
    {
      for (const u8 c : bytes)
        accessors->WriteU8(guard, address++, c);
    }

    m_view->Update();
  }

private:
  MemoryViewWidget* m_view;
};

MemoryViewWidget::MemoryViewWidget(QWidget* parent)
    : QWidget(parent), m_system(Core::System::GetInstance())
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
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          qOverload<>(&MemoryViewWidget::UpdateColumns));
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          qOverload<>(&MemoryViewWidget::UpdateColumns));
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &MemoryViewWidget::Update);

  // Also calls create table.
  UpdateFont();
}

void MemoryViewWidget::UpdateFont()
{
  const QFontMetrics fm(Settings::Instance().GetDebugFont());
  m_font_vspace = fm.lineSpacing();
  // BoundingRect is too unpredictable, a custom one would be needed for each view type. Different
  // fonts have wildly different spacing between two characters and horizontalAdvance includes
  // spacing.
  m_font_width = fm.horizontalAdvance(QLatin1Char('0'));
  m_table->setFont(Settings::Instance().GetDebugFont());

  CreateTable();
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

void MemoryViewWidget::CreateTable()
{
  m_table->clearContents();

  // This sets all row heights and determines horizontal ascii spacing.
  // Could be placed in UpdateFont() but doesn't apply correctly unless called more.
  m_table->verticalHeader()->setDefaultSectionSize(m_font_vspace - 1);
  m_table->verticalHeader()->setMinimumSectionSize(m_font_vspace - 1);
  m_table->horizontalHeader()->setMinimumSectionSize(m_font_width * 2);

  const QSignalBlocker blocker(m_table);

  // Set column and row parameters.
  // Span is the number of unique memory values covered in one row.
  const int data_span = m_bytes_per_row / GetTypeSize(m_type);
  m_data_columns = m_dual_view ? data_span * 2 : data_span;
  const int total_columns = MISC_COLUMNS + m_data_columns;

  const int rows =
      std::round((m_table->height() / static_cast<float>(m_table->rowHeight(0))) - 0.25);

  m_table->setColumnCount(total_columns);
  m_table->setRowCount(rows);
  m_table->setColumnWidth(0, m_table->rowHeight(0));

  // Get optional dual-view type
  std::optional<Type> left_type = std::nullopt;

  if (m_dual_view)
  {
    if (GetTypeSize(m_type) == 1)
      left_type = Type::Hex8;
    else if (GetTypeSize(m_type) == 2)
      left_type = Type::Hex16;
    else if (GetTypeSize(m_type) == 8)
      left_type = Type::Hex64;
    else
      left_type = Type::Hex32;
  }

  // Create cells and add data that won't be changing.
  // Breakpoint buttons
  auto bp_item = QTableWidgetItem();
  bp_item.setFlags(Qt::ItemIsEnabled);
  bp_item.setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, true);
  bp_item.setData(USER_ROLE_VALUE_TYPE, static_cast<int>(Type::Null));

  // Row Addresses
  auto row_item = QTableWidgetItem(INVALID_MEMORY);
  row_item.setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  row_item.setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, false);
  row_item.setData(USER_ROLE_VALUE_TYPE, static_cast<int>(Type::Null));

  // Data item
  auto item = QTableWidgetItem(INVALID_MEMORY);
  item.setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
  item.setData(USER_ROLE_IS_ROW_BREAKPOINT_CELL, false);

  for (int i = 0; i < rows; i++)
  {
    m_table->setItem(i, 0, bp_item.clone());
    m_table->setItem(i, 1, row_item.clone());

    for (int c = 0; c < m_data_columns; c++)
    {
      if (left_type && c < data_span)
      {
        item.setData(USER_ROLE_VALUE_TYPE, static_cast<int>(left_type.value()));
      }
      else
      {
        item.setData(USER_ROLE_VALUE_TYPE, static_cast<int>(m_type));

        // Left type will never be these.
        auto text_alignment = Qt::AlignLeft;
        if (m_type == Type::Signed32 || m_type == Type::Unsigned32 || m_type == Type::Signed16 ||
            m_type == Type::Unsigned16 || m_type == Type::Signed8 || m_type == Type::Unsigned8)
        {
          text_alignment = Qt::AlignRight;
        }
        item.setTextAlignment(text_alignment | Qt::AlignVCenter);
      }

      m_table->setItem(i, c + MISC_COLUMNS, item.clone());
    }
  }

  // Update column width
  int start_fill = MISC_COLUMNS;
  if (left_type)
  {
    const int width_left = m_font_width * GetCharacterCount(left_type.value());

    for (int i = 0; i < data_span - 1; i++)
      m_table->setColumnWidth(i + start_fill, width_left);

    // Extra spacing between dual views.
    m_table->setColumnWidth(start_fill + data_span - 1, width_left + m_font_width * 2);

    start_fill += data_span;
  }

  // If dual-view, updates the right-side columns only.
  const int width = m_font_width * GetCharacterCount(m_type);
  for (int i = start_fill; i < total_columns; i++)
    m_table->setColumnWidth(i, width);

  Update();
}

void MemoryViewWidget::Update()
{
  // Check if table is created
  if (m_table->item(1, 1) == nullptr)
    return;

  const QSignalBlocker blocker(m_table);
  m_table->clearSelection();

  // Update addresses
  const u32 address = Common::AlignDown(m_address, m_alignment);
  u32 row_address = address - (m_table->rowCount() / 2) * m_bytes_per_row;
  const int data_span = m_bytes_per_row / GetTypeSize(m_type);

  for (int i = 0; i < m_table->rowCount(); i++, row_address += m_bytes_per_row)
  {
    auto* bp_item = m_table->item(i, 0);
    bp_item->setData(USER_ROLE_CELL_ADDRESS, row_address);

    auto* row_item = m_table->item(i, 1);
    row_item->setText(QStringLiteral("%1").arg(row_address, 8, 16, QLatin1Char('0')));
    row_item->setData(USER_ROLE_CELL_ADDRESS, row_address);

    for (int c = 0; c < m_data_columns; c++)
    {
      auto* item = m_table->item(i, c + MISC_COLUMNS);

      u32 item_address;
      if (m_dual_view && c >= data_span)
        item_address = row_address + (c - data_span) * GetTypeSize(m_type);
      else
        item_address = row_address + c * GetTypeSize(m_type);

      item->setData(USER_ROLE_CELL_ADDRESS, item_address);
    }
  }

  UpdateColumns();
  UpdateBreakpointTags();

  m_table->viewport()->update();
  m_table->update();
  update();
}

void MemoryViewWidget::UpdateColumns()
{
  if (!isVisible())
    return;

  // Check if table is created
  if (m_table->item(1, 1) == nullptr)
    return;

  if (Core::GetState() == Core::State::Paused)
  {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    UpdateColumns(&guard);
  }
  else
  {
    // If the core is running, blank out the view of memory instead of reading anything.
    UpdateColumns(nullptr);
  }
}

void MemoryViewWidget::UpdateColumns(const Core::CPUThreadGuard* guard)
{
  // Check if table is created
  if (m_table->item(1, 1) == nullptr)
    return;

  const QSignalBlocker blocker(m_table);

  for (int i = 0; i < m_table->rowCount(); i++)
  {
    for (int c = 0; c < m_data_columns; c++)
    {
      auto* cell_item = m_table->item(i, c + MISC_COLUMNS);
      const u32 cell_address = cell_item->data(USER_ROLE_CELL_ADDRESS).toUInt();
      const Type type = static_cast<Type>(cell_item->data(USER_ROLE_VALUE_TYPE).toInt());

      cell_item->setText(guard ? ValueToString(*guard, cell_address, type) : INVALID_MEMORY);

      // Set search address to selected / colored
      if (cell_address == m_address_highlight)
        cell_item->setSelected(true);
    }
  }
}

// May only be called if we have taken on the role of the CPU thread
QString MemoryViewWidget::ValueToString(const Core::CPUThreadGuard& guard, u32 address, Type type)
{
  const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);
  if (!accessors->IsValidAddress(guard, address))
    return INVALID_MEMORY;

  switch (type)
  {
  case Type::Hex8:
  {
    const u8 value = accessors->ReadU8(guard, address);
    return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0'));
  }
  case Type::ASCII:
  {
    const char value = accessors->ReadU8(guard, address);
    return Common::IsPrintableCharacter(value) ? QString{QChar::fromLatin1(value)} :
                                                 QString{QChar::fromLatin1('.')};
  }
  case Type::Hex16:
  {
    const u16 value = accessors->ReadU16(guard, address);
    return QStringLiteral("%1").arg(value, 4, 16, QLatin1Char('0'));
  }
  case Type::Hex32:
  {
    const u32 value = accessors->ReadU32(guard, address);
    return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
  }
  case Type::Hex64:
  {
    const u64 value = accessors->ReadU64(guard, address);
    return QStringLiteral("%1").arg(value, 16, 16, QLatin1Char('0'));
  }
  case Type::Unsigned8:
    return QString::number(accessors->ReadU8(guard, address));
  case Type::Unsigned16:
    return QString::number(accessors->ReadU16(guard, address));
  case Type::Unsigned32:
    return QString::number(accessors->ReadU32(guard, address));
  case Type::Signed8:
    return QString::number(Common::BitCast<s8>(accessors->ReadU8(guard, address)));
  case Type::Signed16:
    return QString::number(Common::BitCast<s16>(accessors->ReadU16(guard, address)));
  case Type::Signed32:
    return QString::number(Common::BitCast<s32>(accessors->ReadU32(guard, address)));
  case Type::Float32:
  {
    QString string = QString::number(accessors->ReadF32(guard, address), 'g', 4);
    // Align to first digit.
    if (!string.startsWith(QLatin1Char('-')))
      string.prepend(QLatin1Char(' '));

    return string;
  }
  case Type::Double:
  {
    QString string =
        QString::number(Common::BitCast<double>(accessors->ReadU64(guard, address)), 'g', 4);
    // Align to first digit.
    if (!string.startsWith(QLatin1Char('-')))
      string.prepend(QLatin1Char(' '));

    return string;
  }
  default:
    return INVALID_MEMORY;
  }
}

void MemoryViewWidget::UpdateBreakpointTags()
{
  for (int i = 0; i < m_table->rowCount(); i++)
  {
    bool row_breakpoint = false;

    for (int c = 0; c < m_data_columns; c++)
    {
      // Pull address from cell itself, helpful for dual column view.
      auto cell = m_table->item(i, c + MISC_COLUMNS);
      const u32 address = cell->data(USER_ROLE_CELL_ADDRESS).toUInt();

      if (address == 0)
      {
        row_breakpoint = false;
        continue;
      }

      if (m_address_space == AddressSpace::Type::Effective &&
          m_system.GetPowerPC().GetMemChecks().GetMemCheck(address, GetTypeSize(m_type)) != nullptr)
      {
        row_breakpoint = true;
        cell->setBackground(Qt::red);
      }
      else
      {
        cell->setBackground(Qt::transparent);
      }
    }

    if (row_breakpoint)
    {
      m_table->item(i, 0)->setData(
          Qt::DecorationRole,
          Resources::GetThemeIcon("debugger_breakpoint")
              .pixmap(QSize(m_table->rowHeight(0) - 3, m_table->rowHeight(0) - 3)));
    }
    else
    {
      m_table->item(i, 0)->setData(Qt::DecorationRole, QIcon());
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
  default:
    // Do nothing
    break;
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

  CreateTable();
}

void MemoryViewWidget::SetBPType(BPType type)
{
  m_bp_type = type;
}

void MemoryViewWidget::SetAddress(u32 address)
{
  m_address_highlight = address;
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

  auto& memchecks = m_system.GetPowerPC().GetMemChecks();

  // Row breakpoint should either remove any breakpoint left on the row, or activate all
  // breakpoints.
  if (row && memchecks.OverlapsMemcheck(addr, m_bytes_per_row))
    overlap = true;

  for (int i = 0; i < breaks; i++)
  {
    u32 address = addr + length * i;
    TMemCheck* check_ptr = memchecks.GetMemCheck(address, length);

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

      memchecks.Add(std::move(check));
    }
    else if (check_ptr != nullptr)
    {
      // Using the pointer fixes misaligned breakpoints (0x11 breakpoint in 0x10 aligned view).
      memchecks.Remove(check_ptr->start_address);
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

  const u64 value = [addr, accessors] {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    return accessors->ReadU64(guard, addr);
  }();

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

  const u32 addr = item_selected->data(USER_ROLE_CELL_ADDRESS).toUInt();
  const bool item_has_value =
      item_selected->data(USER_ROLE_VALUE_TYPE).toInt() != static_cast<int>(Type::Null) &&
      [this, addr] {
        const AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_address_space);

        Core::CPUThreadGuard guard(Core::System::GetInstance());
        return accessors->IsValidAddress(guard, addr);
      }();

  auto* menu = new QMenu(this);

  menu->addAction(tr("Copy Address"), this, [this, addr] { OnCopyAddress(addr); });

  auto* copy_hex = menu->addAction(tr("Copy Hex"), this, [this, addr] { OnCopyHex(addr); });
  copy_hex->setEnabled(item_has_value);

  auto* copy_value = menu->addAction(tr("Copy Value"), this, [item_selected] {
    QApplication::clipboard()->setText(item_selected->text());
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
    m_address += difference * m_bytes_per_row;
    Update();
  }
  else
  {
    if (std::abs(difference) == 1)
    {
      // User clicked the arrows at the top or bottom, go up/down one row.
      m_address += difference * m_bytes_per_row;
    }
    else
    {
      // User clicked the free part of the scrollbar, go up/down one page.
      m_address += (difference < 0 ? -1 : 1) * m_bytes_per_row * m_table->rowCount();
    }

    Update();
    // Manually reset the draggable part of the bar back to the center.
    m_scrollbar->setSliderPosition(SCROLLBAR_CENTER);
  }
}

void MemoryViewWidget::ScrollbarSliderReleased()
{
  // Reset the draggable part of the bar back to the center.
  m_scrollbar->setValue(SCROLLBAR_CENTER);
}

void MemoryViewWidget::SetFocus() const
{
  m_table->setFocus();
}
