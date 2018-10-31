// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  void BrowseSDCard();
  QGroupBox* MakeGameFolderBox();
  QGridLayout* MakePathsLayout();
  void RemovePath();

  void OnSDCardPathChanged();
  void OnNANDPathChanged();

  QListWidget* m_path_list;
  QLineEdit* m_game_edit;
  QLineEdit* m_nand_edit;
  QLineEdit* m_dump_edit;
  QLineEdit* m_sdcard_edit;

  QPushButton* m_remove_path;
};
