// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/InstructionDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/GekkoDisassembler.h"

InstructionDialog::InstructionDialog(QWidget* parent, u32 addr, u32 instruction) : QDialog(parent)
{
  setWindowTitle(tr("Change instruction"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_instruction_addr = addr;
  m_new_instruction = instruction;

  QString instr_text = QStringLiteral("%1").arg(instruction, 8, 16, QLatin1Char('0'));

  QLabel* label = new QLabel(tr("New instruction:"));
  QLineEdit* instruction_edit = new QLineEdit(instr_text);
  m_instruction_desc_label = new QLabel;

  m_btn_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  connect(instruction_edit, &QLineEdit::textEdited, this,
          &InstructionDialog::UpdateInstructionText);

  connect(m_btn_box, &QDialogButtonBox::accepted, this, &InstructionDialog::accept);
  connect(m_btn_box, &QDialogButtonBox::rejected, this, &InstructionDialog::reject);

  QVBoxLayout* main_layout = new QVBoxLayout(this);

  main_layout->addWidget(label);
  main_layout->addWidget(instruction_edit);
  main_layout->addWidget(m_instruction_desc_label);
  main_layout->addWidget(m_btn_box);

  setLayout(main_layout);
  main_layout->setSizeConstraint(QLayout::SetFixedSize);

  UpdateInstructionText(instr_text);
}

int InstructionDialog::ShowModal(u32* new_instruction)
{
  int result = QDialog::exec();
  *new_instruction = m_new_instruction;
  return result;
}

void InstructionDialog::UpdateInstructionText(const QString& text)
{
  // Hex to UInt
  bool ok;
  u32 new_instr_candidate = text.toUInt(&ok, 16);

  if (!ok)
  {
    // Reset text
    m_instruction_desc_label->setText(tr("Invalid instruction!"));
    // Disable OK button
    m_btn_box->button(QDialogButtonBox::Ok)->setDisabled(true);
    return;
  }

  m_new_instruction = new_instr_candidate;

  // Disassemble
  QString instr_disasm = QString::fromStdString(Common::GekkoDisassembler::Disassemble(
                                                    m_new_instruction, m_instruction_addr))
                             .replace(QStringLiteral("\t"), QStringLiteral(" "));

  m_instruction_desc_label->setText(tr("Preview: ") + instr_disasm);

  // Enable OK button
  m_btn_box->button(QDialogButtonBox::Ok)->setEnabled(true);
}
