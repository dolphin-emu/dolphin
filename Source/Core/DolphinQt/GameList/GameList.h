// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QListView>
#include <QMap>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStandardItem>
#include <QString>
#include <QTableView>

#include "DolphinQt/GameList/GameFile.h"
#include "DolphinQt/GameList/GameListModel.h"

class GameList final : public QStackedWidget
{
	Q_OBJECT

public:
	explicit GameList(QWidget* parent = nullptr);

	QString GetSelectedGame() const;

public slots:
	void SetTableView() { setCurrentWidget(m_table); }
	void SetListView() { setCurrentWidget(m_list); }

signals:
	void GameSelected();

private:
	void MakeTableView();
	void MakeListView();

	GameListModel* m_model;
	QSortFilterProxyModel* m_proxy;
	QListView* m_list;
	QTableView* m_table;
};
