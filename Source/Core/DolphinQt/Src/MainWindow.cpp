#include <QtGui>
#include "GameList.h"
#include "MainWindow.h"
#include "LogWindow.h"
#include "Resources.h"
#include "Util.h"

#include "BootManager.h"
#include "Common.h"
#include "ConfigManager.h"
#include "Core.h"

// TODO: Move this somewhere else...
class DGameListProgressBar : public QProgressBar, public DAbstractProgressBar
{
public:
	DGameListProgressBar(QWidget* parent = NULL) : QProgressBar(parent) {}
	virtual ~DGameListProgressBar() {}

	void SetValue(int value) { setValue(value); }
	void SetRange(int min, int max) { setRange(min, max); }
	void SetLabel(std::string str) { setFormat(QString::fromStdString(str)); } // TODO: Get translation for this one!
	void SetVisible(bool visible) { setVisible(visible); }
};


DMainWindow::DMainWindow() : logWindow(NULL), renderWindow(NULL), is_stopping(false)
{
	Resources::Init();

	// TODO: Add an option to switch between those two ;)
	centralLayout = new DLayoutWidgetV();
	DGameListProgressBar* progBar = new DGameListProgressBar;
	progBar->SetVisible(false);
//	gameList = new DGameList(progBar);
	gameList = new DGameTable(progBar);
	centralLayout->addWidget(gameList);
	centralLayout->addWidget(progBar);
	setCentralWidget(centralLayout);

	CreateMenus();
	CreateToolBars();
	CreateStatusBar();
	CreateDockWidgets();
	// TODO: read settings

	setWindowIcon(QIcon(Resources::GetDolphinIcon()));
	setWindowTitle("Dolphin");

	QSettings ui_settings("Dolphin Team", "Dolphin");
	QByteArray geometry = ui_settings.value("MainGeometry").toByteArray();
	if (geometry.size()) restoreGeometry(geometry);
	else resize(600, 400);
	restoreState(ui_settings.value("MainState").toByteArray());
	setMinimumSize(400, 300);
	show();

	connect(gameList, SIGNAL(StartGame()), this, SLOT(OnStartPause()));
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
	ui_settings.setValue("MainGeometry", saveGeometry());
	ui_settings.setValue("MainState", saveState());

	QWidget::closeEvent(ev);
}

void DMainWindow::CreateMenus()
{
	// File
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	QAction* loadIsoAct = fileMenu->addAction(tr("&Load ISO..."));
	fileMenu->addSeparator();
	QAction* refreshListAct = fileMenu->addAction(tr("&Refresh list"));
	fileMenu->addSeparator();
	QAction* exitAct = fileMenu->addAction(tr("&Exit"));

	// Emulation
	QMenu* emuMenu = menuBar()->addMenu(tr("&Emulation"));
	QAction* pauseAct = emuMenu->addAction(tr("&Start/Pause"));
	QAction* stopAct = emuMenu->addAction(tr("&Stop"));

	// Options
	QMenu* optionsMenu = menuBar()->addMenu(tr("&Options"));
	QAction* configureAct = optionsMenu->addAction(tr("Configure"));
	optionsMenu->addSeparator();
	QAction* gfxSettingsAct = optionsMenu->addAction(tr("Graphics settings"));
	QAction* soundSettingsAct = optionsMenu->addAction(tr("Sound settings"));
	QAction* gcpadSettingsAct = optionsMenu->addAction(tr("Controller settings"));
	QAction* wiimoteSettingsAct = optionsMenu->addAction(tr("Wiimote settings"));

	// View
	QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
	showLogManAct = viewMenu->addAction(tr("Show &log manager"));
	showLogManAct->setCheckable(true);
	showLogManAct->setChecked(SConfig::GetInstance().m_InterfaceLogWindow);
	showLogSettingsAct = viewMenu->addAction(tr("Show log &settings"));
	showLogSettingsAct->setCheckable(true);
	showLogSettingsAct->setChecked(SConfig::GetInstance().m_InterfaceLogConfigWindow);
	viewMenu->addSeparator();
	QAction* hideMenuAct = viewMenu->addAction(tr("Hide menu"));
	hideMenuAct->setCheckable(true);
	hideMenuAct->setChecked(false); // TODO: Read this from config

	// Help
	QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
	QAction* aboutAct = helpMenu->addAction(tr("About..."));


	// Events
	connect(loadIsoAct, SIGNAL(triggered()), this, SLOT(OnLoadIso()));
	connect(refreshListAct, SIGNAL(triggered()), this, SLOT(OnRefreshList()));
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	connect(pauseAct, SIGNAL(triggered()), this, SLOT(OnStartPause()));
	connect(stopAct, SIGNAL(triggered()), this, SLOT(OnStop()));

	connect(showLogManAct, SIGNAL(toggled(bool)), this, SLOT(OnShowLogMan(bool)));
	connect(showLogSettingsAct, SIGNAL(toggled(bool)), this, SLOT(OnShowLogSettings(bool)));
	connect(hideMenuAct, SIGNAL(toggled(bool)), this, SLOT(OnHideMenu(bool)));

	connect(configureAct, SIGNAL(triggered()), this, SLOT(OnConfigure()));
	connect(gfxSettingsAct, SIGNAL(triggered()), this, SLOT(OnGfxSettings()));
	connect(soundSettingsAct, SIGNAL(triggered()), this, SLOT(OnSoundSettings()));
	connect(gcpadSettingsAct, SIGNAL(triggered()), this, SLOT(OnGCPadSettings()));
	connect(wiimoteSettingsAct, SIGNAL(triggered()), this, SLOT(OnWiimoteSettings()));

	connect(aboutAct, SIGNAL(triggered()), this, SLOT(OnAbout()));


	connect(this, SIGNAL(CoreStateChanged(Core::EState)), this, SLOT(OnCoreStateChanged(Core::EState)));
}

void DMainWindow::CreateToolBars()
{
	QToolBar* toolBar = addToolBar(tr("File"));
	toolBar->setIconSize(QSize(24, 24));
	toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

	QAction* openAction = toolBar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open"));
	QAction* refreshAction = toolBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Refresh"));
	QAction* browseAction = toolBar->addAction(style()->standardIcon(QStyle::SP_DirIcon), tr("Browse"));
	toolBar->addSeparator();

	playAction = toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Play"));
	stopAction = toolBar->addAction(style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"));
	QAction* fscrAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_FULLSCREEN)), tr("FullScr"));
	QAction* scrshotAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_SCREENSHOT)), tr("ScrShot"));
	toolBar->addSeparator();

	QAction* configAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_CONFIGURE)), tr("Config"));
	QAction* gfxAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_GFX)), tr("Graphics"));
	QAction* soundAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_DSP)), tr("Sound"));
	QAction* padAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_GCPAD)), tr("GC Pad"));
	QAction* wiimoteAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_WIIMOTE)), tr("Wiimote"));


	connect(refreshAction, SIGNAL(triggered()), this, SLOT(OnRefreshList()));
	connect(playAction, SIGNAL(triggered()), this, SLOT(OnStartPause()));
	connect(stopAction, SIGNAL(triggered()), this, SLOT(OnStop()));
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
