// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/CheatCodeEditor.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTextEdit>

#include "Common/StringUtil.h"

#include "Core/ARDecrypt.h"
#include "Core/ActionReplay.h"
#include "Core/GeckoCodeConfig.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

CheatCodeEditor::CheatCodeEditor(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("Cheat Code Editor"));

  CreateWidgets();
  ConnectWidgets();
}

void CheatCodeEditor::SetARCode(ActionReplay::ARCode* code)
{
  m_name_edit->setText(QString::fromStdString(code->name));

  m_code_edit->clear();

  for (ActionReplay::AREntry& e : code->ops)
    m_code_edit->append(QString::fromStdString(ActionReplay::SerializeLine(e)));

  m_creator_label->setHidden(true);
  m_creator_edit->setHidden(true);
  m_notes_label->setHidden(true);
  m_notes_edit->setHidden(true);

  m_ar_code = code;
  m_gecko_code = nullptr;
}

void CheatCodeEditor::SetGeckoCode(Gecko::GeckoCode* code)
{
  m_name_edit->setText(QString::fromStdString(code->name));
  m_creator_edit->setText(QString::fromStdString(code->creator));

  m_code_edit->clear();

  for (const auto& c : code->codes)
    m_code_edit->append(QString::fromStdString(c.original_line));

  QString notes_string;
  for (const auto& line : code->notes)
    notes_string += QStringLiteral("%1\n").arg(QString::fromStdString(line));

  m_notes_edit->setText(notes_string);

  m_creator_label->setHidden(false);
  m_creator_edit->setHidden(false);
  m_notes_label->setHidden(false);
  m_notes_edit->setHidden(false);

  m_gecko_code = code;
  m_ar_code = nullptr;
}

void CheatCodeEditor::CreateWidgets()
{
  m_name_edit = new QLineEdit;
  m_creator_edit = new QLineEdit;
  m_notes_edit = new QTextEdit;
  m_code_edit = new QTextEdit;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);

  m_creator_label = new QLabel(tr("Creator:"));
  m_notes_label = new QLabel(tr("Notes:"));

  QGridLayout* grid_layout = new QGridLayout;

  grid_layout->addWidget(new QLabel(tr("Name:")), 0, 0);
  grid_layout->addWidget(m_name_edit, 0, 1);
  grid_layout->addWidget(m_creator_label, 1, 0);
  grid_layout->addWidget(m_creator_edit, 1, 1);
  grid_layout->addWidget(m_notes_label, 2, 0);
  grid_layout->addWidget(m_notes_edit, 2, 1);
  grid_layout->addWidget(new QLabel(tr("Code:")), 3, 0);
  grid_layout->addWidget(m_code_edit, 3, 1);
  grid_layout->addWidget(m_button_box, 4, 1);

  QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());

  m_code_edit->setFont(monospace);

  m_code_edit->setAcceptRichText(false);
  m_notes_edit->setAcceptRichText(false);

  setLayout(grid_layout);
}

void CheatCodeEditor::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &CheatCodeEditor::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool CheatCodeEditor::AcceptAR()
{
  QString name = m_name_edit->text();

  std::vector<ActionReplay::AREntry> entries;
  std::vector<std::string> encrypted_lines;

  QStringList lines = m_code_edit->toPlainText().split(QLatin1Char{'\n'});

  for (int i = 0; i < lines.size(); i++)
  {
    QString line = lines[i];

    if (line.isEmpty())
      continue;
    if (i == 0 && line[0] == u'$')
    {
      if (name.isEmpty())
        name = line.right(line.size() - 1);

      continue;
    }

    const auto parse_result = ActionReplay::DeserializeLine(line.toStdString());

    if (std::holds_alternative<ActionReplay::AREntry>(parse_result))
    {
      entries.push_back(std::get<ActionReplay::AREntry>(parse_result));
    }
    else if (std::holds_alternative<ActionReplay::EncryptedLine>(parse_result))
    {
      encrypted_lines.emplace_back(std::get<ActionReplay::EncryptedLine>(parse_result));
    }
    else
    {
      auto result = ModalMessageBox::warning(
          this, tr("Parsing Error"),
          tr("Unable to parse line %1 of the entered AR code as a valid "
             "encrypted or decrypted code. Make sure you typed it correctly.\n\n"
             "Would you like to ignore this line and continue parsing?")
              .arg(i + 1),
          QMessageBox::Ok | QMessageBox::Abort);

      if (result == QMessageBox::Abort)
        return false;
    }
  }

  if (!encrypted_lines.empty())
  {
    if (!entries.empty())
    {
      auto result = ModalMessageBox::warning(
          this, tr("Invalid Mixed Code"),
          tr("This Action Replay code contains both encrypted and unencrypted lines; "
             "you should check that you have entered it correctly.\n\n"
             "Do you want to discard all unencrypted lines?"),
          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

      // YES = Discard the unencrypted lines then decrypt the encrypted ones instead.
      // NO = Discard the encrypted lines, keep the unencrypted ones
      // CANCEL = Stop and let the user go back to editing
      switch (result)
      {
      case QMessageBox::Yes:
        entries.clear();
        break;
      case QMessageBox::No:
        encrypted_lines.clear();
        break;
      case QMessageBox::Cancel:
        return false;
      default:
        break;
      }
    }
    ActionReplay::DecryptARCode(encrypted_lines, &entries);
  }

  if (entries.empty())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("The resulting decrypted AR code doesn't contain any lines."));
    return false;
  }

  m_ar_code->name = name.toStdString();
  m_ar_code->ops = std::move(entries);
  m_ar_code->user_defined = true;

  return true;
}

bool CheatCodeEditor::AcceptGecko()
{
  QString name = m_name_edit->text();

  std::vector<Gecko::GeckoCode::Code> entries;

  QStringList lines = m_code_edit->toPlainText().split(QLatin1Char{'\n'});

  for (int i = 0; i < lines.size(); i++)
  {
    QString line = lines[i];

    if (line.isEmpty())
      continue;

    if (i == 0 && line[0] == u'$')
    {
      if (name.isEmpty())
        name = line.right(line.size() - 1);

      continue;
    }

    if (std::optional<Gecko::GeckoCode::Code> c = Gecko::DeserializeLine(line.toStdString()))
    {
      entries.push_back(*c);
    }
    else
    {
      auto result = ModalMessageBox::warning(
          this, tr("Parsing Error"),
          tr("Unable to parse line %1 of the entered Gecko code as a valid "
             "code. Make sure you typed it correctly.\n\n"
             "Would you like to ignore this line and continue parsing?")
              .arg(i + 1),
          QMessageBox::Ok | QMessageBox::Abort);

      if (result == QMessageBox::Abort)
        return false;
    }
  }

  if (entries.empty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("This Gecko code doesn't contain any lines."));
    return false;
  }

  m_gecko_code->name = name.toStdString();
  m_gecko_code->creator = m_creator_edit->text().toStdString();
  m_gecko_code->codes = std::move(entries);
  m_gecko_code->notes = SplitString(m_notes_edit->toPlainText().toStdString(), '\n');
  m_gecko_code->user_defined = true;

  return true;
}

void CheatCodeEditor::accept()
{
  bool success = m_gecko_code != nullptr ? AcceptGecko() : AcceptAR();

  if (success)
    QDialog::accept();
}
