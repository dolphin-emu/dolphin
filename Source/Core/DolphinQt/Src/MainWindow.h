#pragma once

#include <QMainWindow>
#include "GameList.h"

// TODO: remove
#include <QtGui>
#include <Core.h>
class DLogWindow;
class DMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	DMainWindow();
	~DMainWindow();

private slots:
	void OnLoadIso();
	void OnRefreshList();

	void OnStartPause();
	void OnStop() {};

	void OnConfigure() {};
	void OnGfxSettings() {};
	void OnSoundSettings() {};
	void OnGCPadSettings() {};
	void OnWiimoteSettings() {};

	void OnShowLogMan(bool show);
	void OnHideMenu(bool hide);

	void OnAbout();

	void OnLogWindowClosed();

	void OnCoreStateChanged(Core::EState state);

private:
	void CreateMenus();
	void CreateToolBars();
	void CreateStatusBar();
	void CreateDockWidgets();

	void StartGame(const std::string& filename);
	std::string RequestBootFilename();
	void DoStartPause();

	void DoStop() {}

	DGameList* gameList;

	QAction* showLogManAct;

	DLogWindow* logWindow;

	QAction* playAction;
	QAction* stopAction;

signals:
	void CoreStateChanged(Core::EState state);
};
