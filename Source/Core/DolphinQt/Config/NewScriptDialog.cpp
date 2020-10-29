// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/NewScriptDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLineEdit>

#include "Core/ScriptEngine.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

// TODO This should be a file picker
NewScriptDialog::NewScriptDialog(QWidget* parent, ScriptEngine::ScriptTarget& script)
    : QDialog(parent), m_script(script)
{
  setWindowTitle(tr("Add New Script"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  ConnectWidgets();
}

void NewScriptDialog::CreateWidgets()
{
  m_file_path_edit = new QLineEdit;
  m_file_path_edit->setPlaceholderText(tr("Script file path"));

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);

  auto* layout = new QGridLayout;

  layout->addWidget(m_file_path_edit, 0, 0);
  layout->addWidget(m_button_box, 1, 0);

  setLayout(layout);
}

void NewScriptDialog::ConnectWidgets()
{
  connect(m_file_path_edit, qOverload<const QString&>(&QLineEdit::textEdited),
          [this](const QString& name) { m_script.file_path = name.toStdString(); });

  connect(m_button_box, &QDialogButtonBox::accepted, this, &NewScriptDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NewScriptDialog::accept()
{
  if (m_file_path_edit->text().isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("You have to enter a script path."));
    return;
  }
  QDialog::accept();
}
