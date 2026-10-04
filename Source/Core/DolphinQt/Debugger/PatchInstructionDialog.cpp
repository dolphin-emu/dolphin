// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/PatchInstructionDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/Assembler/GekkoAssembler.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Swap.h"

PatchInstructionDialog::PatchInstructionDialog(QWidget* parent, u32 address, u32 value)
    : QDialog(parent), m_address(address)
{
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
  const QString input = m_input_edit->text();

  bool legal = false;
  bool is_hex = false;

  QString preview = QStringLiteral("(ill)\t---");

  const u32 hex_code = input.toUInt(&is_hex, 16);

  if (is_hex)
  {
    legal = true;
    m_code = hex_code;
    preview = QString::fromStdString(Common::GekkoDisassembler::Disassemble(hex_code, m_address));
  }
  else  // Try to parse as a single instruction
  {
    const auto asm_result = Common::GekkoAssembler::Assemble(input.toStdString(), m_address);

    if (!Common::GekkoAssembler::IsFailure(asm_result))
    {
      const auto& blocks = Common::GekkoAssembler::GetT(asm_result);

      if (!blocks.empty())
      {
        legal = true;

        m_code = Common::swap32(blocks[0].instructions.data());

        preview = QStringLiteral("%1").arg(m_code, 8, 16, QLatin1Char('0'));
      }
    }
  }

  m_button_box->button(QDialogButtonBox::Ok)->setEnabled(legal);

  m_preview_label->setText(tr("Instruction: %1").arg(preview));
}

u32 PatchInstructionDialog::GetCode() const
{
  return m_code;
}
