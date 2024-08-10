// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BreakpointDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>

#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/Expression.h"
#include "DolphinQt/Debugger/BreakpointWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

BreakpointDialog::BreakpointDialog(BreakpointWidget* parent)
    : QDialog(parent), m_parent(parent), m_open_mode(OpenMode::New)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("New Breakpoint"));
  CreateWidgets();
  ConnectWidgets();

  OnBPTypeChanged();
  OnAddressTypeChanged();
}

BreakpointDialog::BreakpointDialog(BreakpointWidget* parent, const TBreakPoint* breakpoint)
    : QDialog(parent), m_parent(parent), m_open_mode(OpenMode::EditBreakPoint)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("Edit Breakpoint"));
  CreateWidgets();
  ConnectWidgets();

  m_instruction_address->setText(QString::number(breakpoint->address, 16));
  if (breakpoint->condition)
    m_conditional->setText(QString::fromStdString(breakpoint->condition->GetText()));

  m_do_break->setChecked(breakpoint->break_on_hit && !breakpoint->log_on_hit);
  m_do_log->setChecked(!breakpoint->break_on_hit && breakpoint->log_on_hit);
  m_do_log_and_break->setChecked(breakpoint->break_on_hit && breakpoint->log_on_hit);

  OnBPTypeChanged();
  OnAddressTypeChanged();
}

BreakpointDialog::BreakpointDialog(BreakpointWidget* parent, const TMemCheck* memcheck)
    : QDialog(parent), m_parent(parent), m_open_mode(OpenMode::EditMemCheck)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("Edit Breakpoint"));

  CreateWidgets();
  ConnectWidgets();

  m_memory_address_from->setText(QString::number(memcheck->start_address, 16));
  if (memcheck->is_ranged)
  {
    m_memory_use_address->setChecked(false);
    m_memory_use_range->setChecked(true);
    m_memory_address_to->setText(QString::number(memcheck->end_address, 16));
  }
  else
  {
    m_memory_address_to->setText(QString::number(memcheck->start_address + 1, 16));
  }
  if (memcheck->condition)
    m_conditional->setText(QString::fromStdString(memcheck->condition->GetText()));

  m_memory_on_read->setChecked(memcheck->is_break_on_read && !memcheck->is_break_on_write);
  m_memory_on_write->setChecked(!memcheck->is_break_on_read && memcheck->is_break_on_write);
  m_memory_on_read_and_write->setChecked(memcheck->is_break_on_read && memcheck->is_break_on_write);

  m_do_break->setChecked(memcheck->break_on_hit && !memcheck->log_on_hit);
  m_do_log->setChecked(!memcheck->break_on_hit && memcheck->log_on_hit);
  m_do_log_and_break->setChecked(memcheck->break_on_hit && memcheck->log_on_hit);

  OnBPTypeChanged();
  OnAddressTypeChanged();
}

void BreakpointDialog::CreateWidgets()
{
  m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                   QDialogButtonBox::Help);
  auto* type_group = new QButtonGroup(this);

  // Instruction BP
  auto* instruction_widget = new QWidget;
  auto* instruction_layout = new QGridLayout;

  m_instruction_bp = new QRadioButton(tr("Instruction Breakpoint"));
  type_group->addButton(m_instruction_bp);
  m_instruction_box = new QGroupBox;
  m_instruction_address = new QLineEdit;

  auto* instruction_data_layout = new QHBoxLayout;
  m_instruction_box->setLayout(instruction_data_layout);
  instruction_data_layout->addWidget(new QLabel(tr("Address:")));
  instruction_data_layout->addWidget(m_instruction_address);

  instruction_layout->addWidget(m_instruction_bp, 0, 0, 1, 1);
  instruction_layout->addWidget(m_instruction_box, 1, 0, 1, 2);
  instruction_widget->setLayout(instruction_layout);

  // Memory BP
  auto* memory_widget = new QWidget;
  auto* memory_layout = new QGridLayout;

  m_memory_bp = new QRadioButton(tr("Memory Breakpoint"));
  type_group->addButton(m_memory_bp);
  m_memory_box = new QGroupBox;
  auto* memory_type_group = new QButtonGroup(this);
  m_memory_use_address = new QRadioButton(tr("Address"));
  m_memory_use_address->setChecked(true);
  memory_type_group->addButton(m_memory_use_address);
  // i18n: A range of memory addresses
  m_memory_use_range = new QRadioButton(tr("Range"));
  memory_type_group->addButton(m_memory_use_range);
  m_memory_address_from = new QLineEdit;
  m_memory_address_to = new QLineEdit;
  m_memory_address_from_label = new QLabel;  // Set by OnAddressTypeChanged
  m_memory_address_to_label = new QLabel(tr("To:"));
  // i18n: This is a selectable condition when adding a breakpoint
  m_memory_on_read = new QRadioButton(tr("Read"));
  // i18n: This is a selectable condition when adding a breakpoint
  m_memory_on_write = new QRadioButton(tr("Write"));
  // i18n: This is a selectable condition when adding a breakpoint
  m_memory_on_read_and_write = new QRadioButton(tr("Read or Write"));
  m_memory_on_write->setChecked(true);
  // i18n: This is a selectable action when adding a breakpoint
  m_do_log = new QRadioButton(tr("Write to Log"));
  // i18n: This is a selectable action when adding a breakpoint
  m_do_break = new QRadioButton(tr("Break"));
  // i18n: This is a selectable action when adding a breakpoint
  m_do_log_and_break = new QRadioButton(tr("Write to Log and Break"));
  m_do_log_and_break->setChecked(true);

  auto* memory_data_layout = new QGridLayout;
  m_memory_box->setLayout(memory_data_layout);
  memory_data_layout->addWidget(m_memory_use_address, 0, 0);
  memory_data_layout->addWidget(m_memory_use_range, 0, 3);
  memory_data_layout->addWidget(m_memory_address_from_label, 1, 0);
  memory_data_layout->addWidget(m_memory_address_from, 1, 1);
  memory_data_layout->addWidget(m_memory_address_to_label, 1, 2);
  memory_data_layout->addWidget(m_memory_address_to, 1, 3);

  // i18n: If a condition is set for a breakpoint, the condition becoming true is a prerequisite for
  // triggering the breakpoint.
  QGroupBox* condition_box = new QGroupBox(tr("Condition"));
  auto* condition_layout = new QHBoxLayout;
  condition_box->setLayout(condition_layout);

  memory_data_layout->addWidget(condition_box, 2, 0, 1, -1);
  condition_layout->addWidget(m_memory_on_read);
  condition_layout->addWidget(m_memory_on_write);
  condition_layout->addWidget(m_memory_on_read_and_write);

  memory_layout->addWidget(m_memory_bp, 0, 0);
  memory_layout->addWidget(m_memory_box, 1, 0);
  memory_widget->setLayout(memory_layout);

  QGroupBox* action_box = new QGroupBox(tr("Action"));

  QHBoxLayout* conditional_layout = new QHBoxLayout;
  m_conditional = new QLineEdit();
  // i18n: If a condition is set for a breakpoint, the condition becoming true is a prerequisite for
  // triggering the breakpoint.
  conditional_layout->addWidget(new QLabel(tr("Condition:")));
  conditional_layout->addWidget(m_conditional);

  auto* action_layout = new QHBoxLayout;
  action_layout->addWidget(m_do_log);
  action_layout->addWidget(m_do_break);
  action_layout->addWidget(m_do_log_and_break);

  auto* action_vlayout = new QVBoxLayout;
  action_vlayout->addLayout(conditional_layout);
  action_vlayout->addLayout(action_layout);

  action_box->setLayout(action_vlayout);

  auto* layout = new QVBoxLayout;
  layout->addWidget(instruction_widget);
  layout->addWidget(memory_widget);
  layout->addWidget(action_box);
  layout->addWidget(m_buttons);

  switch (m_open_mode)
  {
  case OpenMode::New:
    m_instruction_bp->setChecked(true);
    m_instruction_address->setFocus();
    break;
  case OpenMode::EditBreakPoint:
    memory_widget->setVisible(false);
    m_instruction_bp->setChecked(true);
    m_instruction_address->setEnabled(false);
    m_instruction_address->setFocus();
    break;
  case OpenMode::EditMemCheck:
    instruction_widget->setVisible(false);
    m_memory_bp->setChecked(true);
    m_memory_address_from->setEnabled(false);
    m_memory_address_to->setFocus();
    break;
  }

  setLayout(layout);
}

void BreakpointDialog::ConnectWidgets()
{
  connect(m_buttons, &QDialogButtonBox::accepted, this, &BreakpointDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &BreakpointDialog::reject);
  connect(m_buttons, &QDialogButtonBox::helpRequested, this, &BreakpointDialog::ShowConditionHelp);

  connect(m_instruction_bp, &QRadioButton::toggled, this, &BreakpointDialog::OnBPTypeChanged);
  connect(m_memory_bp, &QRadioButton::toggled, this, &BreakpointDialog::OnBPTypeChanged);

  connect(m_memory_use_address, &QRadioButton::toggled, this,
          &BreakpointDialog::OnAddressTypeChanged);
  connect(m_memory_use_range, &QRadioButton::toggled, this,
          &BreakpointDialog::OnAddressTypeChanged);
}

void BreakpointDialog::OnBPTypeChanged()
{
  m_instruction_box->setEnabled(m_instruction_bp->isChecked());
  m_memory_box->setEnabled(m_memory_bp->isChecked());
}

void BreakpointDialog::OnAddressTypeChanged()
{
  bool ranged = m_memory_use_range->isChecked();

  m_memory_address_to->setHidden(!ranged);
  m_memory_address_to_label->setHidden(!ranged);

  m_memory_address_from_label->setText(ranged ? tr("From:") : tr("Address:"));
}

void BreakpointDialog::accept()
{
  auto invalid_input = [this](const QString& field) {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Invalid input for the field \"%1\"").arg(field));
  };

  bool instruction = m_instruction_bp->isChecked();
  bool ranged = m_memory_use_range->isChecked();

  // Triggers
  bool on_read = m_memory_on_read->isChecked() || m_memory_on_read_and_write->isChecked();
  bool on_write = m_memory_on_write->isChecked() || m_memory_on_read_and_write->isChecked();

  // Actions
  bool do_log = m_do_log->isChecked() || m_do_log_and_break->isChecked();
  bool do_break = m_do_break->isChecked() || m_do_log_and_break->isChecked();

  bool good;

  const QString condition = m_conditional->text().trimmed();

  if (!condition.isEmpty() && !Expression::TryParse(condition.toUtf8().constData()))
  {
    // i18n: If a condition is set for a breakpoint, the condition becoming true is a prerequisite
    // for triggering the breakpoint.
    invalid_input(tr("Condition"));
    return;
  }

  if (instruction)
  {
    u32 address = m_instruction_address->text().toUInt(&good, 16);

    if (!good)
    {
      invalid_input(tr("Address"));
      return;
    }

    m_parent->AddBP(address, do_break, do_log, condition);
  }
  else
  {
    u32 from = m_memory_address_from->text().toUInt(&good, 16);

    if (!good)
    {
      invalid_input(ranged ? tr("From") : tr("Address"));
      return;
    }

    if (ranged)
    {
      u32 to = m_memory_address_to->text().toUInt(&good, 16);
      if (!good)
      {
        invalid_input(tr("To"));
        return;
      }

      m_parent->AddRangedMBP(from, to, on_read, on_write, do_log, do_break, condition);
    }
    else
    {
      m_parent->AddAddressMBP(from, on_read, on_write, do_log, do_break, condition);
    }
  }

  QDialog::accept();
}

void BreakpointDialog::ShowConditionHelp()
{
  const auto message = tr(
      "Conditions:\n"
      "Sets an expression that is evaluated when a breakpoint is hit. If the expression is false "
      "or 0, the breakpoint is ignored until hit again. Statements should be separated by a comma. "
      "Only the last statement will be used to determine what to do.\n"
      "\n"
      "Registers that can be referenced:\n"
      "GPRs : r0..r31\n"
      "FPRs : f0..f31\n"
      "SPRs : xer, lr, ctr, dsisr, dar, dec, sdr1, srr0, srr1, tbl, tbu, pvr, sprg0..sprg3, ear, "
      "ibat0u..ibat7u, ibat0l..ibat7l, dbat0u..dbat7u, dbat0l..dbat07, gqr0..gqr7, hid0, hid1, "
      "hid2, hid4, iabr, dabr, wpar, dmau, dmal, ecid_u, ecid_m, ecid_l, upmc1..upmc4, usia, sia, "
      "l2cr, ictc, mmcr0, mmcr1, pmc1..pmc4, thrm1..thrm3\n"
      "Other : pc, msr\n"
      "\n"
      "Functions:\n"
      "Set a register: r1 = 8\n"
      "Casts: s8(0xff). Available: s8, u8, s16, u16, s32, u32\n"
      "Callstack: callstack(0x80123456), callstack(\"anim\")\n"
      "Compare Strings: streq(r3, \"abc\"). Both parameters can be addresses or string constants.\n"
      "Read Memory: read_u32(0x80000000). Available: u8, s8, u16, s16, u32, s32, f32, f64\n"
      "Write Memory: write_u32(r3, 0x80000000). Available: u8, u16, u32, f32, f64\n"
      "*currently writing will always be triggered\n"
      "\n"
      "Operations:\n"
      "Unary: -u, !u, ~u\n"
      "Math: *  / + -, power: **, remainder: %, shift: <<, >>\n"
      "Compare: <, <=, >, >=, ==, !=, &&, ||\n"
      "Bitwise: &, |, ^\n"
      "\n"
      "Examples:\n"
      "r4 == 1\n"
      "f0 == 1.0 && f2 < 10.0\n"
      "r26 <= r0 && ((r5 + 3) & -4) * ((r6 + 3) & -4)* 4 > r0\n"
      "p = r3 + 0x8, p == 0x8003510 && read_u32(p) != 0\n"
      "Write and break: r4 = 8, 1\n"
      "Write and continue: f3 = f1 + f2, 0\n"
      "The condition must always be last\n\n"
      "Strings should only be used in callstack() or streq() and \"quoted\". Do not assign strings "
      "to a variable.\n"
      "All variables will be printed in the Memory Interface log, if there's a hit or a NaN "
      "result. To check for issues, assign a variable to your equation, so it can be printed.\n\n"
      "Note: All values are internally converted to Doubles for calculations. It's possible for "
      "them to go out of range or to become NaN. A warning will be given if NaN is returned, and "
      "the var that became NaN will be logged.");
  // i18n: The title for a dialog that shows help for how to use conditions. If a condition is set
  // for a breakpoint, the condition becoming true is a prerequisite for triggering the breakpoint.
  ModalMessageBox::information(this, tr("Conditional help"), message);
}
