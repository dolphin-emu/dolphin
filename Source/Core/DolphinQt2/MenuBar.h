// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMenu>
#include <QMenuBar>

class MenuBar final : public QMenuBar
{
	Q_OBJECT

public:
	explicit MenuBar(QWidget* parent = nullptr);

signals:
	void Open();
	void Exit();

	void ShowTable();
	void ShowList();

private:
	void AddFileMenu();
	void AddViewMenu();

	void AddGameListTypeSection(QMenu* view_menu);
	void AddTableColumnsMenu(QMenu* view_menu);
};
