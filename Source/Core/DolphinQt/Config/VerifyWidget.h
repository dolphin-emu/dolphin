// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

namespace DiscIO
{
class Volume;
}

class VerifyWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit VerifyWidget(std::shared_ptr<DiscIO::Volume> volume);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Verify();
  void SetProblemCellText(int row, int column, QString text);

  std::shared_ptr<DiscIO::Volume> m_volume;
  QTableWidget* m_problems;
  QTextEdit* m_summary_text;
  QPushButton* m_verify_button;
};
