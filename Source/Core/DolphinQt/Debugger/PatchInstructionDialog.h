// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class QDialogButtonBox;
class QLabel;
class QLineEdit;

class PatchInstructionDialog : public QDialog
{
  Q_OBJECT
public:
  explicit PatchInstructionDialog(QWidget* parent, u32 address, u32 value);

  u32 GetCode() const;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnEditChanged();

  u32 m_code;
  u32 m_address;

  QLineEdit* m_input_edit;
  QLabel* m_preview_label;
  QDialogButtonBox* m_button_box;
};
