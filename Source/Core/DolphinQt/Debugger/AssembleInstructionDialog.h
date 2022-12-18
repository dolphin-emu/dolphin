// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class QDialogButtonBox;
class QLabel;
class QLineEdit;

class AssembleInstructionDialog : public QDialog
{
  Q_OBJECT
public:
  explicit AssembleInstructionDialog(QWidget* parent, u32 address, u32 value);

  u32 GetCode() const;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnEditChanged();

  u32 m_code;
  u32 m_address;

  QLineEdit* m_input_edit;
  QLabel* m_error_loc_label;
  QLabel* m_error_line_label;
  QLabel* m_msg_label;
  QDialogButtonBox* m_button_box;
};
