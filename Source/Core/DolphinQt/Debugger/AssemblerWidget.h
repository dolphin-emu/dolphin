// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include "Common/Assembler/GekkoAssembler.h"
#include "Core/Core.h"

class QTabWidget;
class AsmEditor;
class QAction;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTextEdit;
class QToolBar;

namespace Core
{
class System;
}  // namespace Core

class AssemblerWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit AssemblerWidget(QWidget* parent);

  bool ApplicationCloseRequest();

  ~AssemblerWidget();

protected:
  void closeEvent(QCloseEvent*);

private:
  enum class AsmKind
  {
    Raw,
    ActionReplay,
    Gecko,
    GeckoExec,
    GeckoTrampoline
  };
  static constexpr int OUTPUT_BOX_WIDTH = 18;
  void CreateWidgets();
  void ConnectWidgets();

  void OnEditChanged();

  void OnAssemble(std::vector<Common::GekkoAssembler::CodeBlock>* asm_out);
  void OnCopyOutput();
  void OnOpen();
  void OnNew();
  void OnInject();
  void OnSave();
  void OnZoomIn();
  void OnZoomOut();
  void OnZoomReset();
  void OnBaseAddressChanged();
  void OnTabChange(int index);
  QString TabTextForEditor(AsmEditor* editor, bool with_dirty);
  AsmEditor* GetEditor(int idx);
  void NewEditor(const QString& path = QStringLiteral());
  bool SaveEditor(AsmEditor* editor);
  void OnEmulationStateChanged(Core::State state);
  void OnTabClose(int index);
  void CloseTab(int index, AsmEditor* editor);
  int AllocateTabNum();
  void FreeTabNum(int num);
  void UpdateTabText(AsmEditor* editor);
  void DisambiguateTabTitles(AsmEditor* editor);
  void UpdateIcons();
  void ZoomAllEditors(int amount);

  static constexpr int INVALID_EDITOR_NUM = -1;

  Core::System& m_system;

  QTabWidget* m_asm_tabs;
  QPlainTextEdit* m_output_box;
  QComboBox* m_output_type;
  QPushButton* m_copy_output_button;
  QTextEdit* m_error_box;
  QLineEdit* m_address_line;
  QToolBar* m_toolbar;
  QAction* m_open;
  QAction* m_new;
  QAction* m_assemble;
  QAction* m_inject;
  QAction* m_save;

  std::list<int> m_free_editor_nums;
  int m_unnamed_editor_count;
  int m_net_zoom_delta;
  bool m_dark_scheme;
};
