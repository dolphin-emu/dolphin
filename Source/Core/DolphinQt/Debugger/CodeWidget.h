// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>
#include <QString>

#include "Common/CommonTypes.h"
#include "DolphinQt/Debugger/CodeViewWidget.h"

class BranchWatchDialog;
class QCloseEvent;
class QComboBox;
class QLineEdit;
class QShowEvent;
class QSplitter;
class QListWidget;
class QPushButton;
class QTableWidget;
class QToolButton;

namespace Common
{
	struct Symbol;
}
namespace Core
{
	class System;
}

class CodeWidget : public QDockWidget
{
	Q_OBJECT
public:
	explicit CodeWidget(QWidget* parent = nullptr);
	~CodeWidget();

	void Step();
	void StepOver();
	void StepOut();
	void Skip();
	void ShowPC();
	void SetPC();

	void OnBranchWatchDialog();
	void ToggleBreakpoint();
	void AddBreakpoint();
	void SetAddress(u32 address, CodeViewWidget::SetAddressUpdate update);

	void Update();
	void UpdateSymbols();
signals:
	void BreakpointsChanged();
	void RequestPPCComparison(u32 addr);
	void ShowMemory(u32 address);
	void DoAutoStep(CodeTrace::AutoStop option, std::string reg);

private:
	void CreateWidgets();
	void ConnectWidgets();
	void UpdateCallstack();
	void UpdateFunctionCalls(const Common::Symbol* symbol);
	void UpdateFunctionCallers(const Common::Symbol* symbol);
	void UpdateNotes();

	void OnSearchAddress();
	void OnSearchSymbols();
	void OnSelectSymbol();
	void OnSelectNote();
	void OnSelectCallstack();
	void OnSelectFunctionCallers();
	void OnSelectFunctionCalls();

	void closeEvent(QCloseEvent*) override;
	void showEvent(QShowEvent* event) override;

	Core::System& m_system;

	BranchWatchDialog* m_diff_dialog;
	QPushButton* m_branch_watch_dialog;
	QComboBox* m_search_address;
	QToolButton* m_save_address_btn;
	QToolButton* m_lock_btn;

	QLineEdit* m_search_callstack;
	QListWidget* m_callstack_list;
	QLineEdit* m_search_symbols;
	QListWidget* m_symbols_list;
	QListWidget* m_note_list;
	QLineEdit* m_search_calls;
	QListWidget* m_function_calls_list;
	QLineEdit* m_search_callers;
	QListWidget* m_function_callers_list;
	CodeViewWidget* m_code_view;
	QSplitter* m_box_splitter;
	QSplitter* m_code_splitter;

	QString m_symbol_filter;
};
