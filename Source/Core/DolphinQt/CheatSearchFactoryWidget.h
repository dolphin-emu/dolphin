// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "Core/CheatSearch.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

class CheatSearchFactoryWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CheatSearchFactoryWidget();
  ~CheatSearchFactoryWidget() override;

signals:
  void NewSessionCreated(const Cheats::CheatSearchSessionBase& session);

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
};
