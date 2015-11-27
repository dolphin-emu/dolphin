// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QHeaderView>

#include "Core/ConfigManager.h"
#include "DolphinQt2/GameList/GameList.h"

GameList::GameList(QWidget* parent): QStackedWidget(parent)
{
	m_model = new GameListModel(this);
	m_proxy = new QSortFilterProxyModel(this);
	m_proxy->setSourceModel(m_model);

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

	// These fixed column widths make it so that the DisplayRole is cut
	// off, which lets us see the icon but sort by the actual value.
	// It's a bit of a hack. To do it right we need to subclass
	// QSortFilterProxyModel and not show those items.
	m_table->setColumnWidth(GameListModel::COL_PLATFORM, 52);
	m_table->setColumnWidth(GameListModel::COL_COUNTRY, 38);
	m_table->setColumnWidth(GameListModel::COL_RATING, 52);
	m_table->setColumnHidden(GameListModel::COL_LARGE_ICON, true);

	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_PLATFORM, QHeaderView::Fixed);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_COUNTRY, QHeaderView::Fixed);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_ID, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_TITLE, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_MAKER, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_SIZE, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_DESCRIPTION, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(
			GameListModel::COL_RATING, QHeaderView::Fixed);
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
