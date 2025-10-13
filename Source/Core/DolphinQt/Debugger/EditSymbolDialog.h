// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <string>

#include <QDialog>
#include <QString>
#include "Common/CommonTypes.h"

class QLineEdit;
class QDialogButtonBox;
class QSpinBox;

class EditSymbolDialog : public QDialog
{
  Q_OBJECT

public:
  enum class Type
  {
    Symbol,
    Note,
  };

  explicit EditSymbolDialog(QWidget* parent, const u32 symbol_address, u32* symbol_size,
                            std::string* symbol_name, Type type = Type::Symbol);

  bool DeleteRequested() const { return m_delete_chosen; }

private:
  void CreateWidgets();
  void ConnectWidgets();
  void FillFunctionData();
  void UpdateAddressData(u32 size);
  void Accepted();

  QLineEdit* m_name_edit;
  QSpinBox* m_size_lines_spin;
  QLineEdit* m_size_hex_edit;
  QLineEdit* m_address_end_edit;

  QDialogButtonBox* m_buttons;

  Type m_type;
  std::string* m_symbol_name;
  u32* m_symbol_size;
  const u32 m_symbol_address;

  bool m_delete_chosen = false;
};
