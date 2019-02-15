// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/NewPatchDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Core/PatchEngine.h"

NewPatchDialog::NewPatchDialog(QWidget* parent, PatchEngine::Patch& patch)
    : QDialog(parent), m_patch(patch)
{
  setWindowTitle(tr("Patch Editor"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  ConnectWidgets();

  for (size_t i = 0; i < m_patch.entries.size(); i++)
  {
    m_entry_layout->addWidget(CreateEntry(static_cast<int>(i)));
  }

  if (m_patch.entries.empty())
  {
    AddEntry();
    m_patch.active = true;
  }
}

void NewPatchDialog::CreateWidgets()
{
  m_name_edit = new QLineEdit;
  m_name_edit->setPlaceholderText(tr("Patch name"));
  m_name_edit->setText(QString::fromStdString(m_patch.name));

  m_entry_widget = new QWidget;
  m_entry_layout = new QVBoxLayout;

  auto* scroll_area = new QScrollArea;
  m_entry_widget->setLayout(m_entry_layout);
  scroll_area->setWidget(m_entry_widget);
  scroll_area->setWidgetResizable(true);

  m_add_button = new QPushButton(tr("Add"));

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);

  auto* layout = new QGridLayout;

  layout->addWidget(m_name_edit, 0, 0);
  layout->addWidget(scroll_area, 1, 0);
  layout->addWidget(m_add_button, 2, 0);
  layout->addWidget(m_button_box, 3, 0);

  setLayout(layout);
}

void NewPatchDialog::ConnectWidgets()
{
  connect(m_name_edit, static_cast<void (QLineEdit::*)(const QString&)>(&QLineEdit::textEdited),
          [this](const QString& name) { m_patch.name = name.toStdString(); });

  connect(m_add_button, &QPushButton::pressed, this, &NewPatchDialog::AddEntry);

  connect(m_button_box, &QDialogButtonBox::accepted, this, &NewPatchDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NewPatchDialog::AddEntry()
{
  m_patch.entries.emplace_back();

  m_entry_layout->addWidget(CreateEntry(static_cast<int>(m_patch.entries.size() - 1)));
}

QGroupBox* NewPatchDialog::CreateEntry(int index)
{
  QGroupBox* box = new QGroupBox();

  auto* type = new QGroupBox(tr("Type"));
  auto* type_layout = new QHBoxLayout;
  auto* remove = new QPushButton(tr("Remove"));

  auto* byte = new QRadioButton(tr("8-bit"));
  auto* word = new QRadioButton(tr("16-bit"));
  auto* dword = new QRadioButton(tr("32-bit"));

  type_layout->addWidget(byte);
  type_layout->addWidget(word);
  type_layout->addWidget(dword);
  type->setLayout(type_layout);

  auto* offset = new QLineEdit;
  auto* value = new QLineEdit;

  m_edits.push_back(offset);
  m_edits.push_back(value);

  auto* layout = new QGridLayout;
  layout->addWidget(type, 0, 0, 1, -1);
  layout->addWidget(new QLabel(tr("Offset:")), 1, 0);
  layout->addWidget(offset, 1, 1);
  layout->addWidget(new QLabel(tr("Value:")), 2, 0);
  layout->addWidget(value, 2, 1);
  layout->addWidget(remove, 3, 0, 1, -1);
  box->setLayout(layout);

  connect(offset, static_cast<void (QLineEdit::*)(const QString&)>(&QLineEdit::textEdited),
          [this, index, offset](const QString& text) {
            bool okay = true;
            m_patch.entries[index].address = text.toUInt(&okay, 16);

            QFont font;
            QPalette palette;

            font.setBold(!okay);

            if (!okay)
              palette.setColor(QPalette::Text, Qt::red);

            offset->setFont(font);
            offset->setPalette(palette);
          });

  connect(value, static_cast<void (QLineEdit::*)(const QString&)>(&QLineEdit::textEdited),
          [this, index, value](const QString& text) {
            bool okay;
            m_patch.entries[index].value = text.toUInt(&okay, 16);

            QFont font;
            QPalette palette;

            font.setBold(!okay);

            if (!okay)
              palette.setColor(QPalette::Text, Qt::red);

            value->setFont(font);
            value->setPalette(palette);
          });

  connect(remove, &QPushButton::pressed, [this, box, index] {
    if (m_patch.entries.size() > 1)
    {
      m_entry_layout->removeWidget(box);
      m_patch.entries.erase(m_patch.entries.begin() + index);
      m_id--;
    }
  });

  connect(byte, &QRadioButton::toggled, [this, index](bool checked) {
    if (checked)
      m_patch.entries[index].type = PatchEngine::PatchType::Patch8Bit;
  });

  connect(word, &QRadioButton::toggled, [this, index](bool checked) {
    if (checked)
      m_patch.entries[index].type = PatchEngine::PatchType::Patch16Bit;
  });

  connect(dword, &QRadioButton::toggled, [this, index](bool checked) {
    if (checked)
      m_patch.entries[index].type = PatchEngine::PatchType::Patch32Bit;
  });

  auto entry_type = m_patch.entries[index].type;

  byte->setChecked(entry_type == PatchEngine::PatchType::Patch8Bit);
  word->setChecked(entry_type == PatchEngine::PatchType::Patch16Bit);
  dword->setChecked(entry_type == PatchEngine::PatchType::Patch32Bit);

  offset->setText(
      QStringLiteral("%1").arg(m_patch.entries[index].address, 10, 16, QLatin1Char('0')));
  value->setText(QStringLiteral("%1").arg(m_patch.entries[index].value, 10, 16, QLatin1Char('0')));

  return box;
}

void NewPatchDialog::accept()
{
  if (m_name_edit->text().isEmpty())
  {
    QMessageBox::critical(this, tr("Error"), tr("You have to enter a name."));
    return;
  }

  bool valid = true;

  for (const auto* edit : m_edits)
  {
    edit->text().toUInt(&valid, 16);
    if (!valid)
      break;
  }

  if (!valid)
  {
    QMessageBox::critical(
        this, tr("Error"),
        tr("Some values you provided are invalid.\nPlease check the highlighted values."));
    return;
  }

  QDialog::accept();
}
