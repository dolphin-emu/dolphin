// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/Config/Config.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/PathPane.h"

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
  QString dir = QDir::toNativeSeparators(
      QFileDialog::getExistingDirectory(this, tr("Select a Directory"), QDir::currentPath()));
  if (!dir.isEmpty())
    Settings::Instance().AddPath(dir);
}

void PathPane::BrowseDefaultGame()
{
  QString file = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this, tr("Select a Game"), Settings::Instance().GetDefaultGame(),
      tr("All GC/Wii files (*.elf *.dol *.gcm *.iso *.tgc *.wbfs *.ciso *.gcz *.wad *.m3u);;"
         "All Files (*)")));

  if (!file.isEmpty())
    Settings::Instance().SetDefaultGame(file);
}

void PathPane::BrowseWiiNAND()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
      this, tr("Select Wii NAND Root"), QString::fromStdString(Config::Get(Config::MAIN_FS_PATH))));
  if (!dir.isEmpty())
  {
    m_nand_edit->setText(dir);
    OnNANDPathChanged();
  }
}

void PathPane::BrowseDump()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
      this, tr("Select Dump Path"), QString::fromStdString(Config::Get(Config::MAIN_DUMP_PATH))));
  if (!dir.isEmpty())
  {
    m_dump_edit->setText(dir);
    Config::SetBase(Config::MAIN_DUMP_PATH, dir.toStdString());
  }
}

void PathPane::BrowseSDCard()
{
  QString file = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this, tr("Select a SD Card Image"), QString::fromStdString(Config::Get(Config::MAIN_SD_PATH)),
      tr("SD Card Image (*.raw);;"
         "All Files (*)")));
  if (!file.isEmpty())
  {
    m_sdcard_edit->setText(file);
    OnSDCardPathChanged();
  }
}

void PathPane::OnSDCardPathChanged()
{
  Config::SetBase(Config::MAIN_SD_PATH, m_sdcard_edit->text().toStdString());
}

void PathPane::OnNANDPathChanged()
{
  Config::SetBase(Config::MAIN_FS_PATH, m_nand_edit->text().toStdString());
}

QGroupBox* PathPane::MakeGameFolderBox()
{
  QGroupBox* game_box = new QGroupBox(tr("Game Folders"));
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
  connect(m_path_list, &QListWidget::itemSelectionChanged, this,
          [this] { m_remove_path->setEnabled(m_path_list->selectedItems().count()); });

  vlayout->addWidget(m_path_list);

  QHBoxLayout* hlayout = new QHBoxLayout;

  hlayout->addStretch();
  QPushButton* add = new QPushButton(tr("Add..."));
  m_remove_path = new QPushButton(tr("Remove"));

  m_remove_path->setEnabled(false);

  auto* recursive_checkbox = new QCheckBox(tr("Search Subfolders"));
  recursive_checkbox->setChecked(SConfig::GetInstance().m_RecursiveISOFolder);

  auto* auto_checkbox = new QCheckBox(tr("Check for Game List Changes in the Background"));
  auto_checkbox->setChecked(Settings::Instance().IsAutoRefreshEnabled());

  hlayout->addWidget(add);
  hlayout->addWidget(m_remove_path);
  vlayout->addLayout(hlayout);
  vlayout->addWidget(recursive_checkbox);
  vlayout->addWidget(auto_checkbox);

  connect(recursive_checkbox, &QCheckBox::toggled, this, [](bool checked) {
    SConfig::GetInstance().m_RecursiveISOFolder = checked;
    Settings::Instance().RefreshGameList();
  });

  connect(auto_checkbox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetAutoRefreshEnabled);

  connect(add, &QPushButton::clicked, this, &PathPane::Browse);
  connect(m_remove_path, &QPushButton::pressed, this, &PathPane::RemovePath);

  game_box->setLayout(vlayout);
  return game_box;
}

QGridLayout* PathPane::MakePathsLayout()
{
  QGridLayout* layout = new QGridLayout;
  layout->setColumnStretch(1, 1);

  m_game_edit = new QLineEdit(Settings::Instance().GetDefaultGame());
  connect(m_game_edit, &QLineEdit::editingFinished,
          [this] { Settings::Instance().SetDefaultGame(m_game_edit->text()); });
  connect(&Settings::Instance(), &Settings::DefaultGameChanged,
          [this](const QString& path) { m_game_edit->setText(path); });
  QPushButton* game_open = new QPushButton(QStringLiteral("..."));
  connect(game_open, &QPushButton::pressed, this, &PathPane::BrowseDefaultGame);
  layout->addWidget(new QLabel(tr("Default ISO:")), 0, 0);
  layout->addWidget(m_game_edit, 0, 1);
  layout->addWidget(game_open, 0, 2);

  m_nand_edit = new QLineEdit(QString::fromStdString(Config::Get(Config::MAIN_FS_PATH)));
  connect(m_nand_edit, &QLineEdit::editingFinished, this, &PathPane::OnNANDPathChanged);
  QPushButton* nand_open = new QPushButton(QStringLiteral("..."));
  connect(nand_open, &QPushButton::pressed, this, &PathPane::BrowseWiiNAND);
  layout->addWidget(new QLabel(tr("Wii NAND Root:")), 1, 0);
  layout->addWidget(m_nand_edit, 1, 1);
  layout->addWidget(nand_open, 1, 2);

  m_dump_edit = new QLineEdit(QString::fromStdString(Config::Get(Config::MAIN_DUMP_PATH)));
  connect(m_dump_edit, &QLineEdit::editingFinished,
          [=] { Config::SetBase(Config::MAIN_DUMP_PATH, m_dump_edit->text().toStdString()); });
  QPushButton* dump_open = new QPushButton(QStringLiteral("..."));
  connect(dump_open, &QPushButton::pressed, this, &PathPane::BrowseDump);
  layout->addWidget(new QLabel(tr("Dump Path:")), 2, 0);
  layout->addWidget(m_dump_edit, 2, 1);
  layout->addWidget(dump_open, 2, 2);

  m_sdcard_edit = new QLineEdit(QString::fromStdString(Config::Get(Config::MAIN_SD_PATH)));
  connect(m_sdcard_edit, &QLineEdit::editingFinished, this, &PathPane::OnSDCardPathChanged);
  QPushButton* sdcard_open = new QPushButton(QStringLiteral("..."));
  connect(sdcard_open, &QPushButton::pressed, this, &PathPane::BrowseSDCard);
  layout->addWidget(new QLabel(tr("SD Card Path:")), 3, 0);
  layout->addWidget(m_sdcard_edit, 3, 1);
  layout->addWidget(sdcard_open, 3, 2);

  return layout;
}

void PathPane::RemovePath()
{
  auto item = m_path_list->currentItem();
  if (!item)
    return;
  Settings::Instance().RemovePath(item->text());
}
