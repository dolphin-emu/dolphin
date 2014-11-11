// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QListWidget>

#include "DolphinQt/GameList/GameTracker.h"

// Predefinitions
namespace Ui
{
class DGameGrid;
}

class DGameGrid : public QListWidget, public AbstractGameList
{
	Q_OBJECT

public:
	explicit DGameGrid(QWidget* parent_widget = nullptr);
	~DGameGrid();

	// AbstractGameList stuff
	virtual GameFile* SelectedGame();
	virtual void SelectGame(GameFile* game);

	virtual void SetViewStyle(GameListStyle newStyle);

	virtual void AddGame(GameFile* item);
	virtual void RemoveGame(GameFile* item);

signals:
	void StartGame();

private:
	std::unique_ptr<Ui::DGameGrid> m_ui;

	QMap<QListWidgetItem*, GameFile*> m_items;
	GameListStyle m_current_style;
};
