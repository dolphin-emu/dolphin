// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QActionGroup>

#include "Core/ConfigManager.h"
#include "DolphinQt2/MenuBar.h"

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

// TODO implement this after we stop using SConfig.
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
