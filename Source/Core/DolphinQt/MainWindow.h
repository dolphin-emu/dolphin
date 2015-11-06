// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QString>
#include <QToolBar>

#include "DolphinQt/GameList/GameList.h"

class MainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow();
	~MainWindow();

	QWidget* GetRenderWidget() const { return m_render_widget; }

signals:
	void EmulationStarted();
	void EmulationPaused();
	void EmulationStopped();

private slots:
	void Open();
	void Browse();
	void Play();
	void Pause();
	void Stop();

private:
	void MakeMenus();
	void MakeToolBar();
	void MakeStack();
	void MakeGameList();
	void MakeRenderWidget();

	void PopulateToolBar();

	void StartGame(QString path);

	QStackedWidget* m_stack;
	QToolBar* m_tool_bar;
	GameList* m_game_list;
	QWidget* m_render_widget;
};

extern MainWindow* g_main_window;
