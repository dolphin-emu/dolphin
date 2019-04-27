// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/PatchInstructionDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/GekkoDisassembler.h"

PatchInstructionDialog::PatchInstructionDialog(QWidget* parent, u32 address, u32 value)
    : QDialog(parent), m_address(address)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::WindowModal);
  setWindowTitle(tr("Instruction"));

  CreateWidgets();
  ConnectWidgets();

  m_input_edit->setText(QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0')));
}

void PatchInstructionDialog::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  m_input_edit = new QLineEdit;
  m_preview_label = new QLabel;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  layout->addWidget(new QLabel(tr("New instruction:")));
  layout->addWidget(m_input_edit);
  layout->addWidget(m_preview_label);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void PatchInstructionDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_input_edit, &QLineEdit::textChanged, this, &PatchInstructionDialog::OnEditChanged);
}

void PatchInstructionDialog::OnEditChanged()
{
  bool good;
  m_code = m_input_edit->text().toUInt(&good, 16);

  m_button_box->button(QDialogButtonBox::Ok)->setEnabled(good);

  m_preview_label->setText(
      tr("Instruction: %1")
          .arg(QString::fromStdString(Common::GekkoDisassembler::Disassemble(m_code, m_address))));
}

u32 PatchInstructionDialog::GetCode() const
{
  return m_code;
}
