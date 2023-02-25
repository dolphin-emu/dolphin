// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QGridLayout;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QPushButton;

class PathPane final : public QWidget
{
  Q_OBJECT
public:
  explicit PathPane(QWidget* parent = nullptr);

private:
  void Browse();
  void BrowseDefaultGame();
  void BrowseWiiNAND();
  void BrowseDump();
  void BrowseLoad();
  void BrowseResourcePack();
  void BrowseWFS();
  QGroupBox* MakeGameFolderBox();
  QGridLayout* MakePathsLayout();
  void RemovePath();

  void OnNANDPathChanged();

  QListWidget* m_path_list;
  QLineEdit* m_game_edit;
  QLineEdit* m_nand_edit;
  QLineEdit* m_dump_edit;
  QLineEdit* m_load_edit;
  QLineEdit* m_resource_pack_edit;
  QLineEdit* m_wfs_edit;

  QPushButton* m_remove_path;
};
