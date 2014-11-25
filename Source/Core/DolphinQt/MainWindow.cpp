// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QActionGroup>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

#include "ui_MainWindow.h"

#include "Common/StdMakeUnique.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/SystemInfo.h"
#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

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

	connect(m_ui->actionListView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));
	connect(m_ui->actionTreeView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));
	connect(m_ui->actionGridView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));
	connect(m_ui->actionIconView, SIGNAL(triggered()), this, SLOT(OnGameListStyleChanged()));

	connect(m_ui->actionPlay, SIGNAL(triggered()), this, SLOT(OnPlay()));
	connect(m_game_tracker, SIGNAL(StartGame()), this, SLOT(OnPlay()));
	connect(m_ui->actionStop, SIGNAL(triggered()), this, SLOT(OnStop()));

	connect(m_ui->actionWebsite, SIGNAL(triggered()), this, SLOT(OnOpenWebsite()));
	connect(m_ui->actionOnlineDocs, SIGNAL(triggered()), this, SLOT(OnOpenDocs()));
	connect(m_ui->actionGitHub, SIGNAL(triggered()), this, SLOT(OnOpenGitHub()));
	connect(m_ui->actionSystemInfo, SIGNAL(triggered()), this, SLOT(OnOpenSystemInfo()));
	connect(m_ui->actionAbout, SIGNAL(triggered()), this, SLOT(OnOpenAbout()));

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

// Emulation

void DMainWindow::StartGame(const QString filename)
{
	m_render_widget = std::make_unique<DRenderWidget>();
	m_render_widget->setWindowTitle(tr("Dolphin")); // TODO
	m_render_widget->setWindowIcon(windowIcon());

	// TODO: When rendering to main, this won't resize the parent window..
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		connect(m_render_widget.get(), SIGNAL(Closed()), this, SLOT(OnStop()));
		m_render_widget->move(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos);
		m_render_widget->resize(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth, // TODO: Make sure these are client sizes!
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight);
		m_render_widget->show();
	}
	else
	{
		m_ui->centralWidget->addWidget(m_render_widget.get());
		m_ui->centralWidget->setCurrentWidget(m_render_widget.get());
	}

	if (!BootManager::BootCore(filename.toStdString()))
	{
		QMessageBox::critical(this, tr("Fatal error"), tr("Failed to init Core"), QMessageBox::Ok);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_ui->centralWidget->removeWidget(m_render_widget.get());
		else
			m_render_widget->close();
		m_render_widget.reset();
	}
	else
	{
		// TODO: Disable screensaver!

		// TODO: Fullscreen
		//DoFullscreen(SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);

		m_render_widget->focusWidget();
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
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
		m_render_widget->setCursor(Qt::BlankCursor);
}

void DMainWindow::OnOpen()
{
	StartGame(ShowFileDialog());
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

void DMainWindow::OnStop()
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED && !m_isStopping)
	{
		m_isStopping = true;
		// Ask for confirmation in case the user accidentally clicked Stop / Escape
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop)
		{
			int ret = QMessageBox::question(m_render_widget.get(), tr("Please confirm..."),
				tr("Do you want to stop the current emulation?"),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

			if (ret == QMessageBox::No)
				return;
		}

		// TODO: Movie stuff
		// TODO: Show the author/description dialog here

		// TODO: Show busy cursor
		BootManager::Stop();
		// TODO: Hide busy cursor again

		// TODO: Allow screensaver again
		// TODO: Restore original window title

		// TODO: Return from fullscreen if necessary (DoFullscreen in the wx code)

		// TODO:
		// If batch mode was specified on the command-line, exit now.
		//if (m_bBatchMode)
		//	Close(true);

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_ui->centralWidget->removeWidget(m_render_widget.get());
		else
			m_render_widget->close();
		m_render_widget.reset();

		emit CoreStateChanged(Core::CORE_UNINITIALIZED);
	}
	m_isStopping = false;
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
	}
	else if (is_paused || is_not_initialized)
	{
		m_ui->actionPlay->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLAY));
		m_ui->actionPlay->setText(tr("Play"));
	}

	m_ui->actionStop->setEnabled(!is_not_initialized);
	m_ui->actionOpen->setEnabled(is_not_initialized);
	m_game_tracker->setEnabled(is_not_initialized);
}

bool DMainWindow::RenderWidgetHasFocus()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
		return isActiveWindow();
	else if (m_render_widget != nullptr)
		return m_render_widget->isActiveWindow();
	else
		return false;
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
	QDesktopServices::openUrl(QUrl(SL("https://github.com/dolphin-emu/dolphin/")));
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
