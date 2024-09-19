// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QDockWidget>

#include "Common/CommonTypes.h"

class MemoryViewWidget;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QShowEvent;
class QSplitter;

namespace Core
{
class System;
class CPUThreadGuard;
}  // namespace Core

class MemoryWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit MemoryWidget(Core::System& system, QWidget* parent = nullptr);
  ~MemoryWidget();

  void SetAddress(u32 address);
  void Update();
signals:
  void BreakpointsChanged();
  void ShowCode(u32 address);
  void RequestWatch(QString name, u32 address);

private:
  struct TargetAddress
  {
    u32 address = 0;
    bool is_good_address = false;
    bool is_good_offset = false;
  };

  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnAddressSpaceChanged();
  void OnDisplayChanged();
  void OnBPLogChanged();
  void OnBPTypeChanged();

  void OnSearchAddress();
  void OnFindNextValue();
  void OnFindPreviousValue();

  void OnSetValue();
  void OnSetValueFromFile();

  void OnDumpMRAM();
  void OnDumpExRAM();
  void OnDumpARAM();
  void OnDumpFakeVMEM();

  void ValidateAndPreviewInputValue();
  QByteArray GetInputData() const;
  TargetAddress GetTargetAddress() const;
  void FindValue(bool next);

  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

  Core::System& m_system;

  MemoryViewWidget* m_memory_view;
  QSplitter* m_splitter;
  QComboBox* m_search_address;
  QLineEdit* m_search_offset;
  QLineEdit* m_data_edit;
  QCheckBox* m_base_check;
  QLabel* m_data_preview;
  QComboBox* m_display_combo;
  QComboBox* m_align_combo;
  QComboBox* m_row_length_combo;
  QCheckBox* m_dual_check;
  QPushButton* m_set_value;

  // Search
  QPushButton* m_find_next;
  QPushButton* m_find_previous;
  QComboBox* m_input_combo;
  QLabel* m_result_label;

  // Address Spaces
  QRadioButton* m_address_space_physical;
  QRadioButton* m_address_space_effective;
  QRadioButton* m_address_space_auxiliary;

  // Breakpoint options
  QRadioButton* m_bp_read_write;
  QRadioButton* m_bp_read_only;
  QRadioButton* m_bp_write_only;
  QCheckBox* m_bp_log_check;
};
