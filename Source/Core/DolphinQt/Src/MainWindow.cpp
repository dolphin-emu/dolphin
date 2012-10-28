#include <QtGui>
#include "GameList.h"
#include "MainWindow.h"
#include "LogWindow.h"
#include "Util/Resources.h"
#include "Util/Util.h"
#include "Config/ConfigMain.h"

#include "BootManager.h"
#include "Common.h"
#include "ConfigManager.h"
#include "Core.h"

// TODO: Consider integrating Qt in to entire project through the use of Common features.

DMainWindow::DMainWindow() : logWindow(NULL), renderWindow(NULL), is_stopping(false)
{
	Resources::Init();

	QSettings ui_settings("Dolphin Team", "Dolphin");
	centralLayout = new DLayoutWidgetV;
	DGameBrowser::Style gameBrowserStyle = (DGameBrowser::Style)ui_settings.value("gameList/layout", DGameBrowser::Style_List).toInt();
	centralLayout->addWidget(gameBrowser = new DGameBrowser(gameBrowserStyle, this));
	setCentralWidget(centralLayout);

	CreateMenus();
	CreateToolBars();
	CreateStatusBar();
	CreateDockWidgets();
	// TODO: read settings

	setWindowIcon(Resources::GetIcon(Resources::DOLPHIN_LOGO));
	setWindowTitle("Dolphin");

	if (false == restoreGeometry(ui_settings.value("main/geometry").toByteArray()))
	{
		// 55% of the window contents are in the upper screen half, 45% in the lower half
		QDesktopWidget* desktop = ((QApplication*)QApplication::instance())->desktop();
		QRect screenRect = desktop->screenGeometry(this);
		int x, y, w, h;
		w = screenRect.width() * 2 / 3;
		h = screenRect.height() / 2;
		x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
		y = (screenRect.y() + screenRect.height()) / 2 - h * 55 / 100;
		setGeometry(x, y, w, h);
	}
	restoreState(ui_settings.value("main/state").toByteArray());
	setMinimumSize(400, 300);
	show();

	// idea: On first start, show a configuration wizard? ;)
	dialog = new DConfigDialog(this);
	connect(dialog, SIGNAL(IsoPathsChanged()), this, SLOT(OnRefreshList()));

	connect(gameBrowser, SIGNAL(StartGame()), this, SLOT(OnStartPause()));
	connect(this, SIGNAL(StartIsoScanning()), this, SLOT(OnRefreshList()));

	emit CoreStateChanged(Core::CORE_UNINITIALIZED); // update GUI items
	emit StartIsoScanning();
}

DMainWindow::~DMainWindow()
{

}

void DMainWindow::closeEvent(QCloseEvent* ev)
{
	DoStop();

	// Save UI configuration
	SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX = x();
	SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY = y();
	SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth = width();
	SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight = height();
	SConfig::GetInstance().m_InterfaceLogWindow = logWindow->isVisible();
	SConfig::GetInstance().m_InterfaceLogConfigWindow = logSettings->isVisible();
	SConfig::GetInstance().SaveSettings();

	QSettings ui_settings("Dolphin Team", "Dolphin");
	ui_settings.setValue("main/geometry", saveGeometry());
	ui_settings.setValue("main/state", saveState());
	ui_settings.setValue("gameList/Layout", gameBrowser->GetStyle());

	QWidget::closeEvent(ev);
}

void DMainWindow::CreateMenus()
{
	QSettings settings( QSTRING_STR(File::GetUserPath(F_DOLPHINCONFIG_IDX)), QSettings::IniFormat );
	// File
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	QAction* loadIsoAct = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open ..."));
	QAction* browseAct = fileMenu->addAction(tr("&Add Folder ..."));
	fileMenu->addSeparator();
	QAction* refreshListAct = fileMenu->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("&Refresh list"));
	fileMenu->addSeparator();
	QAction* exitAct = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogCloseButton), tr("&Exit")); // TODO: Other icon

	// Emulation
	QMenu* emuMenu = menuBar()->addMenu(tr("&Emulation"));
	QAction* pauseAct = emuMenu->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("&Start/Pause"));
	QAction* stopAct = emuMenu->addAction(style()->standardIcon(QStyle::SP_MediaStop), tr("&Stop"));
	emuMenu->addSeparator();
	QAction* fullscreenAct = emuMenu->addAction(tr("Toggle &Fullscreen"));
	emuMenu->addSeparator();
	QMenu* savestateMenu = emuMenu->addMenu(tr("Save State"));
	QMenu* loadstateMenu = emuMenu->addMenu(tr("Load State"));
	// TODO: Add state menu items .. rather than slots, we should keep a list of recently saved/loaded state files

	// Options
	QMenu* optionsMenu = menuBar()->addMenu(tr("&Options"));
	QAction* configureAct = optionsMenu->addAction(Resources::GetIcon(Resources::TOOLBAR_CONFIGURE), tr("&General Settings ..."));
	QAction* hotkeyAct = optionsMenu->addAction(Resources::GetIcon(Resources::HOTKEYS), tr("&Hotkeys"));
	optionsMenu->addSeparator();
	QAction* gfxSettingsAct = optionsMenu->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_GFX), tr("&Graphics Settings ..."));
	QAction* soundSettingsAct = optionsMenu->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_DSP), tr("&Sound Settings ..."));
	QAction* gcpadSettingsAct = optionsMenu->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_GCPAD), tr("&Controller Settings ..."));
	QAction* wiimoteSettingsAct = optionsMenu->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_WIIMOTE), tr("&Wiimote Settings ..."));

	// Tools
	QMenu* toolsMenu = menuBar()->addMenu(tr("Tools"));
	QAction* memcardsAct = toolsMenu->addAction(Resources::GetIcon(Resources::MEMCARD), tr("Memory Cards ..."));
	QAction* wiisavesAct = toolsMenu->addAction(tr("Import Wii Save Games ..."));
	toolsMenu->addSeparator();
	QAction* installWadAct = toolsMenu->addAction(tr("Install WAD ..."));
	QAction* loadWiiAct = toolsMenu->addAction(tr("Load Wii Menu"));
	toolsMenu->addSeparator();
	QAction* cheatsAct = toolsMenu->addAction(tr("Cheats ..."));

	// View
	QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
	showLogManAct = viewMenu->addAction(tr("Show &Log Manager"));
	showLogManAct->setCheckable(true);
	showLogManAct->setChecked(SConfig::GetInstance().m_InterfaceLogWindow);
	showLogSettingsAct = viewMenu->addAction(tr("Show Log &Settings"));
	showLogSettingsAct->setCheckable(true);
	showLogSettingsAct->setChecked(SConfig::GetInstance().m_InterfaceLogConfigWindow);
	viewMenu->addSeparator();
	QAction* showToolbarAct = viewMenu->addAction(tr("Show Toolbar"));
	showToolbarAct->setCheckable(true);
	showToolbarAct->setChecked(settings.value("Interface/ShowToolbar",true).toBool());
	viewMenu->addSeparator();

	QMenu* gameBrowserStyleMenu = viewMenu->addMenu(tr("Game Browser Style"));
	QAction* gameBrowserAsListAct = gameBrowserStyleMenu->addAction(tr("List"));
	QAction* gameBrowserAsGridAct = gameBrowserStyleMenu->addAction(tr("Grid"));
	gameBrowserAsListAct->setCheckable(true);
	gameBrowserAsGridAct->setCheckable(true);
	gameBrowserAsListAct->setChecked(gameBrowser->GetStyle() == DGameBrowser::Style_List);
	gameBrowserAsGridAct->setChecked(gameBrowser->GetStyle() == DGameBrowser::Style_Grid);
	QActionGroup* gameBrowserStyleGroup = new QActionGroup(this);
	gameBrowserStyleGroup->addAction(gameBrowserAsListAct);
	gameBrowserStyleGroup ->addAction(gameBrowserAsGridAct);

	QAction* expertModeAct = viewMenu->addAction(tr("Expert Mode"));

	// Help
	QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
	QAction* websiteAct = helpMenu->addAction(tr("Dolphin Homepage ..."));
	QAction* reportIssueAct = helpMenu->addAction(tr("Report an issue ..."));
	helpMenu->addSeparator();
	QAction* aboutAct = helpMenu->addAction(Resources::GetIcon(Resources::DOLPHIN_LOGO), tr("About ..."));

	if (1/*expertModeEnabled*/)
	{
		installWadAct->setVisible(false);

		showLogManAct->setVisible(false);
		showLogSettingsAct->setVisible(false);
	}

	// Events
	connect(loadIsoAct, SIGNAL(triggered()), this, SLOT(OnLoadIso()));
	connect(browseAct, SIGNAL(triggered()), this, SLOT(OnBrowseIso()));
	connect(refreshListAct, SIGNAL(triggered()), this, SLOT(OnRefreshList()));
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	connect(pauseAct, SIGNAL(triggered()), this, SLOT(OnStartPause()));
	connect(stopAct, SIGNAL(triggered()), this, SLOT(OnStop()));

	connect(showLogManAct, SIGNAL(toggled(bool)), this, SLOT(OnShowLogMan(bool)));
	connect(showLogSettingsAct, SIGNAL(toggled(bool)), this, SLOT(OnShowLogSettings(bool)));
	connect(showToolbarAct, SIGNAL(toggled(bool)), this, SLOT(OnShowToolbar(bool)));
	connect(gameBrowserAsListAct, SIGNAL(triggered()), this, SLOT(OnSwitchToGameList()));
	connect(gameBrowserAsGridAct, SIGNAL(triggered()), this, SLOT(OnSwitchToGameGrid()));

	connect(configureAct, SIGNAL(triggered()), this, SLOT(OnConfigure()));
	connect(hotkeyAct, SIGNAL(triggered()), this, SLOT(OnConfigureHotkeys()));
	connect(gfxSettingsAct, SIGNAL(triggered()), this, SLOT(OnGfxSettings()));
	connect(soundSettingsAct, SIGNAL(triggered()), this, SLOT(OnSoundSettings()));
	connect(gcpadSettingsAct, SIGNAL(triggered()), this, SLOT(OnGCPadSettings()));
	connect(wiimoteSettingsAct, SIGNAL(triggered()), this, SLOT(OnWiimoteSettings()));

	connect(reportIssueAct, SIGNAL(triggered()), this, SLOT(OnReportIssue()));
	connect(aboutAct, SIGNAL(triggered()), this, SLOT(OnAbout()));


	connect(this, SIGNAL(CoreStateChanged(Core::EState)), this, SLOT(OnCoreStateChanged(Core::EState)));
}

void DMainWindow::CreateToolBars()
{
	toolBar = addToolBar(tr("Main Toolbar"));
	toolBar->setIconSize(style()->standardIcon(QStyle::SP_DialogOpenButton).actualSize(QSize(99,99)));
	toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	openAction = toolBar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open"));
	refreshAction = toolBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Refresh"));
	toolBar->addSeparator();

	playAction = toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Play"));
	stopAction = toolBar->addAction(style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"));
	toolBar->addSeparator();

	QAction* configAction = toolBar->addAction(Resources::GetIcon(Resources::TOOLBAR_CONFIGURE), tr("Config"));
	QAction* gfxAction = toolBar->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_GFX), tr("Graphics"));
	QAction* soundAction = toolBar->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_DSP), tr("Sound"));
	QAction* padAction = toolBar->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_GCPAD), tr("GC Pad"));
	QAction* wiimoteAction = toolBar->addAction(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_WIIMOTE), tr("Wiimote"));

	connect(openAction, SIGNAL(triggered()), this, SLOT(OnLoadIso()));
	connect(refreshAction, SIGNAL(triggered()), this, SLOT(OnRefreshList()));
	connect(playAction, SIGNAL(triggered()), this, SLOT(OnStartPause()));
	connect(stopAction, SIGNAL(triggered()), this, SLOT(OnStop()));

	// TODO: Fullscreen
	// TODO: Screenshot

	connect(configAction, SIGNAL(triggered()), this, SLOT(OnConfigure()));
	connect(gfxAction, SIGNAL(triggered()), this, SLOT(OnGfxSettings()));
	connect(soundAction, SIGNAL(triggered()), this, SLOT(OnSoundSettings()));
	connect(padAction, SIGNAL(triggered()), this, SLOT(OnGCPadSettings()));
	connect(wiimoteAction, SIGNAL(triggered()), this, SLOT(OnWiimoteSettings()));
}

void DMainWindow::CreateStatusBar()
{
//	statusBar()->showMessage(tr("Ready"));
}

void DMainWindow::CreateDockWidgets()
{
	logWindow = new DLogWindow(this);
	connect(logWindow, SIGNAL(Closed()), this, SLOT(OnLogWindowClosed()));

	logSettings = new DLogSettingsDock(this);
	connect(logSettings, SIGNAL(Closed()), this, SLOT(OnLogSettingsWindowClosed()));


	addDockWidget(Qt::RightDockWidgetArea, logWindow);
	logWindow->setVisible(SConfig::GetInstance().m_InterfaceLogWindow);

	addDockWidget(Qt::RightDockWidgetArea, logSettings);
	logSettings->setVisible(SConfig::GetInstance().m_InterfaceLogConfigWindow);
}
