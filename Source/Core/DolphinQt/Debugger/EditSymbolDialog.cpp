// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/EditSymbolDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

EditSymbolDialog::EditSymbolDialog(QWidget* parent, const u32 symbol_address, u32& symbol_size,
                                   std::string& symbol_name)
    : QDialog(parent), m_symbol_name(symbol_name), m_symbol_size(symbol_size),
      m_symbol_address(symbol_address)
{
  setWindowTitle(tr("Edit Symbol for a Function"));
  CreateWidgets();
  ConnectWidgets();
}

void EditSymbolDialog::CreateWidgets()
{
  m_reset_button = new QPushButton(tr("Reset"));
  m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  m_buttons->addButton(m_reset_button, QDialogButtonBox::ResetRole);

  m_info_label = new QLabel(tr("Editing symbol for function starting at: ") +
                            QString::number(m_symbol_address, 16));
  m_name_edit = new QLineEdit();
  m_name_edit->setPlaceholderText(tr("Function name"));

  auto* size_layout = new QHBoxLayout;
  m_address_end_label = new QLabel(tr("End Address"));
  m_size_lines_label = new QLabel(tr("Lines"));
  m_size_hex_label = new QLabel(tr("Size: 0x"));
  m_address_end_edit = new QLineEdit();
  m_size_lines_spin = new QSpinBox();
  m_size_hex_edit = new QLineEdit();

  m_size_hex_label->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  m_size_lines_label->setAlignment(Qt::AlignCenter | Qt::AlignRight);

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

  size_layout->addWidget(m_address_end_label);
  size_layout->addWidget(m_address_end_edit);
  size_layout->addWidget(m_size_hex_label);
  size_layout->addWidget(m_size_hex_edit);
  size_layout->addWidget(m_size_lines_label);
  size_layout->addWidget(m_size_lines_spin);

  auto* layout = new QVBoxLayout();
  layout->addWidget(m_info_label);
  layout->addWidget(m_name_edit);
  layout->addLayout(size_layout);
  layout->addWidget(m_buttons);

  setLayout(layout);

  FillFunctionData();
}

void EditSymbolDialog::FillFunctionData()
{
  m_name_edit->setText(QString::fromStdString(m_symbol_name));
  m_size_lines_spin->setValue(m_symbol_size / 4);
  m_size_hex_edit->setText(QString::number(m_symbol_size, 16));
  m_address_end_edit->setText(
      QStringLiteral("%1").arg(m_symbol_address + m_symbol_size, 8, 16, QLatin1Char('0')));
}

void EditSymbolDialog::UpdateAddressData(u32 size)
{
  // Not sure what the max size should be. Definitely not a full 8, so set to 7.
  size = size & 0xFFFFFFC;
  if (size < 4)
    size = 4;

  m_size_lines_spin->setValue(size / 4);
  m_size_hex_edit->setText(QString::number(size, 16));  //("%1").arg(size, 4, 16));
  m_address_end_edit->setText(
      QStringLiteral("%1").arg(m_symbol_address + size, 8, 16, QLatin1Char('0')));
}

void EditSymbolDialog::ConnectWidgets()
{
  connect(m_size_lines_spin, QOverload<int>::of(&QSpinBox::valueChanged),
          [this](int value) { UpdateAddressData(value * 4); });

  connect(m_size_hex_edit, &QLineEdit::editingFinished, [this] {
    bool good;
    u32 size = m_size_hex_edit->text().toUInt(&good, 16);
    if (good)
      UpdateAddressData(size);
  });

  connect(m_address_end_edit, &QLineEdit::textEdited, [this] {
    bool good;
    u32 end = m_address_end_edit->text().toUInt(&good, 16);
    if (good && end > m_symbol_address)
      UpdateAddressData(end - m_symbol_address);
  });

  connect(m_buttons, &QDialogButtonBox::accepted, this, &EditSymbolDialog::Accepted);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &EditSymbolDialog::reject);
  connect(m_reset_button, &QPushButton::pressed, this, &EditSymbolDialog::FillFunctionData);
}

void EditSymbolDialog::Accepted()
{
  const std::string name = m_name_edit->text().toStdString();

  if (m_symbol_name != name)
    m_symbol_name = name;

  bool good;
  const u32 size = m_size_hex_edit->text().toUInt(&good, 16);

  if (good && m_symbol_size != size)
    m_symbol_size = size;

  QDialog::accept();
}
