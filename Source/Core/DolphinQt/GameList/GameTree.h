// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QTreeWidget>

#include "DolphinQt/GameList/GameTracker.h"

// Predefinitions
namespace Ui
{
class DGameTree;
}

class DGameTree : public QTreeWidget, public AbstractGameList
{
	Q_OBJECT

public:
	explicit DGameTree(QWidget* parent_widget = nullptr);
	~DGameTree();

	// AbstractGameList stuff
	virtual GameFile* SelectedGame();
	virtual void SelectGame(GameFile* game);

	virtual void SetViewStyle(GameListStyle newStyle);

	virtual void AddGame(GameFile* item);
	virtual void RemoveGame(GameFile* item);

signals:
	void StartGame();

private slots:
	void ItemActivated(QTreeWidgetItem* item);

private:
	enum Columns
	{
		COL_TYPE = 0,
		COL_BANNER = 1,
		COL_TITLE = 2,
		COL_DESCRIPTION = 3,
		COL_REGION = 4,
		COL_SIZE = 5,
		COL_STATE = 6
	};

	std::unique_ptr<Ui::DGameTree> m_ui;
	GameListStyle m_current_style;

	QMap<QTreeWidgetItem*, GameFile*> m_items;
	QMap<QString, QTreeWidgetItem*> m_path_nodes;

	void RebuildTree();
	void ResizeAllCols();
};
