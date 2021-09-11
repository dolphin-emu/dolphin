// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/AddLocalPlayers.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTextEdit>

#include "Core/LocalPlayersConfig.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

AddLocalPlayersEditor::AddLocalPlayersEditor(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("Add Local Players"));

  CreateWidgets();
  ConnectWidgets();
}

void AddLocalPlayersEditor::SetPlayer(AddPlayers::AddPlayers* name)
{
  m_username_edit->setText(QString::fromStdString(name->username));
  m_userid_edit->setText(QString::fromStdString(name->userid));

  m_local_player = name;
}

void AddLocalPlayersEditor::CreateWidgets()
{
  m_username_edit = new QLineEdit;
  m_userid_edit = new QLineEdit;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);


  QGridLayout* grid_layout = new QGridLayout;

  grid_layout->addWidget(new QLabel(tr("Username:")), 0, 0);
  grid_layout->addWidget(m_username_edit, 0, 1);
  grid_layout->addWidget(new QLabel(tr("User ID:")), 1, 0);
  grid_layout->addWidget(m_userid_edit, 1, 1);
  grid_layout->addWidget(m_button_box, 4, 1);

  QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());

  m_userid_edit->setFont(monospace);

  setLayout(grid_layout);
}

void AddLocalPlayersEditor::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &AddLocalPlayersEditor::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}


bool AddLocalPlayersEditor::AcceptPlayer()
{
  m_local_player->username = m_username_edit->text().toStdString();

  if (m_local_player->username.empty())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("You must enter a username"));
    return false;
  }

  m_local_player->userid = m_userid_edit->text().toStdString();

  if (m_local_player->userid.empty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("You must enter a User ID"));
    return false;
  }

  return true;
}

void AddLocalPlayersEditor::accept()
{
  bool success = AcceptPlayer();

  if (success)
    QDialog::accept();
}
