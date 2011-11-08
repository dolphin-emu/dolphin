#pragma once

#include <QMainWindow>
#include "Config/ConfigMain.h"
#include "RenderWindow.h"
#include "Util.h"

#include <Core.h>
class DLogWindow;
class DLogSettingsDock;
class DGameBrowser;
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

	void OnConfigure();
	void OnConfigureHotkeys() {};
	void OnGfxSettings();
	void OnSoundSettings();
	void OnGCPadSettings();
	void OnWiimoteSettings();

	void OnShowLogMan(bool show);
	void OnShowLogSettings(bool show);
	void OnHideMenu(bool hide);
	void OnSwitchToGameList();
	void OnSwitchToGameGrid();

	void OnReportIssue();
	void OnAbout();

	void OnLogWindowClosed();
	void OnLogSettingsWindowClosed();

	void OnCoreStateChanged(Core::EState state);

private:
	void CreateMenus();
	void CreateToolBars();
	void CreateStatusBar();
	void CreateDockWidgets();

	void OpenConfigDialog(DConfigDialog::InitialConfigItem initialConfigItem);

	void StartGame(const std::string& filename);
	std::string RequestBootFilename();
	void DoStartPause();
	void DoStop();

	void closeEvent(QCloseEvent*);

	// Widgets
	DGameBrowser* gameBrowser;
	DLayoutWidgetV* centralLayout; // Stores gameBrowser OR render widget

	DLogWindow* logWindow;
	DLogSettingsDock* logSettings;

	QWidget* renderWindow;

	// Actions
	QAction* openAction;
	QAction* refreshAction;

	QAction* playAction;
	QAction* stopAction;

	QAction* showLogManAct;
	QAction* showLogSettingsAct;

	// Emulation stopping closes the render window; closing the render window also stops emulation
	// Thus, in order to prevent endless loops, we need this variable.
	bool is_stopping;

signals:
	void CoreStateChanged(Core::EState state);
	void StartIsoScanning();
};
