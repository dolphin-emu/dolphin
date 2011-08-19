#pragma once

#include <QMainWindow>
#include "GameList.h"

// TODO: remove
#include <QtGui>
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

private:
	void CreateMenus();
	void CreateToolBars();
	void CreateStatusBar();
	void CreateDockWidgets();

	void BootGame(const std::string& filename);
	void StartGame(const std::string& filename);

	void DoStop() {}

	DGameList *gameList;

	QAction* showLogManAct;

	DLogWindow *logWindow;
};
