#pragma once

#include <QMainWindow>
#include "GameList.h"
#include "RenderWindow.h"
#include "Util.h"

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

	QWidget* GetRenderWindow() const { return renderWindow; }

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

	// Stores gameList and ISO scanning progress bar OR render widget
	DLayoutWidgetV* centralLayout;

//	DGameList* gameList;
	DGameTable* gameList;

	QAction* showLogManAct;
	QAction* showLogSettingsAct;

	DLogWindow* logWindow;
	DLogSettingsDock* logSettings;

	QAction* playAction;
	QAction* stopAction;

	QWidget* renderWindow;

	// Emulation stopping closes the render window; closing the render window also stops emulation
	// Thus, in order to prevent endless loops, we need this variable.
	bool is_stopping;

signals:
	void CoreStateChanged(Core::EState state);
	void StartIsoScanning();
};
