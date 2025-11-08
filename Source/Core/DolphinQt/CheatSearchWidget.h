// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Core/ActionReplay.h"
#include "Core/CheatSearch.h"

namespace Core
{
class System;
}

class QCheckBox;
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
  explicit CheatSearchWidget(Core::System& system,
                             std::unique_ptr<Cheats::CheatSearchSessionBase> session,
                             QWidget* parent = nullptr);
  ~CheatSearchWidget() override;

  enum class UpdateSource
  {
    User,
    Auto,
  };

  bool UpdateTableAllCurrentValues(UpdateSource source);
  void UpdateTableVisibleCurrentValues(UpdateSource source);

signals:
  void ActionReplayCodeGenerated(const ActionReplay::ARCode& ar_code);
  void RequestWatch(QString name, u32 address);
  void ShowMemory(u32 address);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnNextScanClicked();
  void OnRefreshClicked();
  void OnResetClicked();
  void OnAddressTableItemChanged(QTableWidgetItem* item);
  void OnAddressTableContextMenu();
  void OnValueSourceChanged();
  void OnDisplayHexCheckboxStateChanged();

  void RefreshCurrentValueTableItem(QTableWidgetItem* current_value_table_item);
  void RefreshGUICurrentValues(size_t begin_index, size_t end_index);
  bool UpdateTableRows(const Core::CPUThreadGuard& guard, size_t begin_index, size_t end_index,
                       UpdateSource source);
  void RecreateGUITable();
  void GenerateARCodes();
  void WriteValue();
  int GetVisibleRowsBeginIndex() const;
  int GetVisibleRowsEndIndex() const;
  size_t GetTableRowCount() const;

  Core::System& m_system;

  // Stores the values used by the "last value" search filter and shown in the Last Value column.
  // Updated only after clicking the "Search and Filter" button or resetting the table.
  std::unique_ptr<Cheats::CheatSearchSessionBase> m_last_value_session;

  // storage for the 'Current Value' column's data
  std::unordered_map<u32, std::string> m_address_table_current_values;

  // storage for user-entered metadata such as text descriptions for memory addresses
  // this is intentionally NOT cleared when updating values or resetting or similar
  std::unordered_map<u32, CheatSearchTableUserData> m_address_table_user_data;

  QComboBox* m_compare_type_dropdown;
  QComboBox* m_value_source_dropdown;
  QLineEdit* m_given_value_text;
  QLabel* m_info_label_1;
  QLabel* m_info_label_2;
  QPushButton* m_next_scan_button;
  QPushButton* m_refresh_values_button;
  QPushButton* m_reset_button;
  QCheckBox* m_parse_values_as_hex_checkbox;
  QCheckBox* m_display_values_in_hex_checkbox;
  QCheckBox* m_autoupdate_current_values;
  QTableWidget* m_address_table;
};
