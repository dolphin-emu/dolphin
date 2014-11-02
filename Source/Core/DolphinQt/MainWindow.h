// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QMainWindow>

#include "Core/Core.h"

#include "DolphinQt/VideoInterface/RenderWidget.h"

// Predefinitions
namespace Ui
{
class DMainWindow;
}

class DMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit DMainWindow(QWidget* parent_widget = nullptr);
	~DMainWindow();

	// DRenderWidget
	void RenderWidgetSize(int& x_pos, int& y_pos, int& w, int& h);
	bool RenderWidgetHasFocus();
	DRenderWidget* GetRenderWidget() { return m_render_widget.get(); }

signals:
	void CoreStateChanged(Core::EState state);

private slots:
	// Emulation
	void StartGame(const QString filename);
	void OnCoreStateChanged(Core::EState state);

	// Main toolbar
	void on_actOpen_triggered();
	void on_actPlay_triggered();
	void on_actStop_triggered();

	// Help menu
	void on_actWebsite_triggered();
	void on_actOnlineDocs_triggered();
	void on_actGitHub_triggered();
	void on_actSystemInfo_triggered();
	void on_actAbout_triggered();

	// Misc.
	void UpdateIcons();

private:
	std::unique_ptr<Ui::DMainWindow> m_ui;

	// Emulation
	QString RequestBootFilename();
	QString ShowFileDialog();
	void DoStartPause();

	std::unique_ptr<DRenderWidget> m_render_widget;
	bool m_isStopping = false;
};

// Pointer to the only instance of DMainWindow, used by Host_*
extern DMainWindow* g_main_window;
