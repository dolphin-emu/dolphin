// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/CheatCodeEditor.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QStringList>
#include <QTextEdit>

#include "Core/ARDecrypt.h"
#include "Core/ActionReplay.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/PatchEngine.h"

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

  QString s;

  for (ActionReplay::AREntry& e : code->ops)
  {
    s += QStringLiteral("%1 %2\n")
             .arg(e.cmd_addr, 8, 16, QLatin1Char('0'))
             .arg(e.value, 8, 16, QLatin1Char('0'));
  }

  m_code_edit->setText(s);

  m_creator_label->setHidden(true);
  m_creator_edit->setHidden(true);
  m_notes_label->setHidden(true);
  m_notes_edit->setHidden(true);

  m_ar_code = code;
  m_gecko_code = nullptr;
  m_patch = nullptr;
}

void CheatCodeEditor::SetGeckoCode(Gecko::GeckoCode* code)
{
  m_name_edit->setText(QString::fromStdString(code->name));
  m_creator_edit->setText(QString::fromStdString(code->creator));

  QString code_string;

  for (const auto& c : code->codes)
    code_string += QStringLiteral("%1 %2\n")
                       .arg(c.address, 8, 16, QLatin1Char('0'))
                       .arg(c.data, 8, 16, QLatin1Char('0'));

  m_code_edit->setText(code_string);

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
  m_patch = nullptr;
}

QStringList s_patch_types = { QStringLiteral("byte"), QStringLiteral("word"), QStringLiteral("dword") };

void CheatCodeEditor::SetDolphinPatch(PatchEngine::Patch* patch)
{
  m_name_edit->setText(QString::fromStdString(patch->name));

  QString code_string;

  //QString type[] = { QStringLiteral("byte"), QStringLiteral("word"), QStringLiteral("dword") };
  int length[] = { 2, 4, 8 };

  for (const auto& e : patch->entries)
  {
    size_t idx = static_cast<size_t>(e.type);
    assert(idx <= 3);
    code_string += QStringLiteral("0x%1:%2:0x%3\n")
                       .arg(e.address, 8, 16, QLatin1Char('0'))
                       .arg(s_patch_types[idx])
                       .arg(e.value, length[idx], 16, QLatin1Char('0'));
  }

  m_code_edit->setText(code_string);

  m_creator_label->setHidden(true);
  m_creator_edit->setHidden(true);
  m_notes_label->setHidden(true);
  m_notes_edit->setHidden(true);

  m_gecko_code = nullptr;
  m_ar_code = nullptr;
  m_patch = patch;
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
  std::vector<ActionReplay::AREntry> entries;
  std::vector<std::string> encrypted_lines;

  QStringList lines = m_code_edit->toPlainText().split(QLatin1Char{'\n'});

  for (int i = 0; i < lines.size(); i++)
  {
    QString line = lines[i];

    if (line.isEmpty())
      continue;

    QStringList values = line.split(QLatin1Char{' '});

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
      QStringList blocks = line.split(QLatin1Char{'-'});

      if (blocks.size() == 3 && blocks[0].size() == 4 && blocks[1].size() == 4 &&
          blocks[2].size() == 5)
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

  m_ar_code->name = m_name_edit->text().toStdString();
  m_ar_code->ops = std::move(entries);
  m_ar_code->user_defined = true;

  return true;
}

bool CheatCodeEditor::AcceptGecko()
{
  std::vector<Gecko::GeckoCode::Code> entries;

  QStringList lines = m_code_edit->toPlainText().split(QLatin1Char{'\n'});

  for (int i = 0; i < lines.size(); i++)
  {
    QString line = lines[i];

    if (line.isEmpty())
      continue;

    QStringList values = line.split(QLatin1Char{' '});

    bool good = values.size() == 2;

    u32 addr = 0;
    u32 value = 0;

    if (good)
      addr = values[0].toUInt(&good, 16);

    if (good)
      value = values[1].toUInt(&good, 16);

    if (!good)
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
    else
    {
      Gecko::GeckoCode::Code c;
      c.address = addr;
      c.data = value;
      c.original_line = line.toStdString();

      entries.push_back(c);
    }
  }

  if (entries.empty())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("The resulting decrypted AR code doesn't contain any lines."));
    return false;
  }

  m_gecko_code->name = m_name_edit->text().toStdString();
  m_gecko_code->creator = m_creator_edit->text().toStdString();
  m_gecko_code->codes = std::move(entries);
  m_gecko_code->user_defined = true;

  std::vector<std::string> note_lines;
  for (const QString& line : m_notes_edit->toPlainText().split(QLatin1Char{'\n'}))
    note_lines.push_back(line.toStdString());

  m_gecko_code->notes = std::move(note_lines);

  return true;
}

bool CheatCodeEditor::AcceptPatch()
{
  std::vector<PatchEngine::PatchEntry> entries;

  QStringList lines = m_code_edit->toPlainText().split(QLatin1Char{'\n'});

  for (int i = 0; i < lines.size(); i++) {
    QString line = lines[i];

    if (line.isEmpty())
      continue;

    QStringList values = line.split(QLatin1Char{':'});

    bool good = values.size() == 3;

    u32 addr = 0;
    int type = -1;
    u32 value = 0;

    if (good)
      addr = values[0].toUInt(&good, 16);

    if (good)
    {
      type = s_patch_types.indexOf(values[1].toLower());
      good = type != -1;
    }

    if (good)
      value = values[2].toUInt(&good, 16);

    if (!good)
    {
      auto result = ModalMessageBox::warning(
          this, tr("Parsing Error"),
          tr("Unable to parse line %1 the patch as valid. Make sure you"
             "typed it correctly.\n\n"
             "Would you like to ignore this line and continue parsing?")
              .arg(i + 1),
          QMessageBox::Ok | QMessageBox::Abort);

      if (result == QMessageBox::Abort)
        return false;
    }
    else
    {
      entries.emplace_back(PatchEngine::PatchType(type), addr, value);
    }

  }

  if (entries.empty())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("The patch doesn't contain any lines."));
    return false;
  }

  m_patch->name = m_name_edit->text().toStdString();
  m_patch->entries = entries;
  m_patch->user_defined = true;

  return true;
}

void CheatCodeEditor::accept()
{
  bool success = (m_gecko_code != nullptr && AcceptGecko())
              || (m_ar_code != nullptr && AcceptAR())
              || (m_patch != nullptr && AcceptPatch());

  if (success)
    QDialog::accept();
}
