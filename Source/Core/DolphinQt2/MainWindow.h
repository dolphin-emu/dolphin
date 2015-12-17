// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QString>
#include <QToolBar>

#include "DolphinQt2/RenderWidget.h"
#include "DolphinQt2/GameList/GameList.h"

class MainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow();
	~MainWindow();

signals:
	void EmulationStarted();
	void EmulationPaused();
	void EmulationStopped();

private slots:
	void Open();
	void Browse();
	void Play();
	void Pause();
	bool Stop();
	void ForceStop();
	void FullScreen();
	void ScreenShot();
	void Website();
	void Documentation();
	void GitHub();

private:
	void MakeToolBar();
	void MakeStack();
	void MakeGameList();
	void MakeRenderWidget();

	void MakeMenus();
	void MakeFileMenu();
	void MakeEmulationMenu();
	void MakeMovieMenu();
	void MakeOptionsMenu();
	void MakeToolsMenu();
	void MakeViewMenu();
	void MakeHelpMenu();
	void AddTableColumnsMenu(QMenu* view_menu);
	void AddListTypePicker(QMenu* view_menu);

	void PopulateToolBar();

	void StartGame(QString path);
	void ShowRenderWidget();
	void HideRenderWidget();

	QStackedWidget* m_stack;
	QToolBar* m_tool_bar;
	GameList* m_game_list;
	RenderWidget* m_render_widget;
	bool m_rendering_to_main;
};
