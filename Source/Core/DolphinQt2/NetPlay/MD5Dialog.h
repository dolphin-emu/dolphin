// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class QVBoxLayout;
class QWidget;

class MD5Dialog : public QDialog
{
  Q_OBJECT
public:
  MD5Dialog(QWidget* parent);

  void show(const QString& title);
  void SetProgress(int pid, int progress);
  void SetResult(int pid, const std::string& md5);

  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  std::map<int, QProgressBar*> m_progress_bars;
  std::map<int, QLabel*> m_status_labels;

  std::string m_last_result;

  QGroupBox* m_progress_box;
  QVBoxLayout* m_progress_layout;
  QVBoxLayout* m_main_layout;
  QLabel* m_check_label;
  QDialogButtonBox* m_button_box;
};
