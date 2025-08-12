// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigText;
class QGridLayout;
class QGroupBox;
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

  QListWidget* m_path_list;
  ConfigText* m_game_edit;
  ConfigText* m_nand_edit;
  ConfigText* m_dump_edit;
  ConfigText* m_load_edit;
  ConfigText* m_resource_pack_edit;
  ConfigText* m_wfs_edit;

  QPushButton* m_remove_path;
};
