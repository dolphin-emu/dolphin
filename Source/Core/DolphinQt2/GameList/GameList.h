// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QListView>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameListModel.h"

class GameList final : public QStackedWidget
{
	Q_OBJECT

public:
	explicit GameList(QWidget* parent = nullptr);

	QString GetSelectedGame() const;

public slots:
	void SetTableView() { setCurrentWidget(m_table); }
	void SetListView() { setCurrentWidget(m_list); }
	void SetViewColumn(int col, bool view) { m_table->setColumnHidden(col, !view); }

signals:
	void GameSelected();
	void DirectoryAdded(QString dir);
	void DirectoryRemoved(QString dir);

private:
	void MakeTableView();
	void MakeListView();

	GameListModel* m_model;
	QSortFilterProxyModel* m_table_proxy;
	QSortFilterProxyModel* m_list_proxy;

	QListView* m_list;
	QTableView* m_table;
protected:
	void keyReleaseEvent(QKeyEvent* event) override;
};
