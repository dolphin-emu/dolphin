// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/NewPatchDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Core/PatchEngine.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

NewPatchDialog::NewPatchDialog(QWidget* parent, PatchEngine::Patch& patch)
    : QDialog(parent), m_patch(patch)
{
  setWindowTitle(tr("Patch Editor"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  ConnectWidgets();

  for (size_t i = 0; i < m_patch.entries.size(); i++)
  {
    m_entry_layout->addWidget(CreateEntry(m_patch.entries[i]));
  }

  if (m_patch.entries.empty())
  {
    AddEntry();
    m_patch.enabled = true;
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
  connect(m_name_edit, qOverload<const QString&>(&QLineEdit::textEdited),
          [this](const QString& name) { m_patch.name = name.toStdString(); });

  connect(m_add_button, &QPushButton::clicked, this, &NewPatchDialog::AddEntry);

  connect(m_button_box, &QDialogButtonBox::accepted, this, &NewPatchDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NewPatchDialog::AddEntry()
{
  m_patch.entries.emplace_back();

  m_entry_layout->addWidget(CreateEntry(m_patch.entries[m_patch.entries.size() - 1]));
}

static u32 OnTextEdited(QLineEdit* edit, const QString& text)
{
  bool okay = false;
  u32 value = text.toUInt(&okay, 16);

  QFont font;
  QPalette palette;

  font.setBold(!okay);

  if (!okay)
    palette.setColor(QPalette::Text, Qt::red);

  edit->setFont(font);
  edit->setPalette(palette);

  return value;
}

static bool PatchEq(const PatchEngine::PatchEntry& a, const PatchEngine::PatchEntry& b)
{
  if (a.address != b.address)
    return false;

  if (a.type != b.type)
    return false;

  if (a.value != b.value)
    return false;

  if (a.comparand != b.comparand)
    return false;

  if (a.conditional != b.conditional)
    return false;

  return true;
}

QGroupBox* NewPatchDialog::CreateEntry(PatchEngine::PatchEntry& entry)
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

  auto* address = new QLineEdit;
  auto* value = new QLineEdit;
  auto* comparand = new QLineEdit;

  m_edits.push_back(address);
  m_edits.push_back(value);
  m_edits.push_back(comparand);

  auto* conditional = new QCheckBox(tr("Conditional"));
  auto* comparand_label = new QLabel(tr("Comparand:"));

  auto* layout = new QGridLayout;
  layout->addWidget(type, 0, 0, 1, -1);
  layout->addWidget(new QLabel(tr("Address:")), 1, 0);
  layout->addWidget(address, 1, 1);
  layout->addWidget(new QLabel(tr("Value:")), 2, 0);
  layout->addWidget(value, 2, 1);
  layout->addWidget(conditional, 3, 0, 1, -1);
  layout->addWidget(comparand_label, 4, 0);
  layout->addWidget(comparand, 4, 1);
  layout->addWidget(remove, 5, 0, 1, -1);
  box->setLayout(layout);

  connect(address, qOverload<const QString&>(&QLineEdit::textEdited),
          [&entry, address](const QString& text) { entry.address = OnTextEdited(address, text); });

  connect(value, qOverload<const QString&>(&QLineEdit::textEdited),
          [&entry, value](const QString& text) { entry.value = OnTextEdited(value, text); });

  connect(comparand, qOverload<const QString&>(&QLineEdit::textEdited),
          [&entry, comparand](const QString& text) {
            entry.comparand = OnTextEdited(comparand, text);
          });

  connect(remove, &QPushButton::clicked, [this, box, address, value, comparand, entry] {
    if (m_patch.entries.size() > 1)
    {
      box->setVisible(false);
      m_entry_layout->removeWidget(box);
      box->deleteLater();

      m_patch.entries.erase(
          std::find_if(m_patch.entries.begin(), m_patch.entries.end(),
                       [entry](const PatchEngine::PatchEntry& e) { return PatchEq(e, entry); }));

      const auto it = std::remove_if(
          m_edits.begin(), m_edits.end(), [address, value, comparand](QLineEdit* line_edit) {
            return line_edit == address || line_edit == value || line_edit == comparand;
          });
      m_edits.erase(it, m_edits.end());
    }
  });

  connect(byte, &QRadioButton::toggled, [&entry](bool checked) {
    if (checked)
      entry.type = PatchEngine::PatchType::Patch8Bit;
  });

  connect(word, &QRadioButton::toggled, [&entry](bool checked) {
    if (checked)
      entry.type = PatchEngine::PatchType::Patch16Bit;
  });

  connect(dword, &QRadioButton::toggled, [&entry](bool checked) {
    if (checked)
      entry.type = PatchEngine::PatchType::Patch32Bit;
  });

  byte->setChecked(entry.type == PatchEngine::PatchType::Patch8Bit);
  word->setChecked(entry.type == PatchEngine::PatchType::Patch16Bit);
  dword->setChecked(entry.type == PatchEngine::PatchType::Patch32Bit);

  connect(conditional, &QCheckBox::toggled, [&entry, comparand_label, comparand](bool checked) {
    entry.conditional = checked;
    comparand_label->setVisible(checked);
    comparand->setVisible(checked);
  });

  conditional->setChecked(entry.conditional);
  comparand_label->setVisible(entry.conditional);
  comparand->setVisible(entry.conditional);

  address->setText(QStringLiteral("%1").arg(entry.address, 8, 16, QLatin1Char('0')));
  value->setText(QStringLiteral("%1").arg(entry.value, 8, 16, QLatin1Char('0')));
  comparand->setText(QStringLiteral("%1").arg(entry.comparand, 8, 16, QLatin1Char('0')));

  return box;
}

void NewPatchDialog::accept()
{
  if (m_name_edit->text().isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("You have to enter a name."));
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
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("Some values you provided are invalid.\nPlease check the highlighted values."));
    return;
  }

  QDialog::accept();
}
