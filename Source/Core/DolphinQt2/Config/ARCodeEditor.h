// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ActionReplay.h"

#include <QDialog>

class QDialogButtonBox;
class QLineEdit;
class QTextEdit;

class ARCodeEditor : public QDialog
{
public:
  explicit ARCodeEditor(ActionReplay::ARCode& ar);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void accept() override;

  QLineEdit* m_name_edit;
  QTextEdit* m_code_edit;
  QDialogButtonBox* m_button_box;

  ActionReplay::ARCode& m_code;
};
