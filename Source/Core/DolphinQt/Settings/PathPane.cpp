// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/PathPane.h"

#include <QCheckBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/Settings.h"

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
      DolphinFileDialog::getExistingDirectory(this, tr("Select a Directory"), QDir::currentPath()));
  if (!dir.isEmpty())
    Settings::Instance().AddPath(dir);
}

void PathPane::BrowseDefaultGame()
{
  QString file = QDir::toNativeSeparators(DolphinFileDialog::getOpenFileName(
      this, tr("Select a Game"), Settings::Instance().GetDefaultGame(),
      QStringLiteral("%1 (*.elf *.dol *.gcm *.iso *.tgc *.wbfs *.ciso *.gcz *.wia *.rvz "
                     "hif_000000.nfs *.wad *.m3u *.json);;%2 (*)")
          .arg(tr("All GC/Wii files"))
          .arg(tr("All Files"))));

  if (!file.isEmpty())
    Settings::Instance().SetDefaultGame(file);
}

void PathPane::BrowseWiiNAND()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Wii NAND Root"), QString::fromStdString(Config::Get(Config::MAIN_FS_PATH))));
  if (!dir.isEmpty())
  {
    m_nand_edit->setText(dir);
    OnNANDPathChanged();
  }
}

void PathPane::BrowseDump()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Dump Path"), QString::fromStdString(Config::Get(Config::MAIN_DUMP_PATH))));
  if (!dir.isEmpty())
  {
    m_dump_edit->setText(dir);
    Config::SetBase(Config::MAIN_DUMP_PATH, dir.toStdString());
  }
}

void PathPane::BrowseLoad()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Load Path"), QString::fromStdString(Config::Get(Config::MAIN_LOAD_PATH))));
  if (!dir.isEmpty())
  {
    m_load_edit->setText(dir);
    Config::SetBase(Config::MAIN_LOAD_PATH, dir.toStdString());
  }
}

void PathPane::BrowseResourcePack()
{
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select Resource Pack Path"),
      QString::fromStdString(Config::Get(Config::MAIN_RESOURCEPACK_PATH))));
  if (!dir.isEmpty())
  {
    m_resource_pack_edit->setText(dir);
    Config::SetBase(Config::MAIN_RESOURCEPACK_PATH, dir.toStdString());
  }
}

void PathPane::BrowseWFS()
{
  const QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select WFS Path"), QString::fromStdString(Config::Get(Config::MAIN_WFS_PATH))));
  if (!dir.isEmpty())
  {
    m_wfs_edit->setText(dir);
    Config::SetBase(Config::MAIN_WFS_PATH, dir.toStdString());
  }
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
  connect(&Settings::Instance(), &Settings::PathAdded, this,
          [this](const QString& dir) { m_path_list->addItem(dir); });
  connect(&Settings::Instance(), &Settings::PathRemoved, this, [this](const QString& dir) {
    auto items = m_path_list->findItems(dir, Qt::MatchExactly);
    for (auto& item : items)
      delete item;
  });
  connect(m_path_list, &QListWidget::itemSelectionChanged, this,
          [this] { m_remove_path->setEnabled(m_path_list->selectedItems().count()); });

  vlayout->addWidget(m_path_list);

  QHBoxLayout* hlayout = new QHBoxLayout;

  hlayout->addStretch();
  QPushButton* add = new NonDefaultQPushButton(tr("Add..."));
  m_remove_path = new NonDefaultQPushButton(tr("Remove"));

  m_remove_path->setEnabled(false);

  auto* recursive_checkbox = new QCheckBox(tr("Search Subfolders"));
  recursive_checkbox->setChecked(Config::Get(Config::MAIN_RECURSIVE_ISO_PATHS));

  auto* auto_checkbox = new QCheckBox(tr("Check for Game List Changes in the Background"));
  auto_checkbox->setChecked(Settings::Instance().IsAutoRefreshEnabled());

  hlayout->addWidget(add);
  hlayout->addWidget(m_remove_path);
  vlayout->addLayout(hlayout);
  vlayout->addWidget(recursive_checkbox);
  vlayout->addWidget(auto_checkbox);

  connect(recursive_checkbox, &QCheckBox::toggled, [](bool checked) {
    Config::SetBase(Config::MAIN_RECURSIVE_ISO_PATHS, checked);
    Settings::Instance().RefreshGameList();
  });

  connect(auto_checkbox, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetAutoRefreshEnabled);

  connect(add, &QPushButton::clicked, this, &PathPane::Browse);
  connect(m_remove_path, &QPushButton::clicked, this, &PathPane::RemovePath);

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
  connect(&Settings::Instance(), &Settings::DefaultGameChanged, this,
          [this](const QString& path) { m_game_edit->setText(path); });
  QPushButton* game_open = new NonDefaultQPushButton(QStringLiteral("..."));
  connect(game_open, &QPushButton::clicked, this, &PathPane::BrowseDefaultGame);
  layout->addWidget(new QLabel(tr("Default ISO:")), 0, 0);
  layout->addWidget(m_game_edit, 0, 1);
  layout->addWidget(game_open, 0, 2);

  m_nand_edit = new QLineEdit(QString::fromStdString(File::GetUserPath(D_WIIROOT_IDX)));
  connect(m_nand_edit, &QLineEdit::editingFinished, this, &PathPane::OnNANDPathChanged);
  QPushButton* nand_open = new NonDefaultQPushButton(QStringLiteral("..."));
  connect(nand_open, &QPushButton::clicked, this, &PathPane::BrowseWiiNAND);
  layout->addWidget(new QLabel(tr("Wii NAND Root:")), 1, 0);
  layout->addWidget(m_nand_edit, 1, 1);
  layout->addWidget(nand_open, 1, 2);

  m_dump_edit = new QLineEdit(QString::fromStdString(File::GetUserPath(D_DUMP_IDX)));
  connect(m_dump_edit, &QLineEdit::editingFinished,
          [this] { Config::SetBase(Config::MAIN_DUMP_PATH, m_dump_edit->text().toStdString()); });
  QPushButton* dump_open = new NonDefaultQPushButton(QStringLiteral("..."));
  connect(dump_open, &QPushButton::clicked, this, &PathPane::BrowseDump);
  layout->addWidget(new QLabel(tr("Dump Path:")), 2, 0);
  layout->addWidget(m_dump_edit, 2, 1);
  layout->addWidget(dump_open, 2, 2);

  m_load_edit = new QLineEdit(QString::fromStdString(File::GetUserPath(D_LOAD_IDX)));
  connect(m_load_edit, &QLineEdit::editingFinished,
          [this] { Config::SetBase(Config::MAIN_LOAD_PATH, m_load_edit->text().toStdString()); });
  QPushButton* load_open = new NonDefaultQPushButton(QStringLiteral("..."));
  connect(load_open, &QPushButton::clicked, this, &PathPane::BrowseLoad);
  layout->addWidget(new QLabel(tr("Load Path:")), 3, 0);
  layout->addWidget(m_load_edit, 3, 1);
  layout->addWidget(load_open, 3, 2);

  m_resource_pack_edit =
      new QLineEdit(QString::fromStdString(File::GetUserPath(D_RESOURCEPACK_IDX)));
  connect(m_resource_pack_edit, &QLineEdit::editingFinished, [this] {
    Config::SetBase(Config::MAIN_RESOURCEPACK_PATH, m_resource_pack_edit->text().toStdString());
  });
  QPushButton* resource_pack_open = new NonDefaultQPushButton(QStringLiteral("..."));
  connect(resource_pack_open, &QPushButton::clicked, this, &PathPane::BrowseResourcePack);
  layout->addWidget(new QLabel(tr("Resource Pack Path:")), 4, 0);
  layout->addWidget(m_resource_pack_edit, 4, 1);
  layout->addWidget(resource_pack_open, 4, 2);

  m_wfs_edit = new QLineEdit(QString::fromStdString(File::GetUserPath(D_WFSROOT_IDX)));
  connect(m_load_edit, &QLineEdit::editingFinished,
          [this] { Config::SetBase(Config::MAIN_WFS_PATH, m_wfs_edit->text().toStdString()); });
  QPushButton* wfs_open = new NonDefaultQPushButton(QStringLiteral("..."));
  connect(wfs_open, &QPushButton::clicked, this, &PathPane::BrowseWFS);
  layout->addWidget(new QLabel(tr("WFS Path:")), 5, 0);
  layout->addWidget(m_wfs_edit, 5, 1);
  layout->addWidget(wfs_open, 5, 2);

  return layout;
}

void PathPane::RemovePath()
{
  auto item = m_path_list->currentItem();
  if (!item)
    return;
  Settings::Instance().RemovePath(item->text());
}
