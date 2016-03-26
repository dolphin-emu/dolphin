// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>

#include "Core/BootManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/HW/ProcessorInterface.h"
#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/PathDialog.h"

MainWindow::MainWindow() : QMainWindow(nullptr)
{
	setWindowTitle(tr("Dolphin"));
	setWindowIcon(QIcon(Resources::GetMisc(Resources::LOGO_SMALL)));

	CreateComponents();

	ConnectGameList();
	ConnectPathsDialog();
	ConnectToolBar();
	ConnectRenderWidget();
	ConnectStack();
	ConnectMenuBar();
}

MainWindow::~MainWindow()
{
	m_render_widget->deleteLater();
}

void MainWindow::CreateComponents()
{
	m_menu_bar = new MenuBar(this);
	m_tool_bar = new ToolBar(this);
	m_game_list = new GameList(this);
	m_render_widget = new RenderWidget;
	m_stack = new QStackedWidget(this);
	m_paths_dialog = new PathDialog(this);
}

void MainWindow::ConnectMenuBar()
{
	setMenuBar(m_menu_bar);
	// File
	connect(m_menu_bar, &MenuBar::Open, this, &MainWindow::Open);
	connect(m_menu_bar, &MenuBar::Exit, this, &MainWindow::close);

	// Emulation
	connect(m_menu_bar, &MenuBar::Pause, this, &MainWindow::Pause);
	connect(m_menu_bar, &MenuBar::Play, this, &MainWindow::Play);
	connect(m_menu_bar, &MenuBar::Stop, this, &MainWindow::Stop);
	connect(m_menu_bar, &MenuBar::Reset, this, &MainWindow::Reset);
	connect(m_menu_bar, &MenuBar::Fullscreen, this, &MainWindow::FullScreen);
	connect(m_menu_bar, &MenuBar::FrameAdvance, this, &MainWindow::FrameAdvance);
	connect(m_menu_bar, &MenuBar::Screenshot, this, &MainWindow::ScreenShot);
	connect(m_menu_bar, &MenuBar::StateLoad, this, &MainWindow::StateLoad);
	connect(m_menu_bar, &MenuBar::StateSave, this, &MainWindow::StateSave);
	connect(m_menu_bar, &MenuBar::StateLoadSlot, this, &MainWindow::StateLoadSlot);
	connect(m_menu_bar, &MenuBar::StateSaveSlot, this, &MainWindow::StateSaveSlot);
	connect(m_menu_bar, &MenuBar::StateLoadSlotAt, this, &MainWindow::StateLoadSlotAt);
	connect(m_menu_bar, &MenuBar::StateSaveSlotAt, this, &MainWindow::StateSaveSlotAt);
	connect(m_menu_bar, &MenuBar::StateLoadUndo, this, &MainWindow::StateLoadUndo);
	connect(m_menu_bar, &MenuBar::StateSaveUndo, this, &MainWindow::StateSaveUndo);
	connect(m_menu_bar, &MenuBar::StateSaveOldest, this, &MainWindow::StateSaveOldest);
	connect(m_menu_bar, &MenuBar::SetStateSlot, this, &MainWindow::SetStateSlot);

	// View
	connect(m_menu_bar, &MenuBar::ShowTable, m_game_list, &GameList::SetTableView);
	connect(m_menu_bar, &MenuBar::ShowList, m_game_list, &GameList::SetListView);
	connect(m_menu_bar, &MenuBar::ShowAboutDialog, this, &MainWindow::ShowAboutDialog);

	connect(this, &MainWindow::EmulationStarted, m_menu_bar, &MenuBar::EmulationStarted);
	connect(this, &MainWindow::EmulationPaused, m_menu_bar, &MenuBar::EmulationPaused);
	connect(this, &MainWindow::EmulationStopped, m_menu_bar, &MenuBar::EmulationStopped);
}

void MainWindow::ConnectToolBar()
{
	addToolBar(m_tool_bar);
	connect(m_tool_bar, &ToolBar::OpenPressed, this, &MainWindow::Open);
	connect(m_tool_bar, &ToolBar::PlayPressed, this, &MainWindow::Play);
	connect(m_tool_bar, &ToolBar::PausePressed, this, &MainWindow::Pause);
	connect(m_tool_bar, &ToolBar::StopPressed, this, &MainWindow::Stop);
	connect(m_tool_bar, &ToolBar::FullScreenPressed, this, &MainWindow::FullScreen);
	connect(m_tool_bar, &ToolBar::ScreenShotPressed, this, &MainWindow::ScreenShot);
	connect(m_tool_bar, &ToolBar::PathsPressed, this, &MainWindow::ShowPathsDialog);

	connect(this, &MainWindow::EmulationStarted, m_tool_bar, &ToolBar::EmulationStarted);
	connect(this, &MainWindow::EmulationPaused, m_tool_bar, &ToolBar::EmulationPaused);
	connect(this, &MainWindow::EmulationStopped, m_tool_bar, &ToolBar::EmulationStopped);
}

void MainWindow::ConnectGameList()
{
	connect(m_game_list, &GameList::GameSelected, this, &MainWindow::Play);
}

void MainWindow::ConnectRenderWidget()
{
	m_rendering_to_main = false;
	m_render_widget->hide();
	connect(m_render_widget, &RenderWidget::EscapePressed, this, &MainWindow::Stop);
	connect(m_render_widget, &RenderWidget::Closed, this, &MainWindow::ForceStop);
}

void MainWindow::ConnectStack()
{
	m_stack->setMinimumSize(800, 600);
	m_stack->addWidget(m_game_list);
	setCentralWidget(m_stack);
}

void MainWindow::ConnectPathsDialog()
{
	connect(m_paths_dialog, &PathDialog::PathAdded, m_game_list, &GameList::DirectoryAdded);
	connect(m_paths_dialog, &PathDialog::PathRemoved, m_game_list, &GameList::DirectoryRemoved);
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
	emit EmulationStopped();
}

void MainWindow::Reset()
{
	if (Movie::IsRecordingInput())
		Movie::g_bReset = true;
	ProcessorInterface::ResetButton_Tap();
}

void MainWindow::FrameAdvance()
{
	Movie::DoFrameStep();
	EmulationPaused();
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

void MainWindow::ShowPathsDialog()
{
	m_paths_dialog->show();
	m_paths_dialog->raise();
	m_paths_dialog->activateWindow();
}

void MainWindow::ShowAboutDialog()
{
	AboutDialog* about = new AboutDialog(this);
	about->show();
}

void MainWindow::StateLoad()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Select a File"), QDir::currentPath(),
	    tr("All Save States (*.sav *.s##);; All Files (*)"));
	State::LoadAs(path.toStdString());
}

void MainWindow::StateSave()
{
	QString path = QFileDialog::getSaveFileName(this, tr("Select a File"), QDir::currentPath(),
	    tr("All Save States (*.sav *.s##);; All Files (*)"));
	State::SaveAs(path.toStdString());
}

void MainWindow::StateLoadSlot()
{
	State::Load(m_state_slot);
}

void MainWindow::StateSaveSlot()
{
	State::Save(m_state_slot, true);
	m_menu_bar->UpdateStateSlotMenu();
}

void MainWindow::StateLoadSlotAt(int slot)
{
	State::Load(slot);
}

void MainWindow::StateSaveSlotAt(int slot)
{
	State::Save(slot, true);
	m_menu_bar->UpdateStateSlotMenu();
}

void MainWindow::StateLoadUndo()
{
	State::UndoLoadState();
}

void MainWindow::StateSaveUndo()
{
	State::UndoSaveState();
}

void MainWindow::StateSaveOldest()
{
	State::SaveFirstSaved();
}

void MainWindow::SetStateSlot(int slot)
{
	Settings().SetStateSlot(slot);
	m_state_slot = slot;
}
