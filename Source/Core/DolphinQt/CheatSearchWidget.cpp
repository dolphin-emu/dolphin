// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatSearchWidget.h"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/BitUtils.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/Config/CheatCodeEditor.h"
#include "DolphinQt/Config/CheatWarningWidget.h"

#include "UICommon/GameFile.h"

constexpr size_t TABLE_MAX_ROWS = 1000;

constexpr int ADDRESS_TABLE_ADDRESS_ROLE = Qt::UserRole;
constexpr int ADDRESS_TABLE_RESULT_INDEX_ROLE = Qt::UserRole + 1;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION = 0;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_ADDRESS = 1;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE = 2;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE = 3;

constexpr int AR_SET_BYTE_CMD = 0x00;
constexpr int AR_SET_SHORT_CMD = 0x02;
constexpr int AR_SET_INT_CMD = 0x04;

CheatSearchWidget::CheatSearchWidget(
    std::vector<Cheats::MemoryRange> memory_ranges, PowerPC::RequestedAddressSpace address_space,
    Cheats::DataType data_type, bool data_aligned,
    std::function<void(ActionReplay::ARCode ar_code)> generate_ar_code_callback)
    : m_memory_ranges(std::move(memory_ranges)), m_address_space(address_space),
      m_data_type(data_type), m_data_aligned(data_aligned),
      m_generate_ar_code_callback(generate_ar_code_callback)
{
  setAttribute(Qt::WA_DeleteOnClose);
  CreateWidgets();
  ConnectWidgets();
  UpdateGuiTable();
}

CheatSearchWidget::~CheatSearchWidget() = default;

namespace
{
enum class CheatSearchValueRefType
{
  GivenValue,
  LastValue,
  UnknownValue,
};
}

Q_DECLARE_METATYPE(Cheats::CompareType);
Q_DECLARE_METATYPE(CheatSearchValueRefType);

static std::optional<Cheats::SearchValue> ParseValue(const std::string& str,
                                                     Cheats::DataType data_type)
{
  if (str.empty())
    return std::nullopt;

  Cheats::SearchValue sv;
  switch (data_type)
  {
  case Cheats::DataType::U8:
  {
    u8 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::U16:
  {
    u16 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::U32:
  {
    u32 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::U64:
  {
    u64 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::S8:
  {
    s8 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::S16:
  {
    s16 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::S32:
  {
    s32 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::S64:
  {
    s64 tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::F32:
  {
    float tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  case Cheats::DataType::F64:
  {
    double tmp;
    if (TryParse(str, &tmp))
    {
      sv.m_value = tmp;
      return sv;
    }
    break;
  }
  default:
    assert(0);
    break;
  }

  return std::nullopt;
}

static std::string GetValueString(const Cheats::SearchResult& value)
{
  if (!value.IsValueValid())
  {
    switch (value.m_value_state)
    {
    case Cheats::SearchResultValueState::AddressNotAccessible:
      return "(inaccessible)";
    default:
      assert(0);
      return "";
    }
  }

  const auto sv = value.GetAsSearchValue();
  if (!sv)
  {
    assert(0);
    return "";
  }

  switch (Cheats::GetDataType(*sv))
  {
  case Cheats::DataType::U8:
    return fmt::format("{}", std::get<u8>(sv->m_value));
  case Cheats::DataType::U16:
    return fmt::format("{}", std::get<u16>(sv->m_value));
  case Cheats::DataType::U32:
    return fmt::format("{}", std::get<u32>(sv->m_value));
  case Cheats::DataType::U64:
    return fmt::format("{}", std::get<u64>(sv->m_value));
  case Cheats::DataType::S8:
    return fmt::format("{}", std::get<s8>(sv->m_value));
  case Cheats::DataType::S16:
    return fmt::format("{}", std::get<s16>(sv->m_value));
  case Cheats::DataType::S32:
    return fmt::format("{}", std::get<s32>(sv->m_value));
  case Cheats::DataType::S64:
    return fmt::format("{}", std::get<s64>(sv->m_value));
  case Cheats::DataType::F32:
    return fmt::format("{}", std::get<float>(sv->m_value));
  case Cheats::DataType::F64:
    return fmt::format("{}", std::get<double>(sv->m_value));
  default:
    return "";
  }
}

void CheatSearchWidget::CreateWidgets()
{
  auto* value_layout = new QHBoxLayout();

  auto* instructions_label = new QLabel(tr("Keep addresses where value in memory"));
  value_layout->addWidget(instructions_label);

  m_compare_type_dropdown = new QComboBox();
  m_compare_type_dropdown->addItem(tr("is equal to"),
                                   QVariant::fromValue(Cheats::CompareType::Equal));
  m_compare_type_dropdown->addItem(tr("is not equal to"),
                                   QVariant::fromValue(Cheats::CompareType::NotEqual));
  m_compare_type_dropdown->addItem(tr("is less than"),
                                   QVariant::fromValue(Cheats::CompareType::Less));
  m_compare_type_dropdown->addItem(tr("is less than or equal to"),
                                   QVariant::fromValue(Cheats::CompareType::LessEqual));
  m_compare_type_dropdown->addItem(tr("is more than"),
                                   QVariant::fromValue(Cheats::CompareType::More));
  m_compare_type_dropdown->addItem(tr("is more than or equal to"),
                                   QVariant::fromValue(Cheats::CompareType::MoreEqual));
  value_layout->addWidget(m_compare_type_dropdown);

  m_value_source_dropdown = new QComboBox();
  m_value_source_dropdown->addItem(tr("this value:"),
                                   QVariant::fromValue(CheatSearchValueRefType::GivenValue));
  m_value_source_dropdown->addItem(tr("Last Value"),
                                   QVariant::fromValue(CheatSearchValueRefType::LastValue));
  m_value_source_dropdown->addItem(tr("unknown value"),
                                   QVariant::fromValue(CheatSearchValueRefType::UnknownValue));
  value_layout->addWidget(m_value_source_dropdown);

  m_given_value_text = new QLineEdit();
  value_layout->addWidget(m_given_value_text);

  auto* button_layout = new QHBoxLayout();
  m_next_scan_button = new QPushButton(tr("Search and Filter"));
  button_layout->addWidget(m_next_scan_button);
  m_refresh_values_button = new QPushButton(tr("Refresh Current Values"));
  button_layout->addWidget(m_refresh_values_button);
  m_reset_button = new QPushButton(tr("Reset Results"));
  button_layout->addWidget(m_reset_button);
  m_close_button = new QPushButton(tr("Close"));
  button_layout->addWidget(m_close_button);

  m_address_table = new QTableWidget();
  m_address_table->setContextMenuPolicy(Qt::CustomContextMenu);

  m_info_label = new QLabel(tr("Waiting for first scan..."));

  QVBoxLayout* layout = new QVBoxLayout();
  layout->addLayout(value_layout);
  layout->addLayout(button_layout);
  layout->addWidget(m_info_label);
  layout->addWidget(m_address_table);
  setLayout(layout);
}

void CheatSearchWidget::ConnectWidgets()
{
  connect(m_next_scan_button, &QPushButton::clicked, this, &CheatSearchWidget::OnNextScanClicked);
  connect(m_refresh_values_button, &QPushButton::clicked, this,
          &CheatSearchWidget::OnRefreshClicked);
  connect(m_reset_button, &QPushButton::clicked, this, &CheatSearchWidget::OnResetClicked);
  connect(m_close_button, &QPushButton::clicked, this, &CheatSearchWidget::OnCloseClicked);
  connect(m_address_table, &QTableWidget::itemChanged, this,
          &CheatSearchWidget::OnAddressTableItemChanged);
  connect(m_address_table, &QTableWidget::customContextMenuRequested, this,
          &CheatSearchWidget::OnAddressTableContextMenu);
  connect(m_value_source_dropdown, &QComboBox::currentTextChanged, this,
          &CheatSearchWidget::OnValueSourceChanged);
}

void CheatSearchWidget::OnNextScanClicked()
{
  const auto ref_type = m_value_source_dropdown->currentData().value<CheatSearchValueRefType>();
  if (ref_type == CheatSearchValueRefType::GivenValue)
  {
    const auto op = m_compare_type_dropdown->currentData().value<Cheats::CompareType>();
    const auto value = ParseValue(m_given_value_text->text().toStdString(), m_data_type);
    if (!value)
    {
      m_info_label->setText(tr("Failed to parse given value into target data type."));
      return;
    }

    if (m_results)
    {
      ApplySearchResults(Cheats::NextSearch(m_address_space, *m_results, op, *value));
    }
    else
    {
      ApplySearchResults(
          Cheats::NewSearch(m_memory_ranges, m_address_space, op, *value, m_data_aligned));
    }
  }
  else if (ref_type == CheatSearchValueRefType::LastValue)
  {
    if (!m_results)
    {
      m_info_label->setText(tr("Cannot compare against last value on first search."));
      return;
    }

    const auto op = m_compare_type_dropdown->currentData().value<Cheats::CompareType>();
    ApplySearchResults(Cheats::NextSearch(m_address_space, *m_results, op, m_data_type));
  }
  else if (ref_type == CheatSearchValueRefType::UnknownValue)
  {
    if (m_results)
    {
      ApplySearchResults(Cheats::UpdateValues(m_address_space, *m_results, m_data_type));
    }
    else
    {
      ApplySearchResults(
          Cheats::NewSearch(m_memory_ranges, m_address_space, m_data_type, m_data_aligned));
    }
  }
}

void CheatSearchWidget::OnRefreshClicked()
{
  if (!m_results)
  {
    m_info_label->setText(tr("Cannot refresh without results."));
    return;
  }

  const auto results = Cheats::UpdateValues(m_address_space, *m_results, m_data_type);
  if (!results.Succeeded())
  {
    m_info_label->setText(tr("Refresh failed. Please run the game for a bit and try again."));
    return;
  }

  m_address_table_current_values.clear();
  const bool too_many_results = results->size() > TABLE_MAX_ROWS;
  const size_t result_count_to_display = too_many_results ? TABLE_MAX_ROWS : results->size();
  for (size_t i = 0; i < result_count_to_display; ++i)
    m_address_table_current_values[(*results)[i].m_address] = (*results)[i];

  UpdateGuiTable();
  m_info_label->setText(tr("Refreshed current values."));
}

void CheatSearchWidget::OnResetClicked()
{
  m_results = std::nullopt;
  m_address_table_current_values.clear();

  UpdateGuiTable();
  m_info_label->setText(tr("Reset search. Waiting for first scan..."));
}

void CheatSearchWidget::OnCloseClicked()
{
  this->close();
}

void CheatSearchWidget::OnAddressTableItemChanged(QTableWidgetItem* item)
{
  const u32 address = item->data(ADDRESS_TABLE_ADDRESS_ROLE).toUInt();
  const int column = item->column();

  switch (column)
  {
  case ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION:
  {
    m_address_table_user_data[address].m_description = item->text().toStdString();
    break;
  }
  default:
    break;
  }
}

void CheatSearchWidget::OnAddressTableContextMenu()
{
  if (m_address_table->selectedItems().isEmpty())
    return;

  QMenu* menu = new QMenu(this);

  menu->addAction(tr("Generate Action Replay Code"), this, &CheatSearchWidget::GenerateARCode);

  menu->exec(QCursor::pos());
}

void CheatSearchWidget::OnValueSourceChanged()
{
  const auto ref_type = m_value_source_dropdown->currentData().value<CheatSearchValueRefType>();
  m_given_value_text->setEnabled(ref_type == CheatSearchValueRefType::GivenValue);
}

static std::vector<ActionReplay::AREntry> ResultToAREntries(const Cheats::SearchResult& result)
{
  const auto sv = result.GetAsSearchValue();
  if (!sv)
    return {};

  std::vector<ActionReplay::AREntry> codes;
  std::vector<u8> data = Cheats::GetValueAsByteVector(*sv);

  for (size_t i = 0; i < data.size(); ++i)
  {
    const u32 address = (result.m_address + i) & 0x01ff'ffffu;
    if (Common::AlignUp(address, 4) == address && i + 3 < data.size())
    {
      const u8 cmd = AR_SET_INT_CMD;
      const u32 val = Common::swap32(&data[i]);
      codes.emplace_back((cmd << 24) | address, val);
      i += 3;
    }
    else if (Common::AlignUp(address, 2) == address && i + 1 < data.size())
    {
      const u8 cmd = AR_SET_SHORT_CMD;
      const u32 val = Common::swap16(&data[i]);
      codes.emplace_back((cmd << 24) | address, val);
      ++i;
    }
    else
    {
      const u8 cmd = AR_SET_BYTE_CMD;
      const u32 val = data[i];
      codes.emplace_back((cmd << 24) | address, val);
    }
  }

  return codes;
}

void CheatSearchWidget::GenerateARCode()
{
  if (!m_generate_ar_code_callback)
    return;

  if (m_address_table->selectedItems().isEmpty())
    return;

  auto* item = m_address_table->selectedItems()[0];
  if (!item)
    return;

  const u32 index = item->data(ADDRESS_TABLE_RESULT_INDEX_ROLE).toUInt();
  if (!m_results || index >= m_results->size())
    return;

  const auto& sr = (*m_results)[index];

  // check if the address is actually addressable by the ActionReplay system
  if (((sr.m_address & 0x01ff'ffffu) | 0x8000'0000u) != sr.m_address)
  {
    m_info_label->setText(tr("Cannot generate AR code for this address."));
    return;
  }

  ActionReplay::ARCode ar_code;
  ar_code.enabled = true;
  ar_code.user_defined = true;
  ar_code.name = tr("Generated by search (Address %1)")
                     .arg(sr.m_address, 8, 16, QLatin1Char('0'))
                     .toStdString();
  ar_code.ops = ResultToAREntries(sr);
  if (ar_code.ops.empty())
  {
    m_info_label->setText(tr("Failed to generate AR code."));
    return;
  }

  m_generate_ar_code_callback(std::move(ar_code));
  m_info_label->setText(tr("Generated AR code."));
}

static size_t CountValidValues(const std::vector<Cheats::SearchResult>& results)
{
  size_t count = 0;
  for (const auto& r : results)
  {
    if (r.IsValueValid())
      ++count;
  }
  return count;
}

void CheatSearchWidget::ApplySearchResults(
    Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>> results)
{
  if (results.Succeeded())
  {
    const bool had_old_results = !!m_results;
    const size_t new_count = results->size();
    const size_t new_valid_count = CountValidValues(*results);

    if (had_old_results)
    {
      const size_t old_count = m_results->size();
      if (new_valid_count == new_count)
      {
        m_info_label->setText(
            tr("Scan succeeded. %1 address(es) were removed, %2 address(es) remain.")
                .arg(old_count - new_count)
                .arg(new_count));
      }
      else
      {
        m_info_label->setText(tr("Scan succeeded. %1 address(es) were removed, %2 address(es) "
                                 "remain. %3 address(es) could not be accessed in emulated memory.")
                                  .arg(old_count - new_count)
                                  .arg(new_count)
                                  .arg(new_count - new_valid_count));
      }
    }
    else
    {
      if (new_valid_count == new_count)
      {
        m_info_label->setText(tr("Scan succeeded. Found %1 address(es).").arg(new_count));
      }
      else
      {
        m_info_label->setText(tr("Scan succeeded. Found %1 address(es). %2 address(es) could not "
                                 "be accessed in emulated memory.")
                                  .arg(new_count)
                                  .arg(new_count - new_valid_count));
      }
    }

    m_results = std::move(*results);
    m_address_table_current_values.clear();

    UpdateGuiTable();
  }
  else
  {
    switch (results.Error())
    {
    case Cheats::SearchErrorCode::InvalidParameters:
      m_info_label->setText(tr("Invalid parameters given to search."));
      break;
    case Cheats::SearchErrorCode::VirtualAddressesCurrentlyNotAccessible:
      m_info_label->setText(tr("Search currently not possible in virtual address space. Please run "
                               "the game for a bit and try again."));
      break;
    default:
      m_info_label->setText(tr("Unknown error occurred."));
      break;
    }
  }
}

void CheatSearchWidget::UpdateGuiTable()
{
  const QSignalBlocker blocker(m_address_table);

  m_address_table->clear();
  m_address_table->setColumnCount(4);
  m_address_table->setHorizontalHeaderLabels(
      {tr("Description"), tr("Address"), tr("Last Value"), tr("Current Value")});

  const size_t result_count = m_results ? m_results->size() : 0;
  const bool too_many_results = result_count > TABLE_MAX_ROWS;
  const int result_count_to_display = int(too_many_results ? TABLE_MAX_ROWS : result_count);
  m_address_table->setRowCount(result_count_to_display);

  for (int i = 0; i < result_count_to_display; ++i)
  {
    const u32 address = (*m_results)[i].m_address;
    const auto user_data_it = m_address_table_user_data.find(address);
    const bool has_user_data = user_data_it != m_address_table_user_data.end();

    auto* description_item = new QTableWidgetItem();
    description_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    if (has_user_data)
      description_item->setText(QString::fromStdString(user_data_it->second.m_description));
    description_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    description_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION, description_item);

    auto* address_item = new QTableWidgetItem();
    address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address_item->setText(QStringLiteral("0x%1").arg(address, 8, 16, QLatin1Char('0')));
    address_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    address_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_ADDRESS, address_item);

    auto* last_value_item = new QTableWidgetItem();
    last_value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    last_value_item->setText(QString::fromStdString(GetValueString((*m_results)[i])));
    last_value_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    last_value_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE, last_value_item);

    auto* current_value_item = new QTableWidgetItem();
    current_value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    const auto curr_val_it = m_address_table_current_values.find(address);
    if (curr_val_it != m_address_table_current_values.end())
      current_value_item->setText(QString::fromStdString(GetValueString(curr_val_it->second)));
    else
      current_value_item->setText(QStringLiteral("---"));
    current_value_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    current_value_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE, current_value_item);
  }
}
