#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QTabBar>
#include "ConfigGfx.h"
#include "Util.h"

#include "ConfigManager.h"
#include "VideoBackendBase.h"
#include "VideoConfig.h"


QWidget* DConfigGfx::CreateGeneralTabWidget()
{
	QWidget* tab = new QWidget;

	// Widgets
	cbBackend = new QComboBox;
	for (std::vector<VideoBackend*>::iterator it = g_available_video_backends.begin(); it != g_available_video_backends.end(); ++it)
		cbBackend->addItem(QString::fromStdString((*it)->GetName()));

	cbVsync = new QCheckBox(tr("V-Sync"));
	cbFullscreen = new QCheckBox(tr("Fullscreen"));

	cbFPS = new QCheckBox(tr("Show FPS"));
	cbAutoWindowSize = new QCheckBox(tr("Automatic Sizing"));
	cbHideCursor = new QCheckBox(tr("Hide Mouse Cursor"));
	cbRenderToMain = new QCheckBox(tr("Render to Main Window"));


	// Layouts
	DGroupBoxV* groupGeneral = new DGroupBoxV(tr("General"));
	groupGeneral->addWidget(cbBackend);

	DGroupBoxV* groupDisplay = new DGroupBoxV(tr("Display"));
	groupDisplay->addWidget(cbVsync);
	groupDisplay->addWidget(cbFullscreen);

	DGroupBoxV* groupEmuWindow = new DGroupBoxV(tr("Emulation Window"));
	groupEmuWindow->addWidget(cbFPS);
	groupEmuWindow->addWidget(cbAutoWindowSize);
	groupEmuWindow->addWidget(cbHideCursor);
	groupEmuWindow->addWidget(cbRenderToMain);

	QBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(groupGeneral);
	mainLayout->addWidget(groupDisplay);
	mainLayout->addWidget(groupEmuWindow);
	mainLayout->addStretch();
	tab->setLayout(mainLayout);


	// Initial values
	// TODO: Load the actual config, don't use the current one (which might be "dirtied" by game inis.
	ctrlManager = new DControlStateManager(this);
	ctrlManager->RegisterControl(cbBackend, QString::fromStdString(g_video_backend->GetName()));
	ctrlManager->RegisterControl(cbVsync, g_Config.bVSync);
	ctrlManager->RegisterControl(cbFullscreen, SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);
	ctrlManager->RegisterControl(cbFPS, g_Config.bShowFPS);
	ctrlManager->RegisterControl(cbAutoWindowSize, SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize);
	ctrlManager->RegisterControl(cbHideCursor, SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor);
	ctrlManager->RegisterControl(cbRenderToMain, SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain);

	return tab;
}

QWidget* DConfigGfx::CreateEnhancementsTabWidget()
{
	QWidget* tab = new QWidget;

	// Widgets


	// Layouts
	QBoxLayout* mainLayout = new QVBoxLayout;
	tab->setLayout(mainLayout);


	// Initial values

	return tab;
}

QWidget* DConfigGfx::CreateHacksTabWidget()
{
	QWidget* tab = new QWidget;

	// Widgets


	// Layouts
	QBoxLayout* mainLayout = new QVBoxLayout;
	tab->setLayout(mainLayout);


	// Initial values

	return tab;
}

QWidget* DConfigGfx::CreateAdvancedTabWidget()
{
	QWidget* tab = new QWidget;

	// Widgets


	// Layouts
	QBoxLayout* mainLayout = new QVBoxLayout;
	tab->setLayout(mainLayout);


	// Initial values

	return tab;
}

DConfigGfx::DConfigGfx(QWidget* parent) : QTabWidget(parent)
{
	addTab(CreateGeneralTabWidget(), tr("General"));
	addTab(CreateEnhancementsTabWidget(), tr("Enhancements"));
	addTab(CreateHacksTabWidget(), tr("Hacks"));
	addTab(CreateAdvancedTabWidget(), tr("Advanced"));

	tabBar()->setUsesScrollButtons(false);
}

void DConfigGfx::Reset()
{
	ctrlManager->OnReset();
}

void DConfigGfx::Apply()
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	g_video_backend = g_available_video_backends[cbBackend->currentIndex()];
	StartUp.m_strVideoBackend = g_video_backend->GetName();
	//cbBackend; TODO!

	g_Config.bVSync = cbVsync->isChecked();
	StartUp.bFullscreen = cbFullscreen->isChecked();

	g_Config.bShowFPS = cbFPS->isChecked();
	StartUp.bRenderWindowAutoSize = cbAutoWindowSize->isChecked();
	StartUp.bHideCursor = cbHideCursor->isChecked(); // StartUp.AutoHideCursor??
	StartUp.bRenderToMain = cbRenderToMain->isChecked();
}
