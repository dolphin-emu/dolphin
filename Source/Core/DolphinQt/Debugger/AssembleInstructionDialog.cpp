// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/AssembleInstructionDialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/Assembler/GekkoAssembler.h"
#include "Common/StringUtil.h"

namespace
{
QString HtmlFormatErrorLoc(const Common::GekkoAssembler::AssemblerError& err)
{
  return QObject::tr("<span style=\"color: red; font-weight: bold\">Error</span> on line %1 col %2")
      .arg(err.line + 1)
      .arg(err.col + 1);
}

QString HtmlFormatErrorLine(const Common::GekkoAssembler::AssemblerError& err)
{
  const QString line_pre_error =
      QString::fromStdString(std::string(err.error_line.substr(0, err.col))).toHtmlEscaped();
  const QString line_error =
      QString::fromStdString(std::string(err.error_line.substr(err.col, err.len))).toHtmlEscaped();
  const QString line_post_error =
      QString::fromStdString(std::string(err.error_line.substr(err.col + err.len))).toHtmlEscaped();

  return QStringLiteral("%1<u><span style=\"color:red; font-weight:bold\">%2</span></u>%3")
      .arg(line_pre_error)
      .arg(line_error)
      .arg(line_post_error);
}
}  // namespace

AssembleInstructionDialog::AssembleInstructionDialog(QWidget* parent, u32 address, u32 value)
    : QDialog(parent), m_code(value), m_address(address)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::WindowModal);
  setWindowTitle(tr("Instruction"));

  CreateWidgets();
  ConnectWidgets();
}

void AssembleInstructionDialog::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  m_input_edit = new QLineEdit;
  m_error_loc_label = new QLabel;
  m_error_line_label = new QLabel;
  m_msg_label = new QLabel(tr("No input"));
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  m_error_line_label->setFont(QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family()));
  m_input_edit->setFont(QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family()));
  layout->addWidget(m_error_loc_label);
  layout->addWidget(m_input_edit);
  layout->addWidget(m_error_line_label);
  layout->addWidget(m_msg_label);
  layout->addWidget(m_button_box);
  m_input_edit->setText(QStringLiteral(".4byte 0x%1").arg(m_code, 8, 16, QLatin1Char('0')));

  setLayout(layout);
  OnEditChanged();
}

void AssembleInstructionDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_input_edit, &QLineEdit::textChanged, this, &AssembleInstructionDialog::OnEditChanged);
}

void AssembleInstructionDialog::OnEditChanged()
{
  using namespace Common::GekkoAssembler;
  std::string line = m_input_edit->text().toStdString();
  Common::ToLower(&line);

  FailureOr<std::vector<CodeBlock>> asm_result = Assemble(line, m_address);

  if (IsFailure(asm_result))
  {
    m_button_box->button(QDialogButtonBox::Ok)->setEnabled(false);

    const AssemblerError& failure = GetFailure(asm_result);
    m_error_loc_label->setText(HtmlFormatErrorLoc(failure));
    m_error_line_label->setText(HtmlFormatErrorLine(failure));
    m_msg_label->setText(QString::fromStdString(failure.message).toHtmlEscaped());
  }
  else if (GetT(asm_result).empty() || GetT(asm_result)[0].instructions.empty())
  {
    m_button_box->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_error_loc_label->setText(
        QStringLiteral("<span style=\"color: red; font-weight: bold\">%1</span>").arg(tr("Error")));
    m_error_line_label->clear();
    m_msg_label->setText(tr("No input"));
  }
  else
  {
    m_button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
    m_code = 0;

    const std::vector<u8>& block_bytes = GetT(asm_result)[0].instructions;
    for (size_t i = 0; i < 4 && i < block_bytes.size(); i++)
    {
      m_code = (m_code << 8) | block_bytes[i];
    }

    m_error_loc_label->setText(
        QStringLiteral("<span style=\"color: green; font-weight: bold\">%1</span>").arg(tr("OK")));
    m_error_line_label->clear();
    m_msg_label->setText(tr("Instruction: %1").arg(m_code, 8, 16, QLatin1Char('0')));
  }
}

u32 AssembleInstructionDialog::GetCode() const
{
  return m_code;
}
