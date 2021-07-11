// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/CheatSearch.h"

namespace ActionReplay
{
struct ARCode;
}

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

struct CheatSearchTableUserData
{
  std::string m_description;
};

class CheatSearchWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CheatSearchWidget(
      std::vector<Cheats::MemoryRange> memory_ranges, PowerPC::RequestedAddressSpace address_space,
      Cheats::DataType data_type, bool data_aligned,
      std::function<void(ActionReplay::ARCode ar_code)> generate_ar_code_callback);
  ~CheatSearchWidget() override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnNextScanClicked();
  void OnRefreshClicked();
  void OnResetClicked();
  void OnCloseClicked();
  void OnAddressTableItemChanged(QTableWidgetItem* item);
  void OnAddressTableContextMenu();
  void OnValueSourceChanged();

  void ApplySearchResults(
      Common::Result<Cheats::SearchErrorCode, std::vector<Cheats::SearchResult>> results);
  void UpdateGuiTable();
  void GenerateARCode();

  std::vector<Cheats::MemoryRange> m_memory_ranges;
  PowerPC::RequestedAddressSpace m_address_space;
  Cheats::DataType m_data_type;
  bool m_data_aligned;
  std::optional<std::vector<Cheats::SearchResult>> m_results = std::nullopt;

  // storage for the 'Current Value' column's data
  std::unordered_map<u32, Cheats::SearchResult> m_address_table_current_values;

  // storage for user-entered metadata such as text descriptions for memory addresses
  // this is intentionally NOT cleared when updating values or resetting or similar
  std::unordered_map<u32, CheatSearchTableUserData> m_address_table_user_data;

  std::function<void(ActionReplay::ARCode ar_code)> m_generate_ar_code_callback;

  QComboBox* m_compare_type_dropdown;
  QComboBox* m_value_source_dropdown;
  QLineEdit* m_given_value_text;
  QLabel* m_info_label;
  QPushButton* m_next_scan_button;
  QPushButton* m_refresh_values_button;
  QPushButton* m_reset_button;
  QPushButton* m_close_button;
  QTableWidget* m_address_table;
};
