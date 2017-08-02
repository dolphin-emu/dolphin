// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QGridLayout;
class QGroupBox;
class QLineEdit;
class QListWidget;

class PathPane final : public QWidget
{
public:
  explicit PathPane(QWidget* parent = nullptr);

private:
  void Browse();
  void BrowseDefaultGame();
  void BrowseWiiNAND();
  QGroupBox* MakeGameFolderBox();
  QGridLayout* MakePathsLayout();
  void RemovePath();

  QListWidget* m_path_list;
  QLineEdit* m_game_edit;
  QLineEdit* m_nand_edit;
};
