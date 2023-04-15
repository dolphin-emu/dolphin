// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatSearchWidget.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/BitUtils.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/ActionReplay.h"
#include "Core/CheatGeneration.h"
#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/Config/CheatCodeEditor.h"
#include "DolphinQt/Config/CheatWarningWidget.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"

constexpr size_t TABLE_MAX_ROWS = 1000;

constexpr int ADDRESS_TABLE_ADDRESS_ROLE = Qt::UserRole;
constexpr int ADDRESS_TABLE_RESULT_INDEX_ROLE = Qt::UserRole + 1;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_DESCRIPTION = 0;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_ADDRESS = 1;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE = 2;
constexpr int ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE = 3;

CheatSearchWidget::CheatSearchWidget(std::unique_ptr<Cheats::CheatSearchSessionBase> session)
    : m_session(std::move(session))
{
  setAttribute(Qt::WA_DeleteOnClose);
  CreateWidgets();
  ConnectWidgets();
  OnValueSourceChanged();
  UpdateGuiTable();
}

CheatSearchWidget::~CheatSearchWidget()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("cheatsearchwidget/displayhex"),
                    m_display_values_in_hex_checkbox->isChecked());
  if (m_session->IsIntegerType())
  {
    settings.setValue(QStringLiteral("cheatsearchwidget/parsehex"),
                      m_parse_values_as_hex_checkbox->isChecked());
  }
}

Q_DECLARE_METATYPE(Cheats::CompareType);
Q_DECLARE_METATYPE(Cheats::FilterType);

void CheatSearchWidget::CreateWidgets()
{
  QLabel* session_info_label = new QLabel();
  {
    QString ranges;
    size_t range_count = m_session->GetMemoryRangeCount();
    switch (range_count)
    {
    case 1:
    {
      auto m = m_session->GetMemoryRange(0);
      ranges =
          tr("[%1, %2]")
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m.m_start)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m.m_start + m.m_length - 1)));
      break;
    }
    case 2:
    {
      auto m0 = m_session->GetMemoryRange(0);
      auto m1 = m_session->GetMemoryRange(1);
      ranges =
          tr("[%1, %2] and [%3, %4]")
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m0.m_start)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m0.m_start + m0.m_length - 1)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m1.m_start)))
              .arg(QString::fromStdString(fmt::format("0x{0:08x}", m1.m_start + m1.m_length - 1)));
      break;
    }
    default:
      ranges = tr("%1 memory ranges").arg(range_count);
      break;
    }

    QString space;
    switch (m_session->GetAddressSpace())
    {
    case PowerPC::RequestedAddressSpace::Effective:
      space = tr("Address space by CPU state");
      break;
    case PowerPC::RequestedAddressSpace::Physical:
      space = tr("Physical address space");
      break;
    case PowerPC::RequestedAddressSpace::Virtual:
      space = tr("Virtual address space");
      break;
    default:
      space = tr("Unknown address space");
      break;
    }

    QString type;
    switch (m_session->GetDataType())
    {
    case Cheats::DataType::U8:
      type = tr("8-bit Unsigned Integer");
      break;
    case Cheats::DataType::U16:
      type = tr("16-bit Unsigned Integer");
      break;
    case Cheats::DataType::U32:
      type = tr("32-bit Unsigned Integer");
      break;
    case Cheats::DataType::U64:
      type = tr("64-bit Unsigned Integer");
      break;
    case Cheats::DataType::S8:
      type = tr("8-bit Signed Integer");
      break;
    case Cheats::DataType::S16:
      type = tr("16-bit Signed Integer");
      break;
    case Cheats::DataType::S32:
      type = tr("32-bit Signed Integer");
      break;
    case Cheats::DataType::S64:
      type = tr("64-bit Signed Integer");
      break;
    case Cheats::DataType::F32:
      type = tr("32-bit Float");
      break;
    case Cheats::DataType::F64:
      type = tr("64-bit Float");
      break;
    default:
      type = tr("Unknown data type");
      break;
    }
    QString aligned = m_session->GetAligned() ? tr("aligned") : tr("unaligned");
    session_info_label->setText(tr("%1, %2, %3, %4").arg(ranges).arg(space).arg(type).arg(aligned));
  }

  // i18n: This label is followed by a dropdown where the user can select things like "is equal to"
  // or "is less than or equal to", followed by another dropdown where the user can select "any
  // value", "last value", or "this value:". These three UI elements are intended to form a sentence
  // together. Because the UI elements can't be reordered by a translation, you may have to give
  // up on the idea of having them form a sentence depending on the grammar of your target language.
  auto* instructions_label = new QLabel(tr("Keep addresses where value in memory"));

  auto* value_layout = new QHBoxLayout();
  m_compare_type_dropdown = new QComboBox();
  m_compare_type_dropdown->addItem(tr("is equal to"),
                                   QVariant::fromValue(Cheats::CompareType::Equal));
  m_compare_type_dropdown->addItem(tr("is not equal to"),
                                   QVariant::fromValue(Cheats::CompareType::NotEqual));
  m_compare_type_dropdown->addItem(tr("is less than"),
                                   QVariant::fromValue(Cheats::CompareType::Less));
  m_compare_type_dropdown->addItem(tr("is less than or equal to"),
                                   QVariant::fromValue(Cheats::CompareType::LessOrEqual));
  m_compare_type_dropdown->addItem(tr("is greater than"),
                                   QVariant::fromValue(Cheats::CompareType::Greater));
  m_compare_type_dropdown->addItem(tr("is greater than or equal to"),
                                   QVariant::fromValue(Cheats::CompareType::GreaterOrEqual));
  value_layout->addWidget(m_compare_type_dropdown);

  m_value_source_dropdown = new QComboBox();
  m_value_source_dropdown->addItem(
      tr("this value:"), QVariant::fromValue(Cheats::FilterType::CompareAgainstSpecificValue));
  m_value_source_dropdown->addItem(
      tr("last value"), QVariant::fromValue(Cheats::FilterType::CompareAgainstLastValue));
  m_value_source_dropdown->addItem(tr("any value"),
                                   QVariant::fromValue(Cheats::FilterType::DoNotFilter));
  value_layout->addWidget(m_value_source_dropdown);

  m_given_value_text = new QLineEdit();
  value_layout->addWidget(m_given_value_text);

  auto& settings = Settings::GetQSettings();
  m_parse_values_as_hex_checkbox = new QCheckBox(tr("Parse as Hex"));
  if (m_session->IsIntegerType())
  {
    m_parse_values_as_hex_checkbox->setChecked(
        settings.value(QStringLiteral("cheatsearchwidget/parsehex")).toBool());
    value_layout->addWidget(m_parse_values_as_hex_checkbox);
  }

  auto* button_layout = new QHBoxLayout();
  m_next_scan_button = new QPushButton(tr("Search and Filter"));
  button_layout->addWidget(m_next_scan_button);
  m_refresh_values_button = new QPushButton(tr("Refresh Current Values"));
  button_layout->addWidget(m_refresh_values_button);
  m_reset_button = new QPushButton(tr("Reset Results"));
  button_layout->addWidget(m_reset_button);

  m_address_table = new QTableWidget();
  m_address_table->setContextMenuPolicy(Qt::CustomContextMenu);

  m_info_label_1 = new QLabel(tr("Waiting for first scan..."));
  m_info_label_2 = new QLabel();

  m_display_values_in_hex_checkbox = new QCheckBox(tr("Display values in Hex"));
  m_display_values_in_hex_checkbox->setChecked(
      settings.value(QStringLiteral("cheatsearchwidget/displayhex")).toBool());

  QVBoxLayout* layout = new QVBoxLayout();
  layout->addWidget(session_info_label);
  layout->addWidget(instructions_label);
  layout->addLayout(value_layout);
  layout->addLayout(button_layout);
  layout->addWidget(m_display_values_in_hex_checkbox);
  layout->addWidget(m_info_label_1);
  layout->addWidget(m_info_label_2);
  layout->addWidget(m_address_table);
  setLayout(layout);
}

void CheatSearchWidget::ConnectWidgets()
{
  connect(m_next_scan_button, &QPushButton::clicked, this, &CheatSearchWidget::OnNextScanClicked);
  connect(m_refresh_values_button, &QPushButton::clicked, this,
          &CheatSearchWidget::OnRefreshClicked);
  connect(m_reset_button, &QPushButton::clicked, this, &CheatSearchWidget::OnResetClicked);
  connect(m_address_table, &QTableWidget::itemChanged, this,
          &CheatSearchWidget::OnAddressTableItemChanged);
  connect(m_address_table, &QTableWidget::customContextMenuRequested, this,
          &CheatSearchWidget::OnAddressTableContextMenu);
  connect(m_value_source_dropdown, &QComboBox::currentTextChanged, this,
          &CheatSearchWidget::OnValueSourceChanged);
  connect(m_display_values_in_hex_checkbox, &QCheckBox::toggled, this,
          &CheatSearchWidget::OnDisplayHexCheckboxStateChanged);
}

void CheatSearchWidget::OnNextScanClicked()
{
  const bool had_old_results = m_session->WasFirstSearchDone();
  const size_t old_count = m_session->GetResultCount();

  const auto filter_type = m_value_source_dropdown->currentData().value<Cheats::FilterType>();
  if (filter_type == Cheats::FilterType::CompareAgainstLastValue &&
      !m_session->WasFirstSearchDone())
  {
    m_info_label_1->setText(tr("Cannot compare against last value on first search."));
    return;
  }
  m_session->SetFilterType(filter_type);
  m_session->SetCompareType(m_compare_type_dropdown->currentData().value<Cheats::CompareType>());
  if (filter_type == Cheats::FilterType::CompareAgainstSpecificValue)
  {
    QString search_value = m_given_value_text->text();
    if (m_session->IsIntegerType() || m_session->IsFloatingType())
      search_value = search_value.simplified().remove(QLatin1Char(' '));
    if (!m_session->SetValueFromString(search_value.toStdString(),
                                       m_parse_values_as_hex_checkbox->isChecked()))
    {
      m_info_label_1->setText(tr("Failed to parse given value into target data type."));
      return;
    }
  }

  const Cheats::SearchErrorCode error_code = [this] {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    return m_session->RunSearch(guard);
  }();

  if (error_code == Cheats::SearchErrorCode::Success)
  {
    const size_t new_count = m_session->GetResultCount();
    const size_t new_valid_count = m_session->GetValidValueCount();
    m_info_label_1->setText(tr("Scan succeeded."));

    if (had_old_results)
    {
      const QString removed_str =
          tr("%n address(es) were removed.", "", static_cast<int>(old_count - new_count));
      const QString remain_str = tr("%n address(es) remain.", "", static_cast<int>(new_count));

      if (new_valid_count == new_count)
      {
        m_info_label_2->setText(tr("%1 %2").arg(removed_str).arg(remain_str));
      }
      else
      {
        const QString inaccessible_str =
            tr("%n address(es) could not be accessed in emulated memory.", "",
               static_cast<int>(new_count - new_valid_count));

        m_info_label_2->setText(
            tr("%1 %2 %3").arg(removed_str).arg(remain_str).arg(inaccessible_str));
      }
    }
    else
    {
      const QString found_str = tr("Found %n address(es).", "", static_cast<int>(new_count));

      if (new_valid_count == new_count)
      {
        m_info_label_2->setText(found_str);
      }
      else
      {
        const QString inaccessible_str =
            tr("%n address(es) could not be accessed in emulated memory.", "",
               static_cast<int>(new_count - new_valid_count));

        m_info_label_2->setText(tr("%1 %2").arg(found_str).arg(inaccessible_str));
      }
    }

    m_address_table_current_values.clear();
    const bool show_in_hex = m_display_values_in_hex_checkbox->isChecked();
    const bool too_many_results = new_count > TABLE_MAX_ROWS;
    const size_t result_count_to_display = too_many_results ? TABLE_MAX_ROWS : new_count;
    for (size_t i = 0; i < result_count_to_display; ++i)
    {
      m_address_table_current_values[m_session->GetResultAddress(i)] =
          m_session->GetResultValueAsString(i, show_in_hex);
    }

    UpdateGuiTable();
  }
  else
  {
    switch (error_code)
    {
    case Cheats::SearchErrorCode::NoEmulationActive:
      m_info_label_1->setText(tr("No game is running."));
      break;
    case Cheats::SearchErrorCode::InvalidParameters:
      m_info_label_1->setText(tr("Invalid parameters given to search."));
      break;
    case Cheats::SearchErrorCode::VirtualAddressesCurrentlyNotAccessible:
      m_info_label_1->setText(
          tr("Search currently not possible in virtual address space. Please run "
             "the game for a bit and try again."));
      break;
    default:
      m_info_label_1->setText(tr("Unknown error occurred."));
      break;
    }
  }
}

bool CheatSearchWidget::RefreshValues()
{
  const size_t result_count = m_session->GetResultCount();
  if (result_count == 0)
  {
    m_info_label_1->setText(tr("Cannot refresh without results."));
    return false;
  }

  const bool too_many_results = result_count > TABLE_MAX_ROWS;
  std::unique_ptr<Cheats::CheatSearchSessionBase> tmp;
  if (too_many_results)
  {
    std::vector<size_t> value_indices;
    value_indices.reserve(TABLE_MAX_ROWS);
    for (size_t i = 0; i < TABLE_MAX_ROWS; ++i)
      value_indices.push_back(i);
    tmp = m_session->ClonePartial(value_indices);
  }
  else
  {
    tmp = m_session->Clone();
  }

  tmp->SetFilterType(Cheats::FilterType::DoNotFilter);

  const Cheats::SearchErrorCode error_code = [&tmp] {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    return tmp->RunSearch(guard);
  }();

  if (error_code != Cheats::SearchErrorCode::Success)
  {
    m_info_label_1->setText(tr("Refresh failed. Please run the game for a bit and try again."));
    return false;
  }

  m_address_table_current_values.clear();
  const bool show_in_hex = m_display_values_in_hex_checkbox->isChecked();
  const size_t result_count_to_display = tmp->GetResultCount();
  for (size_t i = 0; i < result_count_to_display; ++i)
  {
    m_address_table_current_values[tmp->GetResultAddress(i)] =
        tmp->GetResultValueAsString(i, show_in_hex);
  }

  UpdateGuiTable();
  m_info_label_1->setText(tr("Refreshed current values."));
  return true;
}

void CheatSearchWidget::OnRefreshClicked()
{
  RefreshValues();
}

void CheatSearchWidget::OnResetClicked()
{
  m_session->ResetResults();
  m_address_table_current_values.clear();

  UpdateGuiTable();
  m_info_label_1->setText(tr("Waiting for first scan..."));
  m_info_label_2->clear();
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

  auto* item = m_address_table->selectedItems()[0];
  const u32 address = item->data(ADDRESS_TABLE_ADDRESS_ROLE).toUInt();

  QMenu* menu = new QMenu(this);

  menu->addAction(tr("Show in memory"), [this, address] { emit ShowMemory(address); });
  menu->addAction(tr("Add to watch"), this, [this, address] {
    const QString name = QStringLiteral("mem_%1").arg(address, 8, 16, QLatin1Char('0'));
    emit RequestWatch(name, address);
  });
  menu->addAction(tr("Generate Action Replay Code"), this, &CheatSearchWidget::GenerateARCode);

  menu->exec(QCursor::pos());
}

void CheatSearchWidget::OnValueSourceChanged()
{
  const auto filter_type = m_value_source_dropdown->currentData().value<Cheats::FilterType>();
  const bool is_value_search = filter_type == Cheats::FilterType::CompareAgainstSpecificValue;
  m_given_value_text->setEnabled(is_value_search);
  m_parse_values_as_hex_checkbox->setEnabled(is_value_search && m_session->IsIntegerType());
}

void CheatSearchWidget::OnDisplayHexCheckboxStateChanged()
{
  if (!m_session->WasFirstSearchDone())
    return;

  if (!RefreshValues())
    UpdateGuiTable();
}

void CheatSearchWidget::GenerateARCode()
{
  if (m_address_table->selectedItems().isEmpty())
    return;

  auto* item = m_address_table->selectedItems()[0];
  if (!item)
    return;

  const u32 index = item->data(ADDRESS_TABLE_RESULT_INDEX_ROLE).toUInt();
  auto result = Cheats::GenerateActionReplayCode(*m_session, index);
  if (result)
  {
    emit ActionReplayCodeGenerated(*result);
    m_info_label_1->setText(tr("Generated AR code."));
  }
  else
  {
    switch (result.Error())
    {
    case Cheats::GenerateActionReplayCodeErrorCode::NotVirtualMemory:
      m_info_label_1->setText(tr("Can only generate AR code for values in virtual memory."));
      break;
    case Cheats::GenerateActionReplayCodeErrorCode::InvalidAddress:
      m_info_label_1->setText(tr("Cannot generate AR code for this address."));
      break;
    default:
      m_info_label_1->setText(tr("Internal error while generating AR code."));
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

  const size_t result_count = m_session->GetResultCount();
  const bool too_many_results = result_count > TABLE_MAX_ROWS;
  const int result_count_to_display = int(too_many_results ? TABLE_MAX_ROWS : result_count);
  m_address_table->setRowCount(result_count_to_display);

  for (int i = 0; i < result_count_to_display; ++i)
  {
    const u32 address = m_session->GetResultAddress(i);
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

    const bool show_in_hex = m_display_values_in_hex_checkbox->isChecked();
    auto* last_value_item = new QTableWidgetItem();
    last_value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    last_value_item->setText(
        QString::fromStdString(m_session->GetResultValueAsString(i, show_in_hex)));
    last_value_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    last_value_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_LAST_VALUE, last_value_item);

    auto* current_value_item = new QTableWidgetItem();
    current_value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    const auto curr_val_it = m_address_table_current_values.find(address);
    if (curr_val_it != m_address_table_current_values.end())
      current_value_item->setText(QString::fromStdString(curr_val_it->second));
    else
      current_value_item->setText(QStringLiteral("---"));
    current_value_item->setData(ADDRESS_TABLE_ADDRESS_ROLE, address);
    current_value_item->setData(ADDRESS_TABLE_RESULT_INDEX_ROLE, static_cast<u32>(i));
    m_address_table->setItem(i, ADDRESS_TABLE_COLUMN_INDEX_CURRENT_VALUE, current_value_item);
  }
}
