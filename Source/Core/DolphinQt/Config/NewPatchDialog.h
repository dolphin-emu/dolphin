// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QDialog>
#include <QWidget>

namespace PatchEngine
{
struct Patch;
struct PatchEntry;
}  // namespace PatchEngine

class QDialogButtonBox;
class QGroupBox;
class QLineEdit;
class QVBoxLayout;
class QPushButton;

class NewPatchDialog : public QDialog
{
public:
  explicit NewPatchDialog(QWidget* parent, PatchEngine::Patch& patch);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void AddEntry();

  void accept() override;

  QGroupBox* CreateEntry(PatchEngine::PatchEntry& entry);

  QLineEdit* m_name_edit;
  QWidget* m_entry_widget;
  QVBoxLayout* m_entry_layout;
  QPushButton* m_add_button;
  QDialogButtonBox* m_button_box;

  std::vector<QLineEdit*> m_edits;

  PatchEngine::Patch& m_patch;
};
