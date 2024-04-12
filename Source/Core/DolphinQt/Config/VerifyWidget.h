// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <utility>

#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

namespace Core
{
enum class State;
}
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
  void OnEmulationStateChanged(Core::State state);
  void CreateWidgets();
  std::pair<QCheckBox*, QLineEdit*> AddHashLine(QFormLayout* layout, QString text);
  void ConnectWidgets();

  bool CanVerifyRedump() const;
  void UpdateRedumpEnabled();
  void Verify();
  void SetProblemCellText(int row, int column, QString text);

  std::shared_ptr<DiscIO::Volume> m_volume;
  QTableWidget* m_problems;
  QTextEdit* m_summary_text;
  QFormLayout* m_hash_layout;
  QFormLayout* m_redump_layout;
  QCheckBox* m_crc32_checkbox;
  QCheckBox* m_md5_checkbox;
  QCheckBox* m_sha1_checkbox;
  QCheckBox* m_redump_checkbox;
  QLineEdit* m_crc32_line_edit;
  QLineEdit* m_md5_line_edit;
  QLineEdit* m_sha1_line_edit;
  QLineEdit* m_redump_line_edit;
  QPushButton* m_verify_button;
};
