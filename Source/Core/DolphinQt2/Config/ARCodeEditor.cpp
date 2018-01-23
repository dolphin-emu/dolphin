// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/ARCodeEditor.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QStringList>
#include <QTextEdit>

#include "Core/ARDecrypt.h"

ARCodeEditor::ARCodeEditor(ActionReplay::ARCode& ar) : m_code(ar)
{
  CreateWidgets();
  ConnectWidgets();

  m_name_edit->setText(QString::fromStdString(ar.name));

  QString s;

  for (ActionReplay::AREntry& e : ar.ops)
  {
    s += QStringLiteral("%1 %2\n")
             .arg(e.cmd_addr, 8, 16, QLatin1Char('0'))
             .arg(e.value, 8, 16, QLatin1Char('0'));
  }

  m_code_edit->setText(s);
}

void ARCodeEditor::CreateWidgets()
{
  m_name_edit = new QLineEdit;
  m_code_edit = new QTextEdit;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);

  QGridLayout* grid_layout = new QGridLayout;

  grid_layout->addWidget(new QLabel(tr("Name:")), 0, 0);
  grid_layout->addWidget(m_name_edit, 0, 1);
  grid_layout->addWidget(new QLabel(tr("Code:")), 1, 0);
  grid_layout->addWidget(m_code_edit, 1, 1);
  grid_layout->addWidget(m_button_box, 2, 1);

  QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());

  m_code_edit->setFont(monospace);

  setLayout(grid_layout);
}

void ARCodeEditor::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &ARCodeEditor::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ARCodeEditor::accept()
{
  std::vector<ActionReplay::AREntry> entries;
  std::vector<std::string> encrypted_lines;

  QStringList lines = m_code_edit->toPlainText().split(QStringLiteral("\n"));

  for (int i = 0; i < lines.size(); i++)
  {
    QString line = lines[i];

    if (line.isEmpty())
      continue;

    QStringList values = line.split(QStringLiteral(" "));

    bool good = true;

    u32 addr = 0;
    u32 value = 0;

    if (values.size() == 2)
    {
      addr = values[0].toUInt(&good, 16);

      if (good)
        value = values[1].toUInt(&good, 16);

      if (good)
        entries.push_back(ActionReplay::AREntry(addr, value));
    }
    else
    {
      QStringList blocks = line.split(QStringLiteral("-"));

      if (blocks.size() == 3 && blocks[0].size() == 4 && blocks[1].size() == 4 &&
          blocks[2].size() == 4)
      {
        encrypted_lines.emplace_back(blocks[0].toStdString() + blocks[1].toStdString() +
                                     blocks[2].toStdString());
      }
      else
      {
        good = false;
      }
    }

    if (!good)
    {
      auto result = QMessageBox::warning(
          this, tr("Parsing Error"),
          tr("Unable to parse line %1 of the entered AR code as a valid "
             "encrypted or decrypted code. Make sure you typed it correctly.\n\n"
             "Would you like to ignore this line and continue parsing?")
              .arg(i + 1),
          QMessageBox::Ok | QMessageBox::Abort);

      if (result == QMessageBox::Abort)
        return;
    }
  }

  if (!encrypted_lines.empty())
  {
    if (!entries.empty())
    {
      auto result = QMessageBox::warning(
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
        return;
      default:
        break;
      }
    }
    ActionReplay::DecryptARCode(encrypted_lines, &entries);
  }

  if (entries.empty())
  {
    QMessageBox::critical(this, tr("Error"),
                          tr("The resulting decrypted AR code doesn't contain any lines."));
    return;
  }

  m_code.name = m_name_edit->text().toStdString();
  m_code.ops = std::move(entries);
  m_code.active = true;
  m_code.user_defined = true;

  QDialog::accept();
}
