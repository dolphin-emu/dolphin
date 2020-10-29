// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QDialog>
#include <QWidget>

namespace ScriptEngine
{
struct ScriptTarget;
}  // namespace ScriptEngine

class QDialogButtonBox;
class QLineEdit;

class NewScriptDialog : public QDialog
{
public:
  explicit NewScriptDialog(QWidget* parent, ScriptEngine::ScriptTarget& script);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void accept() override;

  QLineEdit* m_file_path_edit;
  QDialogButtonBox* m_button_box;

  ScriptEngine::ScriptTarget& m_script;
};
