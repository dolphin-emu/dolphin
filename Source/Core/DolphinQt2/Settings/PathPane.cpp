// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"

#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Settings/PathPane.h"

PathPane::PathPane(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Paths"));

  QVBoxLayout* layout = new QVBoxLayout;
  layout->addWidget(MakeGameFolderBox());
  layout->addLayout(MakePathsLayout());

  setLayout(layout);
}

void PathPane::Browse()
{
  QString dir =
      QFileDialog::getExistingDirectory(this, tr("Select a Directory"), QDir::currentPath());
  if (!dir.isEmpty())
    Settings::Instance().AddPath(dir);
}

void PathPane::BrowseDefaultGame()
{
  QString file = QFileDialog::getOpenFileName(
      this, tr("Select a Game"), QDir::currentPath(),
      tr("All GC/Wii files (*.elf *.dol *.gcm *.iso *.tgc *.wbfs *.ciso *.gcz *.wad);;"
         "All Files (*)"));
  if (!file.isEmpty())
  {
    m_game_edit->setText(file);
    SConfig::GetInstance().m_strDefaultISO = file.toStdString();
  }
}

void PathPane::BrowseDVDRoot()
{
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select DVD Root"), QDir::currentPath());
  if (!dir.isEmpty())
  {
    m_dvd_edit->setText(dir);
    SConfig::GetInstance().m_strDVDRoot = dir.toStdString();
  }
}

void PathPane::BrowseApploader()
{
  QString file = QFileDialog::getOpenFileName(this, tr("Select an Apploader"), QDir::currentPath(),
                                              tr("Apploaders (*.img)"));
  if (!file.isEmpty())
  {
    m_app_edit->setText(file);
    SConfig::GetInstance().m_strApploader = file.toStdString();
  }
}

void PathPane::BrowseWiiNAND()
{
  QString dir =
      QFileDialog::getExistingDirectory(this, tr("Select Wii NAND Root"), QDir::currentPath());
  if (!dir.isEmpty())
  {
    m_nand_edit->setText(dir);
    SConfig::GetInstance().m_NANDPath = dir.toStdString();
  }
}

QGroupBox* PathPane::MakeGameFolderBox()
{
  QGroupBox* game_box = new QGroupBox(tr("Game Folders"));
  game_box->setMinimumSize(QSize(400, 250));
  QVBoxLayout* vlayout = new QVBoxLayout;

  m_path_list = new QListWidget;
  m_path_list->insertItems(0, Settings::Instance().GetPaths());
  m_path_list->setSpacing(1);
  connect(&Settings::Instance(), &Settings::PathAdded,
          [this](const QString& dir) { m_path_list->addItem(dir); });
  connect(&Settings::Instance(), &Settings::PathRemoved, [this](const QString& dir) {
    auto items = m_path_list->findItems(dir, Qt::MatchExactly);
    for (auto& item : items)
      delete item;
  });
  vlayout->addWidget(m_path_list);

  QHBoxLayout* hlayout = new QHBoxLayout;

  hlayout->addStretch();
  QPushButton* add = new QPushButton(tr("Add"));
  QPushButton* remove = new QPushButton(tr("Remove"));
  hlayout->addWidget(add);
  hlayout->addWidget(remove);
  vlayout->addLayout(hlayout);

  connect(add, &QPushButton::clicked, this, &PathPane::Browse);
  connect(remove, &QPushButton::clicked, this, &PathPane::RemovePath);

  game_box->setLayout(vlayout);
  return game_box;
}

QGridLayout* PathPane::MakePathsLayout()
{
  QGridLayout* layout = new QGridLayout;
  layout->setColumnStretch(1, 1);

  m_game_edit = new QLineEdit(QString::fromStdString(SConfig::GetInstance().m_strDefaultISO));
  connect(m_game_edit, &QLineEdit::editingFinished,
          [=] { SConfig::GetInstance().m_strDefaultISO = m_game_edit->text().toStdString(); });
  QPushButton* game_open = new QPushButton;
  connect(game_open, &QPushButton::clicked, this, &PathPane::BrowseDefaultGame);
  layout->addWidget(new QLabel(tr("Default Game")), 0, 0);
  layout->addWidget(m_game_edit, 0, 1);
  layout->addWidget(game_open, 0, 2);

  m_dvd_edit = new QLineEdit(QString::fromStdString(SConfig::GetInstance().m_strDVDRoot));
  connect(m_dvd_edit, &QLineEdit::editingFinished,
          [=] { SConfig::GetInstance().m_strDVDRoot = m_dvd_edit->text().toStdString(); });
  QPushButton* dvd_open = new QPushButton;
  connect(dvd_open, &QPushButton::clicked, this, &PathPane::BrowseDVDRoot);
  layout->addWidget(new QLabel(tr("DVD Root")), 1, 0);
  layout->addWidget(m_dvd_edit, 1, 1);
  layout->addWidget(dvd_open, 1, 2);

  m_app_edit = new QLineEdit(QString::fromStdString(SConfig::GetInstance().m_strApploader));
  connect(m_app_edit, &QLineEdit::editingFinished,
          [=] { SConfig::GetInstance().m_strApploader = m_app_edit->text().toStdString(); });
  QPushButton* app_open = new QPushButton;
  connect(app_open, &QPushButton::clicked, this, &PathPane::BrowseApploader);
  layout->addWidget(new QLabel(tr("Apploader")), 2, 0);
  layout->addWidget(m_app_edit, 2, 1);
  layout->addWidget(app_open, 2, 2);

  m_nand_edit = new QLineEdit(QString::fromStdString(SConfig::GetInstance().m_NANDPath));
  connect(m_nand_edit, &QLineEdit::editingFinished,
          [=] { SConfig::GetInstance().m_NANDPath = m_nand_edit->text().toStdString(); });
  QPushButton* nand_open = new QPushButton;
  connect(nand_open, &QPushButton::clicked, this, &PathPane::BrowseWiiNAND);
  layout->addWidget(new QLabel(tr("Wii NAND Root")), 3, 0);
  layout->addWidget(m_nand_edit, 3, 1);
  layout->addWidget(nand_open, 3, 2);

  return layout;
}

void PathPane::RemovePath()
{
  auto item = m_path_list->currentItem();
  if (!item)
    return;
  Settings::Instance().RemovePath(item->text());
}
