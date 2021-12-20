// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/NewBreakpointDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

#include "DolphinQt/Debugger/BreakpointWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

NewBreakpointDialog::NewBreakpointDialog(BreakpointWidget* parent)
    : QDialog(parent), m_parent(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("New Breakpoint"));
  CreateWidgets();
  ConnectWidgets();

  OnBPTypeChanged();
  OnAddressTypeChanged();
}

void NewBreakpointDialog::CreateWidgets()
{
  m_buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Ok |
                                   QDialogButtonBox::Cancel);
  auto* type_group = new QButtonGroup(this);

  // Instruction BP
  m_instruction_bp = new QRadioButton(tr("Instruction Breakpoint"));
  m_instruction_bp->setChecked(true);
  type_group->addButton(m_instruction_bp);
  m_instruction_box = new QGroupBox;
  m_instruction_address = new QLineEdit;

  auto* instruction_layout = new QHBoxLayout;
  m_instruction_box->setLayout(instruction_layout);
  instruction_layout->addWidget(new QLabel(tr("Address:")));
  instruction_layout->addWidget(m_instruction_address);

  // Memory BP
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
  m_log_message_label = new QLabel(tr("Log message:"));
  m_log_message = new QLineEdit;

  auto* memory_layout = new QGridLayout;
  m_memory_box->setLayout(memory_layout);
  memory_layout->addWidget(m_memory_use_address, 0, 0);
  memory_layout->addWidget(m_memory_use_range, 0, 3);
  memory_layout->addWidget(m_memory_address_from_label, 1, 0);
  memory_layout->addWidget(m_memory_address_from, 1, 1);
  memory_layout->addWidget(m_memory_address_to_label, 1, 2);
  memory_layout->addWidget(m_memory_address_to, 1, 3);
  QGroupBox* condition_box = new QGroupBox(tr("Condition"));
  auto* condition_layout = new QHBoxLayout;
  condition_box->setLayout(condition_layout);

  memory_layout->addWidget(condition_box, 2, 0, 1, -1);
  condition_layout->addWidget(m_memory_on_read);
  condition_layout->addWidget(m_memory_on_write);
  condition_layout->addWidget(m_memory_on_read_and_write);

  QGroupBox* action_box = new QGroupBox(tr("Action"));
  auto* action_layout = new QGridLayout;
  action_box->setLayout(action_layout);
  action_layout->addWidget(m_do_log, 0, 0);
  action_layout->addWidget(m_do_break, 0, 1);
  action_layout->addWidget(m_do_log_and_break, 0, 2);
  action_layout->addWidget(m_log_message_label, 1, 0);
  action_layout->addWidget(m_log_message, 1, 1, 1, 2);

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_instruction_bp);
  layout->addWidget(m_instruction_box);
  layout->addWidget(m_memory_bp);
  layout->addWidget(m_memory_box);
  layout->addWidget(action_box);
  layout->addWidget(m_buttons);

  setLayout(layout);

  m_instruction_address->setFocus();
}

void NewBreakpointDialog::ConnectWidgets()
{
  connect(m_buttons, &QDialogButtonBox::accepted, this, &NewBreakpointDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &NewBreakpointDialog::reject);
  connect(m_buttons, &QDialogButtonBox::helpRequested, this, &NewBreakpointDialog::ShowHelp);

  connect(m_instruction_bp, &QRadioButton::toggled, this, &NewBreakpointDialog::OnBPTypeChanged);
  connect(m_memory_bp, &QRadioButton::toggled, this, &NewBreakpointDialog::OnBPTypeChanged);

  connect(m_memory_use_address, &QRadioButton::toggled, this,
          &NewBreakpointDialog::OnAddressTypeChanged);
  connect(m_memory_use_range, &QRadioButton::toggled, this,
          &NewBreakpointDialog::OnAddressTypeChanged);
}

void NewBreakpointDialog::OnBPTypeChanged()
{
  m_instruction_box->setEnabled(m_instruction_bp->isChecked());
  m_memory_box->setEnabled(m_memory_bp->isChecked());
}

void NewBreakpointDialog::OnAddressTypeChanged()
{
  bool ranged = m_memory_use_range->isChecked();

  m_memory_address_to->setHidden(!ranged);
  m_memory_address_to_label->setHidden(!ranged);

  m_memory_address_from_label->setText(ranged ? tr("From:") : tr("Address:"));
}

void NewBreakpointDialog::accept()
{
  auto invalid_input = [this](QString field) {
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
  std::string message = m_log_message->text().toStdString();

  bool good;

  if (instruction)
  {
    u32 address = m_instruction_address->text().toUInt(&good, 16);

    if (!good)
    {
      invalid_input(tr("Address"));
      return;
    }

    m_parent->AddBP(address, false, do_break, do_log, std::move(message));
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

      m_parent->AddRangedMBP(from, to, on_read, on_write, do_log, do_break, std::move(message));
    }
    else
    {
      m_parent->AddAddressMBP(from, on_read, on_write, do_log, do_break, std::move(message));
    }
  }

  QDialog::accept();
}

void NewBreakpointDialog::ShowHelp()
{
  const auto message =
      QStringLiteral("This dialog allows to set a breakpoint (alias BP). The execution of the "
                     "game will pause when a BP is reached. There are two kinds of BP:\n"
                     " - Instruction BP: will pause when the code at the address is executed\n"
                     " - Memory BP: will pause when the memory address is read/written\n"
                     "\n"
                     "On top of pausing the execution you can also use log messages when the "
                     "BP is hit. Alternatively, you can use them to only log messages and not "
                     "break at all. When used that way, they are often called \"Tracepoint\" "
                     "or \"Log Breakpoint\".\n"
                     "\n"
                     "Using braces, you can include register's state in the log message. The "
                     "format used follows the same principle as the libfmt format {fmt}. All "
                     "its type specifiers are supported. Printf's length specifier are also "
                     "available to cast the value to its proper types: hh, h, l. Other type "
                     "specifiers can also be used: u (unsigned cast), v (format string) and "
                     "V (format string with va_list).\n"
                     "\n"
                     "Registers available:\n"
                     " - GPRs: r0 .. r31\n"
                     " - FPRs: f0 .. f31, (lower) f0_1 .. f31_1, (upper) f0_2 .. f31_2\n"
                     " - Others: TB, PC, LR, CTR, CR, XER, FPSCR, MSR, SRR0, SRR1\n"
                     "Exceptions, IntMask, IntCause, DSISR, DAR, HashMask\n"
                     "\n"
                     "Examples:\n"
                     " -> A simple debug message\n"
                     " -> R3={r3}, F1={f1}\n"
                     " -> puts({r3:s})\n"
                     " -> printf({r3:v})\n"
                     " -> vprintf({r3:V})\n"
                     " -> GPR: Hex={r3:08x}, Oct={r3:o}, Bin={r3:b}, Float={r3:f}\n"
                     " -> Paired Single: {f1} = ({f1_1:016lx},{f1_2:016lx})\n"
                     " -> Double: {f1_1:lf}, {f1_2:lf}\n"
                     " -> Float: {f1_1:f}, {f1_2:f}\n");
  ModalMessageBox::information(this, tr("Breakpoint dialog help"), message);
}
