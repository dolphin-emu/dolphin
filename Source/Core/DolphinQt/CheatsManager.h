// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"

class ARCodeWidget;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSplitter;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;
struct Result;

namespace Core
{
enum class State;
}

namespace UICommon
{
class GameFile;
}

class CheatsManager : public QDialog
{
  Q_OBJECT
public:
  explicit CheatsManager(QWidget* parent = nullptr);
  ~CheatsManager();

private:
  QWidget* CreateCheatSearch();
  void CreateWidgets();
  void ConnectWidgets();
  void OnStateChanged(Core::State state);

  size_t GetTypeSize() const;
  std::function<bool(u32)> CreateMatchFunction();

  void Reset();
  void NewSearch();
  void NextSearch();
  void Update();
  void GenerateARCode();

  void OnWatchContextMenu();
  void OnMatchContextMenu();
  void OnWatchItemChanged(QTableWidgetItem* item);

  std::vector<Result> m_results;
  std::vector<Result> m_watch;
  std::shared_ptr<const UICommon::GameFile> m_game_file;
  QDialogButtonBox* m_button_box;
  QTabWidget* m_tab_widget = nullptr;

  QWidget* m_cheat_search;
  ARCodeWidget* m_ar_code = nullptr;

  QLabel* m_result_label;
  QTableWidget* m_match_table;
  QTableWidget* m_watch_table;
  QSplitter* m_option_splitter;
  QSplitter* m_table_splitter;
  QComboBox* m_match_length;
  QComboBox* m_match_operation;
  QLineEdit* m_match_value;
  QPushButton* m_match_new;
  QPushButton* m_match_next;
  QPushButton* m_match_refresh;
  QPushButton* m_match_reset;

  QRadioButton* m_match_decimal;
  QRadioButton* m_match_hexadecimal;
  QRadioButton* m_match_octal;
  bool m_updating = false;
};
