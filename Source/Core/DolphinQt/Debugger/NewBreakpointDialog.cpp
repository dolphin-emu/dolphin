// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/NewBreakpointDialog.h"

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
  setWindowTitle(tr("New Breakpoint"));
  CreateWidgets();
  ConnectWidgets();

  OnBPTypeChanged();
  OnAddressTypeChanged();
}

void NewBreakpointDialog::CreateWidgets()
{
  m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  // Instruction BP
  m_instruction_bp = new QRadioButton(tr("Instruction Breakpoint"));
  m_instruction_bp->setChecked(true);
  m_instruction_box = new QGroupBox;
  m_instruction_address = new QLineEdit;

  auto* instruction_layout = new QHBoxLayout;
  m_instruction_box->setLayout(instruction_layout);
  instruction_layout->addWidget(new QLabel(tr("Address:")));
  instruction_layout->addWidget(m_instruction_address);

  // Memory BP
  m_memory_bp = new QRadioButton(tr("Memory Breakpoint"));
  m_memory_box = new QGroupBox;
  m_memory_use_address = new QRadioButton(tr("Address"));
  m_memory_use_address->setChecked(true);
  // i18n: A range of memory addresses
  m_memory_use_range = new QRadioButton(tr("Range"));
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
  m_memory_do_log = new QRadioButton(tr("Write to Log"));
  // i18n: This is a selectable action when adding a breakpoint
  m_memory_do_break = new QRadioButton(tr("Break"));
  // i18n: This is a selectable action when adding a breakpoint
  m_memory_do_log_and_break = new QRadioButton(tr("Write to Log and Break"));
  m_memory_do_log_and_break->setChecked(true);

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
  auto* action_layout = new QHBoxLayout;
  action_box->setLayout(action_layout);
  memory_layout->addWidget(action_box, 3, 0, 1, -1);
  action_layout->addWidget(m_memory_do_log);
  action_layout->addWidget(m_memory_do_break);
  action_layout->addWidget(m_memory_do_log_and_break);

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_instruction_bp);
  layout->addWidget(m_instruction_box);
  layout->addWidget(m_memory_bp);
  layout->addWidget(m_memory_box);
  layout->addWidget(m_buttons);

  setLayout(layout);
}

void NewBreakpointDialog::ConnectWidgets()
{
  connect(m_buttons, &QDialogButtonBox::accepted, this, &NewBreakpointDialog::accept);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &NewBreakpointDialog::reject);

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
  bool do_log = m_memory_do_log->isChecked() || m_memory_do_log_and_break->isChecked();
  bool do_break = m_memory_do_break->isChecked() || m_memory_do_log_and_break->isChecked();

  bool good;

  if (instruction)
  {
    u32 address = m_instruction_address->text().toUInt(&good, 16);

    if (!good)
    {
      invalid_input(tr("Address"));
      return;
    }

    m_parent->AddBP(address);
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

      m_parent->AddRangedMBP(from, to, on_read, on_write, do_log, do_break);
    }
    else
    {
      m_parent->AddAddressMBP(from, on_read, on_write, do_log, do_break);
    }
  }

  QDialog::accept();
}
