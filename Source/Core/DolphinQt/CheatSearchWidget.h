// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/CheatSearch.h"
#include "VideoCommon/VideoEvents.h"

namespace ActionReplay
{
struct ARCode;
}
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

signals:
  void ActionReplayCodeGenerated(const ActionReplay::ARCode& ar_code);
  void RequestWatch(QString name, u32 address);
  void ShowMemory(const u32 address);

protected:
  bool event(QEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void showEvent(QShowEvent* event) override;

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
  void OnUpdateDisasmDialog();
  void OnAutoupdate(Cheats::CheatSearchSessionBase* session);
  void OnAutoupdateToggled(bool enabled);

  void AutoupdateAllCurrentValueRowsIfNeeded();
  Cheats::SearchErrorCode UpdateCurrentValueSessionAndTable();
  void UpdateCurrentValueSessionAddressesAndValues();

  void DisplaySharedSearchErrorMessage(Cheats::SearchErrorCode result);
  void UpdateTableLastAndCurrentValues();
  void UpdateTableCurrentValues();
  void RecreateTable();
  int GetTableRowCount() const;
  QString
  GetValueStringFromSessionIndex(const std::unique_ptr<Cheats::CheatSearchSessionBase>& session,
                                 int index) const;

  void GenerateARCodes();

  // Removes any existing callback, and adds an updated one if necessary.
  void RefreshAutoupdateCallback();
  void RemoveAutoupdateCallback();
  void RemovePendingAutoupdates();

  Core::System& m_system;

  // Stores the values used by the "last value" search filter and shown in the Last Value column.
  // Updated only after clicking the "Search and Filter" button or resetting the table.
  std::unique_ptr<Cheats::CheatSearchSessionBase> m_last_value_session;

  // Stores the values shown in the Current Value column. Updated when the user clicks the "Refresh
  // Current Values" button or when "Automatically update Current Values" is checked and an end of
  // frame event, pause, or breakpoint occurs. Overwritten by m_last_value_session (up to the
  // maximum table size) whenever that session updates.
  std::unique_ptr<Cheats::CheatSearchSessionBase> m_current_value_session;

  // storage for user-entered metadata such as text descriptions for memory addresses
  // this is intentionally NOT cleared when updating values or resetting or similar
  std::unordered_map<u32, CheatSearchTableUserData> m_address_table_user_data;

  Common::EventHook m_VI_end_field_event{};

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
