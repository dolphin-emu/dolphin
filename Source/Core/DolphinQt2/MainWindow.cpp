// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <QDir>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>

#ifdef Q_OS_LINUX
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "Common/Logging/Log.h"
#include "Core/BootManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/PathDialog.h"

MainWindow::MainWindow() : QMainWindow(nullptr)
{
	setWindowTitle(tr("Dolphin"));
	setWindowIcon(QIcon(Resources::GetMisc(Resources::LOGO_SMALL)));

	MakeGameList();
	MakeToolBar();
	MakeRenderWidget();
	MakeStack();
	MakeMenuBar();
}

MainWindow::~MainWindow()
{
	m_render_widget->deleteLater();
}

void MainWindow::MakeMenuBar()
{
	m_menu_bar = new MenuBar(this);
	setMenuBar(m_menu_bar);
	connect(m_menu_bar, &MenuBar::Open, this, &MainWindow::Open);
	connect(m_menu_bar, &MenuBar::Exit, this, &MainWindow::close);
	connect(m_menu_bar, &MenuBar::ShowTable, m_game_list, &GameList::SetTableView);
	connect(m_menu_bar, &MenuBar::ShowList, m_game_list, &GameList::SetListView);
}

void MainWindow::MakeToolBar()
{
	m_tool_bar = new ToolBar(this);
	addToolBar(m_tool_bar);

	connect(m_tool_bar, &ToolBar::OpenPressed, this, &MainWindow::Open);
	connect(m_tool_bar, &ToolBar::PlayPressed, this, &MainWindow::Play);
	connect(m_tool_bar, &ToolBar::PausePressed, this, &MainWindow::Pause);
	connect(m_tool_bar, &ToolBar::StopPressed, this, &MainWindow::Stop);
	connect(m_tool_bar, &ToolBar::FullScreenPressed, this, &MainWindow::FullScreen);
	connect(m_tool_bar, &ToolBar::ScreenShotPressed, this, &MainWindow::ScreenShot);
	connect(m_tool_bar, &ToolBar::PathsPressed, this, &MainWindow::PathsConfig);

	connect(this, &MainWindow::EmulationStarted, m_tool_bar, &ToolBar::EmulationStarted);
	connect(this, &MainWindow::EmulationPaused, m_tool_bar, &ToolBar::EmulationPaused);
	connect(this, &MainWindow::EmulationStopped, m_tool_bar, &ToolBar::EmulationStopped);
}

void MainWindow::MakeGameList()
{
	m_game_list = new GameList(this);
	connect(m_game_list, &GameList::GameSelected, this, &MainWindow::Play);
}

void MainWindow::MakeRenderWidget()
{
	m_render_widget = new RenderWidget;
	connect(m_render_widget, &RenderWidget::EscapePressed, this, &MainWindow::Stop);
	connect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);
	m_render_widget->hide();
	m_rendering_to_main = false;
}

void MainWindow::MakeStack()
{
	m_stack = new QStackedWidget;
	m_stack->setMinimumSize(800, 600);
	m_stack->addWidget(m_game_list);
	setCentralWidget(m_stack);
}

void MainWindow::Open()
{
	QString file = QFileDialog::getOpenFileName(this,
			tr("Select a File"),
			QDir::currentPath(),
			tr("All GC/Wii files (*.elf *.dol *.gcm *.iso *.wbfs *.ciso *.gcz *.wad);;"
			   "All Files (*)"));
	if (!file.isEmpty())
		StartGame(file);
}

void MainWindow::Play()
{
	// If we're in a paused game, start it up again.
	// Otherwise, play the selected game, if there is one.
	// Otherwise, play the default game.
	// Otherwise, play the last played game, if there is one.
	// Otherwise, prompt for a new game.
	if (Core::GetState() == Core::CORE_PAUSE)
	{
		Core::SetState(Core::CORE_RUN);
		EnableScreensaver(false);
		emit EmulationStarted();
	}
	else
	{
		QString selection = m_game_list->GetSelectedGame();
		if (selection.length() > 0)
		{
			StartGame(selection);
		}
		else
		{
			QString default_path = Settings().GetDefaultGame();
			if (!default_path.isEmpty() && QFile::exists(default_path))
			{
				StartGame(default_path);
			}
			else
			{
				QString last_path = Settings().GetLastGame();
				if (!last_path.isEmpty() && QFile::exists(last_path))
					StartGame(last_path);
				else
					Open();
			}
		}
	}
}

void MainWindow::Pause()
{
	Core::SetState(Core::CORE_PAUSE);
	emit EmulationPaused();
	EnableScreensaver(true);
}

bool MainWindow::Stop()
{
	bool stop = true;
	if (Settings().GetConfirmStop())
	{
		// We could pause the game here and resume it if they say no.
		QMessageBox::StandardButton confirm;
		confirm = QMessageBox::question(m_render_widget, tr("Confirm"), tr("Stop emulation?"));
		stop = (confirm == QMessageBox::Yes);
	}

	if (stop)
		ForceStop();

	return stop;
}

void MainWindow::ForceStop()
{
	BootManager::Stop();
	HideRenderWidget();
	EnableScreensaver(true);
	emit EmulationStopped();
}

void MainWindow::FullScreen()
{
	// If the render widget is fullscreen we want to reset it to whatever is in
	// settings. If it's set to be fullscreen then it just remakes the window,
	// which probably isn't ideal.
	bool was_fullscreen = m_render_widget->isFullScreen();
	HideRenderWidget();
	if (was_fullscreen)
		ShowRenderWidget();
	else
		m_render_widget->showFullScreen();
}

void MainWindow::ScreenShot()
{
	Core::SaveScreenShot();
}

void MainWindow::PathsConfig()
{
	PathDialog* paths = new PathDialog(this);
	connect(paths, &PathDialog::PathAdded, m_game_list, &GameList::DirectoryAdded);
	connect(paths, &PathDialog::PathRemoved, m_game_list, &GameList::DirectoryRemoved);
	paths->exec();
}

void MainWindow::StartGame(const QString& path)
{
	// If we're running, only start a new game once we've stopped the last.
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (!Stop())
			return;
	}
	// Boot up, show an error if it fails to load the game.
	if (!BootManager::BootCore(path.toStdString()))
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to init core"), QMessageBox::Ok);
		return;
	}
	Settings().SetLastGame(path);
	ShowRenderWidget();
	EnableScreensaver(false);
	emit EmulationStarted();
}

void MainWindow::ShowRenderWidget()
{
	Settings settings;
	if (settings.GetRenderToMain())
	{
		// If we're rendering to main, add it to the stack and update our title when necessary.
		m_rendering_to_main = true;
		m_stack->setCurrentIndex(m_stack->addWidget(m_render_widget));
		connect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
	}
	else
	{
		// Otherwise, just show it.
		m_rendering_to_main = false;
		if (settings.GetFullScreen())
		{
			m_render_widget->showFullScreen();
		}
		else
		{
			m_render_widget->setFixedSize(settings.GetRenderWindowSize());
			m_render_widget->showNormal();
		}
	}
}

void MainWindow::HideRenderWidget()
{
	if (m_rendering_to_main)
	{
		// Remove the widget from the stack and reparent it to nullptr, so that it can draw
		// itself in a new window if it wants. Disconnect the title updates.
		m_stack->removeWidget(m_render_widget);
		m_render_widget->setParent(nullptr);
		m_rendering_to_main = false;
		disconnect(Host::GetInstance(), &Host::RequestTitle, this, &MainWindow::setWindowTitle);
		setWindowTitle(tr("Dolphin"));
	}
	m_render_widget->hide();
}

void MainWindow::EnableScreensaver(bool enable)
{
#ifdef Q_OS_LINUX
	// runs xdg-screensaver which will safely re-enable the screensaver should dolphin crash
	char window_id[20];
	sprintf(window_id, "%llu", winId());
	std::array<char* const, 4> argv = {
			const_cast<char* const>("xdg-screensaver"),
			const_cast<char* const>(enable ? "resume" : "suspend"),
			const_cast<char* const>(window_id),
			nullptr
	};
	pid_t pid;
	if (!posix_spawnp(&pid, "xdg-screensaver", nullptr, nullptr, argv.data(), environ))
	{
		int status;
		while (waitpid(pid, &status, 0) == -1);

		DEBUG_LOG(VIDEO, "Started xdg-screensaver (PID = %d)", static_cast<int>(pid));
	}
#elif defined(Q_OS_WIN)
	if (enable)
		SetThreadExecutionState(ES_CONTINUOUS);
	else
		SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
#elif defined(Q_OS_MAC)
	//TODO: need a dev with mac os
#endif
}
