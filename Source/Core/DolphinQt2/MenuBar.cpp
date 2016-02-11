// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QDesktopServices>
#include <QUrl>

#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/MenuBar.h"
#include "DolphinQt2/Settings.h"

MenuBar::MenuBar(QWidget* parent)
	: QMenuBar(parent)
{
	AddFileMenu();
	addMenu(tr("Emulation"));
	addMenu(tr("Movie"));
	addMenu(tr("Options"));
	addMenu(tr("Tools"));
	AddViewMenu();
	AddHelpMenu();
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

void MenuBar::AddHelpMenu()
{
	QMenu* help_menu = addMenu(tr("Help"));
	QAction* documentation = help_menu->addAction(tr("Online Documentation"));
	connect(documentation, &QAction::triggered, this, [=]() {
	    QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/docs/guides")));
	});
	help_menu->addAction(tr("About"), this, SIGNAL(ShowAboutDialog()));
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

	bool prefer_table = Settings().GetPreferredView();
	table_view->setChecked(prefer_table);
	list_view->setChecked(!prefer_table);

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
