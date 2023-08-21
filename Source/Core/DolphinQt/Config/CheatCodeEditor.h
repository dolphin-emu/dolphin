// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTextEdit;

namespace ActionReplay
{
struct ARCode;
}

namespace Gecko
{
class GeckoCode;
}

class CheatCodeEditor : public QDialog
{
public:
  explicit CheatCodeEditor(QWidget* parent);

  void SetARCode(ActionReplay::ARCode* code);
  void SetGeckoCode(Gecko::GeckoCode* code);

private:
  void CreateWidgets();
  void ConnectWidgets();

  bool AcceptAR();
  bool AcceptGecko();

  void accept() override;

  QLabel* m_creator_label;
  QLabel* m_notes_label;

  QLineEdit* m_name_edit;
  QLineEdit* m_creator_edit;
  QTextEdit* m_notes_edit;
  QTextEdit* m_code_edit;
  QDialogButtonBox* m_button_box;

  ActionReplay::ARCode* m_ar_code = nullptr;
  Gecko::GeckoCode* m_gecko_code = nullptr;
};
