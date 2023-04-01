// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QLabel>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>

#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/GameList/GameListModel.h"

class TableDelegate;

class GameList final : public QStackedWidget
{
	Q_OBJECT

public:
	explicit GameList(QWidget* parent = nullptr);
	QString GetSelectedGame() const;

public slots:
	void SetTableView() { SetPreferredView(true); }
	void SetListView() { SetPreferredView(false); }
	void SetViewColumn(int col, bool view) { m_table->setColumnHidden(col, !view); }

private slots:
	void ShowContextMenu(const QPoint&);
	void OpenWiki();
	void SetDefaultISO();

signals:
	void GameSelected();
	void DirectoryAdded(const QString& dir);
	void DirectoryRemoved(const QString& dir);

private:
	void MakeTableView();
	void MakeListView();
	void MakeEmptyView();
	// We only have two views, just use a bool to distinguish.
	void SetPreferredView(bool table);
	void ConsiderViewChange();

	GameListModel* m_model;
	TableDelegate* m_delegate;
	QSortFilterProxyModel* m_table_proxy;
	QSortFilterProxyModel* m_list_proxy;

	QListView* m_list;
	QTableView* m_table;
	QLabel* m_empty;
	bool m_prefer_table;
protected:
	void keyReleaseEvent(QKeyEvent* event) override;
};
