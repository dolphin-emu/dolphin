// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QActionGroup>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

#include "ui_MainWindow.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/HW/ProcessorInterface.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/SystemInfo.h"
#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

#include "VideoCommon/VideoConfig.h"

// The "g_main_window" object as defined in MainWindow.h
DMainWindow* g_main_window = nullptr;

DMainWindow::DMainWindow(QWidget* parent_widget)
	: QMainWindow(parent_widget)
{
	m_ui = std::make_unique<Ui::DMainWindow>();
	m_ui->setupUi(this);

	Resources::Init();
	UpdateIcons();
	setWindowIcon(Resources::GetIcon(Resources::DOLPHIN_LOGO));

	// Create the GameList
	m_game_tracker = new DGameTracker(this);
	m_ui->centralWidget->addWidget(m_game_tracker);
	m_game_tracker->ScanForGames();
	m_game_tracker->SelectLastBootedGame();

	// Setup the GameList style switching actions
	QActionGroup* gamelistGroup = new QActionGroup(this);
	gamelistGroup->addAction(m_ui->actionListView);
	gamelistGroup->addAction(m_ui->actionTreeView);
	gamelistGroup->addAction(m_ui->actionGridView);
	gamelistGroup->addAction(m_ui->actionIconView);

	// TODO: save/load this from user prefs!
	m_ui->actionListView->setChecked(true);
	OnGameListStyleChanged();

	// Connect all the signals/slots
	connect(this, SIGNAL(CoreStateChanged(Core::EState)), this, SLOT(OnCoreStateChanged(Core::EState)));

	connect(m_ui->actionOpen, SIGNAL(triggered()), this, SLOT(OnOpen()));
	connect(m_ui->actionBrowse, SIGNAL(triggered()), this, SLOT(OnBrowse()));
	connect(m_ui->actionExit, SIGNAL(triggered()), this, SLOT(OnExit()));

	connect(m_ui->actionListView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));
	connect(m_ui->actionTreeView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));
	connect(m_ui->actionGridView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));
	connect(m_ui->actionIconView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));

	connect(m_ui->actionPlay, SIGNAL(triggered()), this, SLOT(OnPlay()));
	connect(m_ui->actionPlay_mnu, SIGNAL(triggered()), this, SLOT(OnPlay()));
	connect(m_game_tracker, SIGNAL(StartGame()), this, SLOT(OnPlay()));
	connect(m_ui->actionStop, SIGNAL(triggered()), this, SLOT(OnStop()));
	connect(m_ui->actionStop_mnu, SIGNAL(triggered()), this, SLOT(OnStop()));
	connect(m_ui->actionReset, SIGNAL(triggered()), this, SLOT(OnReset()));

	connect(m_ui->actionWebsite, SIGNAL(triggered()), this, SLOT(OnOpenWebsite()));
	connect(m_ui->actionOnlineDocs, SIGNAL(triggered()), this, SLOT(OnOpenDocs()));
	connect(m_ui->actionGitHub, SIGNAL(triggered()), this, SLOT(OnOpenGitHub()));
	connect(m_ui->actionSystemInfo, SIGNAL(triggered()), this, SLOT(OnOpenSystemInfo()));
	connect(m_ui->actionAbout, SIGNAL(triggered()), this, SLOT(OnOpenAbout()));
	connect(m_ui->actionAboutQt, SIGNAL(triggered()), this, SLOT(OnOpenAboutQt()));

	// Update GUI items
	emit CoreStateChanged(Core::CORE_UNINITIALIZED);

	// Platform-specific stuff
#ifdef Q_OS_MACX
	m_ui->toolbar->setMovable(false);
#endif
}

DMainWindow::~DMainWindow()
{
}

void DMainWindow::closeEvent(QCloseEvent* ce)
{
	Stop();
}

// Emulation

void DMainWindow::StartGame(const QString filename)
{
	m_render_widget = std::make_unique<DRenderWidget>();
	m_render_widget->setWindowTitle(tr("Dolphin")); // TODO
	m_render_widget->setWindowIcon(windowIcon());

	if (SConfig::GetInstance().bFullscreen)
	{
		m_render_widget->setWindowFlags(m_render_widget->windowFlags() | Qt::BypassWindowManagerHint);
		g_Config.bFullscreen = !g_Config.bBorderlessFullscreen;
		m_render_widget->showFullScreen();
	}
	else
	{
		m_ui->centralWidget->addWidget(m_render_widget.get());
		m_ui->centralWidget->setCurrentWidget(m_render_widget.get());

		if (SConfig::GetInstance().bRenderWindowAutoSize)
		{
			// Resize main window to fit render
			m_render_widget->setMinimumSize(SConfig::GetInstance().iRenderWindowWidth,
				SConfig::GetInstance().iRenderWindowHeight);
			qApp->processEvents(); // Force a redraw so the window has time to resize
			m_render_widget->setMinimumSize(0, 0); // Allow the widget to scale down
		}
		m_render_widget->adjustSize();
	}

	if (!BootManager::BootCore(filename.toStdString()))
	{
		QMessageBox::critical(this, tr("Fatal error"), tr("Failed to init Core"), QMessageBox::Ok);
		if (SConfig::GetInstance().bFullscreen)
			m_render_widget->close();
		else
			m_ui->centralWidget->removeWidget(m_render_widget.get());
		m_render_widget.reset();
	}
	else
	{
		// TODO: Disable screensaver!
		emit CoreStateChanged(Core::CORE_RUN);
	}
}

QString DMainWindow::RequestBootFilename()
{
	// If a game is already selected, just return the filename
	if (m_game_tracker->SelectedGame() != nullptr)
			return m_game_tracker->SelectedGame()->GetFileName();

	return ShowFileDialog();
}

QString DMainWindow::ShowFileDialog()
{
	return QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
		tr("All supported ROMs (%1);;All files (*)")
		.arg(SL("*.gcm *.iso *.ciso *.gcz *.wbfs *.elf *.dol *.dff *.tmd *.wad")));
}

QString DMainWindow::ShowFolderDialog()
{
	return QFileDialog::getExistingDirectory(this, tr("Browse for a directory to add"),
	                                         QDir::homePath(),
	                                         QFileDialog::ShowDirsOnly);
}

void DMainWindow::DoStartPause()
{
	if (Core::GetState() == Core::CORE_RUN)
	{
		Core::SetState(Core::CORE_PAUSE);
		emit CoreStateChanged(Core::CORE_PAUSE);
	}
	else
	{
		Core::SetState(Core::CORE_RUN);
		emit CoreStateChanged(Core::CORE_RUN);
	}
	if (SConfig::GetInstance().bHideCursor)
		m_render_widget->setCursor(Qt::BlankCursor);
}

void DMainWindow::OnOpen()
{
	QString filename = ShowFileDialog();
	if (!filename.isNull())
		StartGame(filename);
}

void DMainWindow::OnBrowse()
{
	std::string path = ShowFolderDialog().toStdString();
	std::vector<std::string>& iso_folder = SConfig::GetInstance().m_ISOFolder;
	if (!path.empty())
	{
		auto itResult = std::find(iso_folder.begin(), iso_folder.end(), path);

		if (itResult == iso_folder.end())
		{
			iso_folder.push_back(path);
			SConfig::GetInstance().SaveSettings();
		}
	}
	m_game_tracker->ScanForGames();
}

void DMainWindow::OnExit()
{
	close();
	if (Core::GetState() == Core::CORE_UNINITIALIZED || m_isStopping)
		return;
	Stop();
}

void DMainWindow::OnPlay()
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		DoStartPause();
	}
	else
	{
		// initialize Core and boot the game
		QString filename = RequestBootFilename();
		if (!filename.isNull())
			StartGame(filename);
	}
}

bool DMainWindow::OnStop()
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED || m_isStopping)
		return true;

	// Ask for confirmation in case the user accidentally clicked Stop / Escape
	if (SConfig::GetInstance().bConfirmStop)
	{
		// Pause emulation
		Core::SetState(Core::CORE_PAUSE);
		emit CoreStateChanged(Core::CORE_PAUSE);

		QMessageBox::StandardButton ret = QMessageBox::question(m_render_widget.get(), tr("Please confirm..."),
			tr("Do you want to stop the current emulation?"),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

		if (ret == QMessageBox::No)
		{
			DoStartPause();
			return false;
		}
	}

	return Stop();
}

bool DMainWindow::Stop()
{
	m_isStopping = true;

	// TODO: Movie stuff
	// TODO: Show the author/description dialog here

	BootManager::Stop();

	// TODO: Allow screensaver again
	// TODO: Restore original window title

	// TODO:
	// If batch mode was specified on the command-line, exit now.
	//if (m_bBatchMode)
	//	Close(true);

	if (SConfig::GetInstance().bFullscreen)
		m_render_widget->close();
	else
		m_ui->centralWidget->removeWidget(m_render_widget.get());
	m_render_widget.reset();

	emit CoreStateChanged(Core::CORE_UNINITIALIZED);
	m_isStopping = false;
	return true;
}

void DMainWindow::OnReset()
{
	// TODO: Movie needs to be reset here
	ProcessorInterface::ResetButton_Tap();
}

void DMainWindow::OnGameListStyleChanged()
{
	if (m_ui->actionListView->isChecked())
		m_game_tracker->SetViewStyle(STYLE_LIST);
	else if (m_ui->actionTreeView->isChecked())
		m_game_tracker->SetViewStyle(STYLE_TREE);
	else if (m_ui->actionGridView->isChecked())
		m_game_tracker->SetViewStyle(STYLE_GRID);
	else if (m_ui->actionIconView->isChecked())
		m_game_tracker->SetViewStyle(STYLE_ICON);
}

void DMainWindow::OnCoreStateChanged(Core::EState state)
{
	bool is_not_initialized = (state == Core::CORE_UNINITIALIZED);
	bool is_running = (state == Core::CORE_RUN);
	bool is_paused = (state == Core::CORE_PAUSE);

	// Update the toolbar
	m_ui->actionPlay->setEnabled(is_not_initialized || is_running || is_paused);
	if (is_running)
	{
		m_ui->actionPlay->setIcon(Resources::GetIcon(Resources::TOOLBAR_PAUSE));
		m_ui->actionPlay->setText(tr("Pause"));
		m_ui->actionPlay_mnu->setText(tr("Pause"));
	}
	else if (is_paused || is_not_initialized)
	{
		m_ui->actionPlay->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLAY));
		m_ui->actionPlay->setText(tr("Play"));
		m_ui->actionPlay_mnu->setText(tr("Play"));
	}

	m_ui->actionStop->setEnabled(!is_not_initialized);
	m_ui->actionOpen->setEnabled(is_not_initialized);
	m_game_tracker->setEnabled(is_not_initialized);
}

// Update all the icons used in DMainWindow with fresh ones from
// "Resources". Call this function after changing the icon theme.
void DMainWindow::UpdateIcons()
{
	// Play/Pause is handled in OnCoreStateChanged().
	m_ui->actionStop->setIcon(Resources::GetIcon(Resources::TOOLBAR_STOP));
}

// Help menu
void DMainWindow::OnOpenWebsite()
{
	QDesktopServices::openUrl(QUrl(SL("https://dolphin-emu.org/")));
}

void DMainWindow::OnOpenDocs()
{
	QDesktopServices::openUrl(QUrl(SL("https://dolphin-emu.org/docs/guides/")));
}

void DMainWindow::OnOpenGitHub()
{
	QDesktopServices::openUrl(QUrl(SL("https://github.com/dolphin-emu/dolphin")));
}

void DMainWindow::OnOpenSystemInfo()
{
	DSystemInfo* dlg = new DSystemInfo(this);
	dlg->open();
}

void DMainWindow::OnOpenAbout()
{
	DAboutDialog* dlg = new DAboutDialog(this);
	dlg->open();
}

void DMainWindow::OnOpenAboutQt()
{
	QApplication::aboutQt();
}
