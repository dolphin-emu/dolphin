// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QMainWindow>

#include "DolphinQt2/LogViewer.h"
#include "DolphinQt2/MenuBar.h"
#include "DolphinQt2/Config/LogDialog.h"

MenuBar::MenuBar(QWidget* parent)
	: QMenuBar(parent)
{
	AddFileMenu();
	addMenu(tr("Emulation"));
	addMenu(tr("Movie"));
	addMenu(tr("Options"));
	addMenu(tr("Tools"));
	AddViewMenu();
	addMenu(tr("Help"));
}

void MenuBar::AddFileMenu()
{
	QMenu* file_menu = addMenu(tr("File"));
	file_menu->addAction(tr("Open"), this, SIGNAL(Open()));
	file_menu->addAction(tr("Exit"), this, SIGNAL(Exit()));
}

void MenuBar::AddViewMenu()
{
	QMenu* view_menu = addMenu(tr("View"));
	AddGameListTypeSection(view_menu);

	view_menu->addSeparator();
	AddTableColumnsMenu(view_menu);

	view_menu->addSeparator();
	AddLogMenu(view_menu);
}

void MenuBar::AddGameListTypeSection(QMenu* view_menu)
{
	QAction* table_view = view_menu->addAction(tr("Table"));
	table_view->setCheckable(true);

	QAction* list_view = view_menu->addAction(tr("List"));
	list_view->setCheckable(true);

	QActionGroup* list_group = new QActionGroup(this);
	list_group->addAction(table_view);
	list_group->addAction(list_view);

	// TODO load this from settings
	table_view->setChecked(true);

	connect(table_view, &QAction::triggered, this, &MenuBar::ShowTable);
	connect(list_view, &QAction::triggered, this, &MenuBar::ShowList);
}

// TODO implement this
void MenuBar::AddTableColumnsMenu(QMenu* view_menu)
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
	for (int i = 0; i < col_names.count(); i++)
	{
		QAction* action = column_group->addAction(cols_menu->addAction(col_names[i]));
		action->setCheckable(true);
	}
}

void MenuBar::AddLogMenu(QMenu* view_menu)
{
	QAction* show_log = view_menu->addAction(tr("Show Log"));
	show_log->setCheckable(true);

	QAction* configure_log = view_menu->addAction(tr("Configure Log"));
	//TODO load this from settings
	show_log->setChecked(false);

	//setup  LogViewer
	log_dock = new QDockWidget(tr("Log"), parentWidget());
	log_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	LogViewer* m_LogViewer = new LogViewer(log_dock);
	log_dock->setWidget(m_LogViewer);
	static_cast<QMainWindow*>(parentWidget())->addDockWidget(Qt::BottomDockWidgetArea, log_dock);
	log_dock->hide();

	connect(show_log, &QAction::toggled, this, &MenuBar::ToggleLogViewer);
	connect(configure_log, &QAction::triggered, this, &MenuBar::OpenLogDialog);
}

void MenuBar::ToggleLogViewer(bool display)
{
	if (display)
		log_dock->show();
	else
		log_dock->hide();
}

void MenuBar::OpenLogDialog()
{
	LogDialog* m_LogDialog = new LogDialog(parentWidget());
	m_LogDialog->exec();
}
