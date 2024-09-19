// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"

class BreakpointWidget;
class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QPushButton;

class BreakpointDialog : public QDialog
{
  Q_OBJECT
public:
  enum class OpenMode
  {
    New,
    EditBreakPoint,
    EditMemCheck
  };

  explicit BreakpointDialog(BreakpointWidget* parent);
  BreakpointDialog(BreakpointWidget* parent, const TBreakPoint* breakpoint);
  BreakpointDialog(BreakpointWidget* parent, const TMemCheck* memcheck);

  void accept() override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void OnBPTypeChanged();
  void OnAddressTypeChanged();
  void ShowConditionHelp();

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

  // Action
  QRadioButton* m_do_log;
  QRadioButton* m_do_break;
  QRadioButton* m_do_log_and_break;

  QLineEdit* m_conditional;
  QPushButton* m_cond_help_btn;

  QDialogButtonBox* m_buttons;
  BreakpointWidget* m_parent;

  OpenMode m_open_mode;
};
