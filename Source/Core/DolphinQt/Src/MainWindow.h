#pragma once

#include <QMainWindow>
#include "GameList.h"
#include "RenderWindow.h"

// TODO: remove
#include <QtGui>
#include <Core.h>
class DLogWindow;
class DLogSettingsDock;
class DMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	DMainWindow();
	~DMainWindow();

	DRenderWindow* GetRenderWindow() const { return renderWindow; }

private slots:
	void OnLoadIso();
	void OnRefreshList();

	void OnStartPause();
	void OnStop();

	void OnConfigure() {};
	void OnGfxSettings() {};
	void OnSoundSettings() {};
	void OnGCPadSettings() {};
	void OnWiimoteSettings() {};

	void OnShowLogMan(bool show);
	void OnShowLogSettings(bool show);
	void OnHideMenu(bool hide);

	void OnAbout();

	void OnLogWindowClosed();
	void OnLogSettingsWindowClosed();

	void OnCoreStateChanged(Core::EState state);

private:
	void CreateMenus();
	void CreateToolBars();
	void CreateStatusBar();
	void CreateDockWidgets();

	void StartGame(const std::string& filename);
	std::string RequestBootFilename();
	void DoStartPause();
	void DoStop();

	void closeEvent(QCloseEvent*);

	DGameList* gameList;

	QAction* showLogManAct;
	QAction* showLogSettingsAct;

	DLogWindow* logWindow;
	DLogSettingsDock* logSettings;

	QAction* playAction;
	QAction* stopAction;

	DRenderWindow* renderWindow;

signals:
	void CoreStateChanged(Core::EState state);
};
