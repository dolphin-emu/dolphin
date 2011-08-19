#include <QtGui>
#include "GameList.h"
#include "MainWindow.h"
#include "LogWindow.h"
#include "Resources.h"

#include "BootManager.h"
#include "Common.h"
#include "ConfigManager.h"
#include "Core.h"


DMainWindow::DMainWindow() : logWindow(NULL)
{
	Resources::Init();

	gameList = new DGameList;
	setCentralWidget(gameList);

	CreateMenus();
	CreateToolBars();
	CreateStatusBar();
	CreateDockWidgets();
	// TODO: read settings

	setWindowIcon(QIcon(Resources::GetDolphinIcon()));
	resize(800, 600);
	setWindowTitle("Dolphin");
	show();

	gameList->ScanForIsos();
}

DMainWindow::~DMainWindow()
{

}

void DMainWindow::CreateMenus()
{
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	QAction* loadIsoAct = fileMenu->addAction(tr("&Load ISO..."));
	fileMenu->addSeparator();
	QAction* refreshListAct = fileMenu->addAction(tr("&Refresh list"));
	fileMenu->addSeparator();
	QAction* exitAct = fileMenu->addAction(tr("&Exit"));

	QMenu* emuMenu = menuBar()->addMenu(tr("&Emulation"));
	QAction* pauseAct = emuMenu->addAction(tr("&Start/Pause"));
	QAction* stopAct = emuMenu->addAction(tr("&Stop"));

	QMenu* optionsMenu = menuBar()->addMenu(tr("&Options"));
	QAction* configureAct = optionsMenu->addAction(tr("Configure"));
	optionsMenu->addSeparator();
	QAction* gfxSettingsAct = optionsMenu->addAction(tr("Graphics settings"));
	QAction* soundSettingsAct = optionsMenu->addAction(tr("Sound settings"));
	QAction* gcpadSettingsAct = optionsMenu->addAction(tr("Controller settings"));
	QAction* wiimoteSettingsAct = optionsMenu->addAction(tr("Wiimote settings"));

	QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
	showLogManAct = viewMenu->addAction(tr("Show &log manager"));
	showLogManAct->setCheckable(true);
	showLogManAct->setChecked(SConfig::GetInstance().m_InterfaceLogWindow);
	viewMenu->addSeparator();
	QAction* hideMenuAct = viewMenu->addAction(tr("Hide menu"));
	hideMenuAct->setCheckable(true);
	hideMenuAct->setChecked(false); // TODO: Read this from config

	QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
	QAction* aboutAct = helpMenu->addAction(tr("About..."));

	connect(loadIsoAct, SIGNAL(triggered()), this, SLOT(OnLoadIso()));
	connect(refreshListAct, SIGNAL(triggered()), this, SLOT(OnRefreshList()));
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	connect(pauseAct, SIGNAL(triggered()), this, SLOT(OnStartPause()));
	connect(stopAct, SIGNAL(triggered()), this, SLOT(OnStop()));

	connect(showLogManAct, SIGNAL(toggled(bool)), this, SLOT(OnShowLogMan(bool)));
	connect(hideMenuAct, SIGNAL(toggled(bool)), this, SLOT(OnHideMenu(bool)));

	connect(configureAct, SIGNAL(triggered()), this, SLOT(OnConfigure()));
	connect(gfxSettingsAct, SIGNAL(triggered()), this, SLOT(OnGfxSettings()));
	connect(soundSettingsAct, SIGNAL(triggered()), this, SLOT(OnSoundSettings()));
	connect(gcpadSettingsAct, SIGNAL(triggered()), this, SLOT(OnGCPadSettings()));
	connect(wiimoteSettingsAct, SIGNAL(triggered()), this, SLOT(OnWiimoteSettings()));

	connect(aboutAct, SIGNAL(triggered()), this, SLOT(OnAbout()));
}

void DMainWindow::CreateToolBars()
{
	QToolBar* toolBar = addToolBar(tr("File"));
	toolBar->setIconSize(QSize(24, 24));
	toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

	QAction* openAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_OPEN)), tr("Open"));
	QAction* refreshAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_REFRESH)), tr("Refresh"));
	QAction* browseAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_BROWSE)), tr("Browse"));
	toolBar->addSeparator();

	QAction* playAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLAY)), tr("Play"));
	QAction* stopAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_STOP)), tr("Stop"));
	QAction* fscrAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_FULLSCREEN)), tr("FullScr"));
	QAction* scrshotAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_SCREENSHOT)), tr("ScrShot"));
	toolBar->addSeparator();

	QAction* configAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_CONFIGURE)), tr("Config"));
	QAction* gfxAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_GFX)), tr("Graphics"));
	QAction* soundAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_DSP)), tr("Sound"));
	QAction* padAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_GCPAD)), tr("GC Pad"));
	QAction* wiimoteAction = toolBar->addAction(QIcon(Resources::GetToolbarPixmap(Resources::TOOLBAR_PLUGIN_WIIMOTE)), tr("Wiimote"));


	connect(playAction, SIGNAL(triggered()), this, SLOT(OnStartPause()));
}

void DMainWindow::CreateStatusBar()
{
	statusBar()->showMessage(tr("Ready"));
}

void DMainWindow::CreateDockWidgets()
{
	logWindow = new DLogWindow(tr("Logging"), this);
	connect(logWindow, SIGNAL(Closed()), this, SLOT(OnLogWindowClosed()));

	addDockWidget(Qt::RightDockWidgetArea, logWindow);
	logWindow->setVisible(SConfig::GetInstance().m_InterfaceLogWindow);
}

void DMainWindow::BootGame(const std::string& filename)
{
    std::string bootfile = filename;
    SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

    if (Core::GetState() != Core::CORE_UNINITIALIZED)
        return;

    // Start filename if non empty.
    // Start the selected ISO, or try one of the saved paths.
    // If all that fails, ask to add a dir and don't boot
    if (bootfile.empty())
    {
/*        if (m_GameListCtrl->GetSelectedISO() != NULL)
        {
            if (m_GameListCtrl->GetSelectedISO()->IsValid())
                bootfile = m_GameListCtrl->GetSelectedISO()->GetFileName();
        }
        else */if (!StartUp.m_strDefaultGCM.empty()
                &&  File::Exists(StartUp.m_strDefaultGCM.c_str()))
            bootfile = StartUp.m_strDefaultGCM;
        else
        {
            if (!SConfig::GetInstance().m_LastFilename.empty()
                    && File::Exists((SConfig::GetInstance().m_LastFilename.c_str())))
                bootfile = SConfig::GetInstance().m_LastFilename;
            else
            {
//                m_GameListCtrl->BrowseForDirectory();
                return;
            }
        }
    }
    if (!bootfile.empty())
        StartGame(bootfile);
}

void DMainWindow::StartGame(const std::string& filename)
{
	// TODO: Disable play toolbar action, replace with pause
	gameList->setVisible(false);

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
//		m_RenderParent = m_Panel;
//		m_RenderFrame = this;
	}
	else
	{
/*		wxPoint position(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
						SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos);
		wxSize size(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth,
					SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight);*/

		// TODO: Create render window at position with size, title Dolphin
		// TODO: Connect close to OnRenderParentClose
		// TODO: render window should be dockable into the main window => render to main window
	}
	if (!BootManager::BootCore(filename))
	{
		// Destroy the renderer frame when not rendering to main
/*		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_RenderFrame->Destroy();
		m_RenderParent = NULL;
		UpdateGUI();*/
	}
//	else
	{
		// TODO: Disable screensaver!

		// TODO: Fullscreen
		//DoFullscreen(SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);

		// TODO: Set focus to render window
	}
}

void DMainWindow::OnStartPause()
{
	if (gameList->GetSelectedFilename().length() == 0)
	{
		SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;
		std::string filename;

		if (!StartUp.m_strDefaultGCM.empty() && File::Exists(StartUp.m_strDefaultGCM.c_str()))
			filename = StartUp.m_strDefaultGCM;
		else
		{
			if (!SConfig::GetInstance().m_LastFilename.empty() && File::Exists(SConfig::GetInstance().m_LastFilename.c_str()))
				filename = SConfig::GetInstance().m_LastFilename;
			else
			{
				// TODO
//				m_GameListCtrl->BrowseForDirectory();
				return;
			}
		}
		if (!filename.empty())
			StartGame(filename);
	}
	else
	{
		StartGame(gameList->GetSelectedFilename().toStdString());
	}
}

void DMainWindow::OnLoadIso()
{
	QString selection = QFileDialog::getOpenFileName(this, tr("Select an ISO"));
	if(selection.length()) QMessageBox::information(this, tr("Notice"), selection, QMessageBox::Ok);
}

void DMainWindow::OnRefreshList()
{
	gameList->ScanForIsos();
}

void DMainWindow::OnShowLogMan(bool show)
{
	if (show)
	{
		logWindow->setVisible(true);
		SConfig::GetInstance().m_InterfaceLogWindow = true;
	}
	else if (logWindow)
	{
		logWindow->setVisible(false);
		SConfig::GetInstance().m_InterfaceLogWindow = false;
	}
}

void DMainWindow::OnHideMenu(bool hide)
{
	menuBar()->setVisible(!hide);
}

void DMainWindow::OnAbout()
{
	QMessageBox::about(this, tr("About Dolphin"),
						tr(	"Dolphin SVN revision %1\n"
							"Copyright (c) 2003-2010+ Dolphin Team\n"
							"\n"
							"Dolphin is a Gamecube/Wii emulator, which was\n"
							"originally written by F|RES and ector.\n"
							"Today Dolphin is an open source project with many\n"
							"contributors, too many to list.\n"
							"If interested, just go check out the project page at\n"
							"http://code.google.com/p/dolphin-emu/ .\n"
							"\n"
							"Special thanks to Bushing, Costis, CrowTRobo,\n"
							"Marcan, Segher, Titanik, or9 and Hotquik for their\n"
							"reverse engineering and docs/demos.\n"
							"\n"
							"Big thanks to Gilles Mouchard whose Microlib PPC\n"
							"emulator gave our development a kickstart.\n"
							"\n"
							"Thanks to Frank Wille for his PowerPC disassembler,\n"
							"which or9 and we modified to include Gekko specifics.\n"
							"\n"
							"Thanks to hcs/destop for their GC ADPCM decoder.\n"
							"\n"
							"We are not affiliated with Nintendo in any way.\n"
							"Gamecube and Wii are trademarks of Nintendo.\n"
							"The emulator is for educational purposes only\n"
							"and should not be used to play games you do\n"
							"not legally own.").arg(svn_rev_str));
}

void DMainWindow::OnLogWindowClosed()
{
	// this calls OnShowLogMan(false)
	showLogManAct->setChecked(false);
}
