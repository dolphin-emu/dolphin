// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include "Common/CommonTypes.h"

// class CodeWidget;
class QLineEdit;
class QDialogButtonBox;
class QSpinBox;

class EditSymbolDialog : public QDialog
{
  Q_OBJECT

public:
  explicit EditSymbolDialog(QWidget* parent, const u32 symbol_address, u32* symbol_size,
                            std::string* symbol_name);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void FillFunctionData();
  void UpdateAddressData(u32 size);
  void Accepted();
  void NotifyDelete();

  QLineEdit* m_name_edit;
  QSpinBox* m_size_lines_spin;
  QLineEdit* m_size_hex_edit;
  QLineEdit* m_address_end_edit;

  QPushButton* m_reset_button;
  QPushButton* m_delete_button;
  QDialogButtonBox* m_buttons;

  std::string* m_symbol_name;
  u32* m_symbol_size;
  const u32 m_symbol_address;
};
