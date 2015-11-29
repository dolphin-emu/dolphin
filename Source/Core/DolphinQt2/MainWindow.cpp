// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

#include "Common/FileUtil.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/GameList/GameListModel.h"

MainWindow::MainWindow() : QMainWindow(nullptr)
{
	setWindowTitle(tr("Dolphin"));
	setWindowIcon(QIcon(Resources::GetMisc(Resources::LOGO_SMALL)));

	MakeToolBar();
	MakeGameList();
	MakeRenderWidget();
	MakeStack();
	MakeMenus();
}

MainWindow::~MainWindow()
{
	m_render_widget->deleteLater();
}

void MainWindow::MakeMenus()
{
	MakeFileMenu();
	menuBar()->addMenu(tr("Emulation"));
	menuBar()->addMenu(tr("Movie"));
	menuBar()->addMenu(tr("Options"));
	menuBar()->addMenu(tr("Tools"));
	MakeViewMenu();
	menuBar()->addMenu(tr("Help"));
}

void MainWindow::MakeFileMenu()
{
	QMenu* file_menu = menuBar()->addMenu(tr("File"));
	file_menu->addAction(tr("Open"), this, SLOT(Open()));
	file_menu->addAction(tr("Exit"), this, SLOT(close()));
}

void MainWindow::MakeViewMenu()
{
	QMenu* view_menu = menuBar()->addMenu(tr("View"));
	AddTableColumnsMenu(view_menu);
	AddListTypePicker(view_menu);
}

void MainWindow::AddTableColumnsMenu(QMenu* view_menu)
{
	QActionGroup* column_group = new QActionGroup(this);
	QMenu* cols_menu = view_menu->addMenu(tr("Table Columns"));
	column_group->setExclusive(false);

	QStringList col_names{
		tr("Platform"),
		tr("ID"),
		tr("Banner"),
		tr("Title"),
		tr("Description"),
		tr("Maker"),
		tr("Size"),
		tr("Country"),
		tr("Quality")
	};
	// TODO we'll need to update SConfig with the extra columns. Then we can
	// clean this up significantly.
	QList<bool> show_cols{
		SConfig::GetInstance().m_showSystemColumn,
		SConfig::GetInstance().m_showIDColumn,
		SConfig::GetInstance().m_showBannerColumn,
		true,
		false,
		SConfig::GetInstance().m_showMakerColumn,
		SConfig::GetInstance().m_showSizeColumn,
		SConfig::GetInstance().m_showRegionColumn,
		SConfig::GetInstance().m_showStateColumn,
	};
	// - 1 because we never show COL_LARGE_ICON column in the table.
	for (int i = 0; i < GameListModel::NUM_COLS - 1; i++)
	{
		QAction* action = column_group->addAction(cols_menu->addAction(col_names[i]));
		action->setCheckable(true);
		action->setChecked(show_cols[i]);
		m_game_list->SetViewColumn(i, show_cols[i]);
		connect(action, &QAction::triggered, [=]()
		{
			m_game_list->SetViewColumn(i, action->isChecked());
			switch (i)
			{
			case GameListModel::COL_PLATFORM:
				SConfig::GetInstance().m_showSystemColumn = action->isChecked();
				break;
			case GameListModel::COL_ID:
				SConfig::GetInstance().m_showIDColumn = action->isChecked();
				break;
			case GameListModel::COL_TITLE:
				SConfig::GetInstance().m_showBannerColumn = action->isChecked();
				break;
			case GameListModel::COL_MAKER:
				SConfig::GetInstance().m_showMakerColumn = action->isChecked();
				break;
			case GameListModel::COL_SIZE:
				SConfig::GetInstance().m_showSizeColumn = action->isChecked();
				break;
			case GameListModel::COL_COUNTRY:
				SConfig::GetInstance().m_showRegionColumn = action->isChecked();
				break;
			case GameListModel::COL_RATING:
				SConfig::GetInstance().m_showStateColumn = action->isChecked();
				break;
			default: break;
			}
			SConfig::GetInstance().SaveSettings();
		});
	}
}

void MainWindow::AddListTypePicker(QMenu* view_menu)
{
	QActionGroup* list_group = new QActionGroup(this);
	view_menu->addSection(tr("List Type"));
	list_group->setExclusive(true);

	QAction* set_table = list_group->addAction(view_menu->addAction(tr("Table")));
	QAction* set_list = list_group->addAction(view_menu->addAction(tr("List")));

	set_table->setCheckable(true);
	set_table->setChecked(true);
	set_list->setCheckable(true);

	connect(set_table, &QAction::triggered, m_game_list, &GameList::SetTableView);
	connect(set_list, &QAction::triggered, m_game_list, &GameList::SetListView);
}

void MainWindow::MakeToolBar()
{
	setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	m_tool_bar = addToolBar(tr("ToolBar"));
	m_tool_bar->setMovable(false);
	m_tool_bar->setFloatable(false);

	PopulateToolBar();
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

void MainWindow::PopulateToolBar()
{
	m_tool_bar->clear();

	QString dir = QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().theme_name));
	m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("open.png")), tr("Open"),
			this, SLOT(Open()));
	m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("browse.png")), tr("Browse"),
			this, SLOT(Browse()));

	m_tool_bar->addSeparator();

	QAction* play_action = m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("play.png")), tr("Play"),
			this, SLOT(Play()));
	connect(this, &MainWindow::EmulationStarted, [=](){ play_action->setEnabled(false); });
	connect(this, &MainWindow::EmulationPaused, [=](){ play_action->setEnabled(true); });
	connect(this, &MainWindow::EmulationStopped, [=](){ play_action->setEnabled(true); });

	QAction* pause_action = m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("pause.png")), tr("Pause"),
			this, SLOT(Pause()));
	pause_action->setEnabled(false);
	connect(this, &MainWindow::EmulationStarted, [=](){ pause_action->setEnabled(true); });
	connect(this, &MainWindow::EmulationPaused, [=](){ pause_action->setEnabled(false); });
	connect(this, &MainWindow::EmulationStopped, [=](){ pause_action->setEnabled(false); });

	QAction* stop_action = m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("stop.png")), tr("Stop"),
			this, SLOT(Stop()));
	stop_action->setEnabled(false);
	connect(this, &MainWindow::EmulationStarted, [=](){ stop_action->setEnabled(true); });
	connect(this, &MainWindow::EmulationPaused, [=](){ stop_action->setEnabled(true); });
	connect(this, &MainWindow::EmulationStopped, [=](){ stop_action->setEnabled(false); });

	QAction* fullscreen_action = m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("fullscreen.png")), tr("Full Screen"),
			this, SLOT(FullScreen()));
	fullscreen_action->setEnabled(false);
	connect(this, &MainWindow::EmulationStarted, [=](){ fullscreen_action->setEnabled(true); });
	connect(this, &MainWindow::EmulationStopped, [=](){ fullscreen_action->setEnabled(false); });

	QAction* screenshot_action = m_tool_bar->addAction(
			QIcon(dir + QStringLiteral("screenshot.png")), tr("Screen Shot"),
			this, SLOT(ScreenShot()));
	screenshot_action->setEnabled(false);
	connect(this, &MainWindow::EmulationStarted, [=](){ screenshot_action->setEnabled(true); });
	connect(this, &MainWindow::EmulationStopped, [=](){ screenshot_action->setEnabled(false); });

	m_tool_bar->addSeparator();
	m_tool_bar->addAction(QIcon(dir + QStringLiteral("config.png")), tr("Settings"))->setEnabled(false);
	m_tool_bar->addAction(QIcon(dir + QStringLiteral("graphics.png")), tr("Graphics"))->setEnabled(false);
	m_tool_bar->addAction(QIcon(dir + QStringLiteral("classic.png")), tr("Controllers"))->setEnabled(false);
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

void MainWindow::Browse()
{
	QString dir = QFileDialog::getExistingDirectory(this,
			tr("Select a Directory"),
			QDir::currentPath());
	if (!dir.isEmpty())
	{
		std::vector<std::string>& iso_folders = SConfig::GetInstance().m_ISOFolder;
		auto found = std::find(iso_folders.begin(), iso_folders.end(), dir.toStdString());
		if (found == iso_folders.end())
		{
			iso_folders.push_back(dir.toStdString());
			SConfig::GetInstance().SaveSettings();
			emit m_game_list->DirectoryAdded(dir);
		}
	}
}

void MainWindow::Play()
{
	// If we're in a paused game, start it up again.
	// Otherwise, play the selected game, if there is one.
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
			QString path = QString::fromStdString(SConfig::GetInstance().m_LastFilename);
			if (QFile::exists(path))
				StartGame(path);
			else
				Open();
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
	if (SConfig::GetInstance().bConfirmStop)
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

void MainWindow::FullScreen()
{
	// If the render widget is fullscreen we want to reset it to whatever is in
	// SConfig. If it's set to be fullscreen then it just remakes the window,
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

void MainWindow::StartGame(QString path)
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
	ShowRenderWidget();
	emit EmulationStarted();
}

void MainWindow::ShowRenderWidget()
{
	if (SConfig::GetInstance().bRenderToMain)
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
		if (SConfig::GetInstance().bFullscreen)
		{
			m_render_widget->showFullScreen();
		}
		else
		{
			m_render_widget->setFixedSize(
					SConfig::GetInstance().iRenderWindowWidth,
					SConfig::GetInstance().iRenderWindowHeight);
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
