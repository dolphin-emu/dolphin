// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QWidget>

#include "Core/CheatSearch.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

class CheatSearchNewWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CheatSearchNewWidget(std::function<void(std::vector<Cheats::MemoryRange> memory_ranges,
                                                   PowerPC::RequestedAddressSpace address_space,
                                                   Cheats::DataType data_type, bool data_aligned)>
                                    new_search_started_callback);
  ~CheatSearchNewWidget() override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void RefreshGui();

  void OnAddressSpaceRadioChanged();
  void OnNewSearchClicked();

  QRadioButton* m_standard_address_space;
  QRadioButton* m_custom_address_space;

  QRadioButton* m_custom_virtual_address_space;
  QRadioButton* m_custom_physical_address_space;
  QRadioButton* m_custom_effective_address_space;

  QLineEdit* m_custom_address_start;
  QLineEdit* m_custom_address_end;

  QComboBox* m_data_type_dropdown;
  QCheckBox* m_data_type_aligned;

  QPushButton* m_new_search;

  std::function<void(std::vector<Cheats::MemoryRange> memory_ranges,
                     PowerPC::RequestedAddressSpace address_space, Cheats::DataType data_type,
                     bool data_aligned)>
      m_new_search_started_callback;
};
