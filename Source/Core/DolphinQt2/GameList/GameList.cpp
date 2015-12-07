// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QHeaderView>

#include "Core/ConfigManager.h"
#include "DolphinQt2/GameList/GameList.h"
#include "DolphinQt2/GameList/GameListProxyModel.h"

GameList::GameList(QWidget* parent): QStackedWidget(parent)
{
	m_model = new GameListModel(this);
	m_proxy = new GameListProxyModel(this);
	m_proxy->setSourceModel(m_model);
	m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

	MakeTableView();
	MakeListView();

	connect(m_table, &QTableView::doubleClicked, this, &GameList::GameSelected);
	connect(m_list, &QListView::doubleClicked, this, &GameList::GameSelected);
	connect(this, &GameList::DirectoryAdded, m_model, &GameListModel::DirectoryAdded);

	addWidget(m_table);
	addWidget(m_list);
	setCurrentWidget(m_table);
}

void GameList::MakeTableView()
{
	m_table = new QTableView(this);
	m_table->setModel(m_proxy);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setAlternatingRowColors(true);
	m_table->setShowGrid(false);
	m_table->setSortingEnabled(true);
	m_table->setCurrentIndex(QModelIndex());

	// FIXME These icon image are overly wide and should be cut down to size,
	// then we can remove these lines.
	m_table->setColumnWidth(GameListModel::COL_PLATFORM, 52);
	m_table->setColumnWidth(GameListModel::COL_COUNTRY, 38);
	m_table->setColumnWidth(GameListModel::COL_RATING, 52);

	// This column is for the icon view. Hide it.
	m_table->setColumnHidden(GameListModel::COL_LARGE_ICON, true);

	QHeaderView* header = m_table->horizontalHeader();
	header->setSectionResizeMode(GameListModel::COL_PLATFORM, QHeaderView::Fixed);
	header->setSectionResizeMode(GameListModel::COL_COUNTRY, QHeaderView::Fixed);
	header->setSectionResizeMode(GameListModel::COL_ID, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(GameListModel::COL_BANNER, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(GameListModel::COL_TITLE, QHeaderView::Stretch);
	header->setSectionResizeMode(GameListModel::COL_MAKER, QHeaderView::Stretch);
	header->setSectionResizeMode(GameListModel::COL_SIZE, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(GameListModel::COL_DESCRIPTION, QHeaderView::Stretch);
	header->setSectionResizeMode(GameListModel::COL_RATING, QHeaderView::Fixed);
}

void GameList::MakeListView()
{
	m_list = new QListView(this);
	m_list->setModel(m_proxy);
	m_list->setViewMode(QListView::IconMode);
	m_list->setModelColumn(GameListModel::COL_LARGE_ICON);
	m_list->setResizeMode(QListView::Adjust);
	m_list->setUniformItemSizes(true);
}

QString GameList::GetSelectedGame() const
{
	QItemSelectionModel* sel_model;
	if (currentWidget() == m_table)
		sel_model = m_table->selectionModel();
	else
		sel_model = m_list->selectionModel();

	if (sel_model->hasSelection())
		return m_model->GetPath(m_proxy->mapToSource(sel_model->selectedIndexes()[0]).row());
	else
		return QString();
}
