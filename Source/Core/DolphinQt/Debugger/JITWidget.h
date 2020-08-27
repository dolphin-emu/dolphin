// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>
#include <memory>

#include "Common/CommonTypes.h"

class QCloseEvent;
class QShowEvent;
class QSplitter;
class QTextBrowser;
class QTableWidget;
class QPushButton;
class HostDisassembler;

class JITWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit JITWidget(QWidget* parent = nullptr);
  ~JITWidget();

  void Compare(u32 address);

private:
  void Update();
  void CreateWidgets();
  void ConnectWidgets();

  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

  QTableWidget* m_table_widget;
  QTextBrowser* m_ppc_asm_widget;
  QTextBrowser* m_host_asm_widget;
  QSplitter* m_table_splitter;
  QSplitter* m_asm_splitter;
  QPushButton* m_refresh_button;

  std::unique_ptr<HostDisassembler> m_disassembler;
  u32 m_address = 0;
};
