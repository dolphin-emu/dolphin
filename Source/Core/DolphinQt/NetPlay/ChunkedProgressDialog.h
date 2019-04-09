// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"

class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class QVBoxLayout;
class QWidget;

class ChunkedProgressDialog : public QDialog
{
  Q_OBJECT
public:
  explicit ChunkedProgressDialog(QWidget* parent);

  void show(const QString& title, u64 data_size, const std::vector<int>& players);
  void SetProgress(int pid, u64 progress);

  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  std::map<int, QProgressBar*> m_progress_bars;
  std::map<int, QLabel*> m_status_labels;
  u64 m_data_size = 0;

  QGroupBox* m_progress_box;
  QVBoxLayout* m_progress_layout;
  QVBoxLayout* m_main_layout;
  QDialogButtonBox* m_button_box;
};
