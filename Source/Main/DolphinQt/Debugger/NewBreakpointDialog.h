// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class BreakpointWidget;
class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QRadioButton;

class NewBreakpointDialog : public QDialog
{
  Q_OBJECT
public:
  explicit NewBreakpointDialog(BreakpointWidget* parent);

  void accept() override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnBPTypeChanged();
  void OnAddressTypeChanged();

  // Instruction BPs
  QRadioButton* m_instruction_bp;
  QGroupBox* m_instruction_box;
  QLineEdit* m_instruction_address;

  // Memory BPs
  QRadioButton* m_memory_bp;
  QRadioButton* m_memory_use_address;
  QRadioButton* m_memory_use_range;
  QGroupBox* m_memory_box;
  QLabel* m_memory_address_from_label;
  QLineEdit* m_memory_address_from;
  QLabel* m_memory_address_to_label;
  QLineEdit* m_memory_address_to;
  QRadioButton* m_memory_on_read;
  QRadioButton* m_memory_on_read_and_write;
  QRadioButton* m_memory_on_write;
  QRadioButton* m_memory_do_log;
  QRadioButton* m_memory_do_break;
  QRadioButton* m_memory_do_log_and_break;

  QDialogButtonBox* m_buttons;
  BreakpointWidget* m_parent;
};
