// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QList>
#include <QMap>
#include <QStackedWidget>

#include "DolphinQt/GameList/GameFile.h"

// Predefinitions
class DGameGrid;
class DGameTree;

enum GameListStyle
{
	STYLE_LIST,
	STYLE_TREE,
	STYLE_GRID,
	STYLE_ICON
};

class AbstractGameList
{
public:
	virtual GameFile* SelectedGame() = 0;
	virtual void SelectGame(GameFile* game) = 0;

	virtual void SetViewStyle(GameListStyle newStyle) = 0;

	virtual void AddGame(GameFile* item) = 0;
	void AddGames(QList<GameFile*> items);

	virtual void RemoveGame(GameFile* item) = 0;
	void RemoveGames(QList<GameFile*> items);
};

class DGameTracker : public QStackedWidget
{
	Q_OBJECT

public:
	DGameTracker(QWidget* parent_widget = nullptr);
	~DGameTracker();

	GameListStyle ViewStyle() const { return m_current_style; }
	void SetViewStyle(GameListStyle newStyle);

	GameFile* SelectedGame();
	void SelectLastBootedGame();

signals:
	void StartGame();

public slots:
	void ScanForGames();

private:
	QMap<QString, GameFile*> m_games;
	QFileSystemWatcher m_watcher;

	GameListStyle m_current_style;
	DGameGrid* m_grid_widget = nullptr;
	DGameTree* m_tree_widget = nullptr;
};
