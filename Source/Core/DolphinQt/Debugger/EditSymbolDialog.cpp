// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/EditSymbolDialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

EditSymbolDialog::EditSymbolDialog(QWidget* parent, const u32 symbol_address, u32* symbol_size,
                                   std::string* symbol_name, Type type)
    : QDialog(parent), m_type(type), m_symbol_name(symbol_name), m_symbol_size(symbol_size),
      m_symbol_address(symbol_address)
{
  setWindowTitle(m_type == Type::Symbol ? tr("Edit Symbol") : tr("Edit Note"));
  CreateWidgets();
  ConnectWidgets();
}

void EditSymbolDialog::CreateWidgets()
{
  m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  m_buttons->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
  m_buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);

  QLabel* info_label =
      // i18n: %1 is an address. %2 is a warning message.
      new QLabel((m_type == Type::Symbol ? tr("Editing symbol starting at: %1\n%2") :
                                           tr("Editing note starting at: %1\n%2"))
                     .arg(QString::number(m_symbol_address, 16))
                     .arg(tr("Warning: Must save the symbol map for changes to be kept.")));
  m_name_edit = new QLineEdit();
  m_name_edit->setPlaceholderText(m_type == Type::Symbol ? tr("Symbol name") : tr("Note name"));

  auto* size_layout = new QHBoxLayout;
  // i18n: The address where a symbol ends
  QLabel* address_end_label = new QLabel(tr("End Address"));
  // i18n: The number of lines of code
  QLabel* size_lines_label = new QLabel(tr("Lines"));
  // i18n: There's a text field immediately to the right of this label where 8 hexadecimal digits
  // can be entered. The "0x" in this string acts as a prefix to the digits to indicate that they
  // are hexadecimal.
  QLabel* size_hex_label = new QLabel(tr("Size: 0x"));
  m_address_end_edit = new QLineEdit();
  m_size_lines_spin = new QSpinBox();
  m_size_hex_edit = new QLineEdit();

  size_hex_label->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  size_lines_label->setAlignment(Qt::AlignCenter | Qt::AlignRight);

  // Get system font and use to size boxes.
  QFont font;
  QFontMetrics fm(font);
  const int width = fm.horizontalAdvance(QLatin1Char('0')) * 2;
  m_address_end_edit->setFixedWidth(width * 6);
  m_size_hex_edit->setFixedWidth(width * 5);
  m_size_lines_spin->setFixedWidth(width * 5);
  m_size_hex_edit->setMaxLength(7);
  m_size_lines_spin->setRange(0, 99999);

  // Accept hex input only
  QRegularExpression rx(QStringLiteral("[0-9a-fA-F]{0,8}"));
  QValidator* validator = new QRegularExpressionValidator(rx, this);
  m_address_end_edit->setValidator(validator);
  m_size_hex_edit->setValidator(validator);

  size_layout->addWidget(address_end_label);
  size_layout->addWidget(m_address_end_edit);
  size_layout->addWidget(size_hex_label);
  size_layout->addWidget(m_size_hex_edit);
  size_layout->addWidget(size_lines_label);
  size_layout->addWidget(m_size_lines_spin);

  auto* layout = new QVBoxLayout();
  layout->addWidget(info_label);
  layout->addWidget(m_name_edit);
  layout->addLayout(size_layout);
  layout->addWidget(m_buttons);

  setLayout(layout);

  FillFunctionData();
}

void EditSymbolDialog::FillFunctionData()
{
  m_name_edit->setText(QString::fromStdString(*m_symbol_name));
  m_size_lines_spin->setValue(*m_symbol_size / 4);
  m_size_hex_edit->setText(QString::number(*m_symbol_size, 16));
  m_address_end_edit->setText(
      QStringLiteral("%1").arg(m_symbol_address + *m_symbol_size, 8, 16, QLatin1Char('0')));
}

void EditSymbolDialog::UpdateAddressData(u32 size)
{
  // Not sure what the max size should be. Definitely not a full 8, so set to 7.
  size = size & 0xFFFFFFF;

  m_size_lines_spin->setValue(size / 4);
  m_size_hex_edit->setText(QString::number(size, 16));
  m_address_end_edit->setText(
      QStringLiteral("%1").arg(m_symbol_address + size, 8, 16, QLatin1Char('0')));
}

void EditSymbolDialog::ConnectWidgets()
{
  connect(m_size_lines_spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) { UpdateAddressData(value * 4); });

  connect(m_size_hex_edit, &QLineEdit::editingFinished, this, [this] {
    bool good;
    const u32 size = m_size_hex_edit->text().toUInt(&good, 16);
    if (good)
      UpdateAddressData(size);
  });

  connect(m_address_end_edit, &QLineEdit::textEdited, this, [this] {
    bool good;
    const u32 end = m_address_end_edit->text().toUInt(&good, 16);
    if (good && end > m_symbol_address)
      UpdateAddressData(end - m_symbol_address);
  });

  connect(m_buttons, &QDialogButtonBox::accepted, this, &EditSymbolDialog::Accepted);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &EditSymbolDialog::reject);
  connect(m_buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton* btn) {
    const auto role = m_buttons->buttonRole(btn);
    if (role == QDialogButtonBox::ButtonRole::ResetRole)
    {
      FillFunctionData();
    }
    else if (role == QDialogButtonBox::ButtonRole::DestructiveRole)
    {
      m_delete_chosen = true;
      QDialog::accept();
    }
  });
}

void EditSymbolDialog::Accepted()
{
  const std::string name = m_name_edit->text().toStdString();

  if (*m_symbol_name != name)
    *m_symbol_name = name;

  bool good;
  const u32 size = m_size_hex_edit->text().toUInt(&good, 16);

  if (good && *m_symbol_size != size)
    *m_symbol_size = size;

  QDialog::accept();
}
