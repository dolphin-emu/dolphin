// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ui_GameGrid.h"

#include "Common/StdMakeUnique.h"

#include "DolphinQt/GameList/GameGrid.h"

// Game banner image size
static const u32 GRID_BANNER_WIDTH = 144;
static const u32 GRID_BANNER_HEIGHT = 48;

static const u32 ICON_BANNER_WIDTH = 64;
static const u32 ICON_BANNER_HEIGHT = 64;

DGameGrid::DGameGrid(QWidget* parent_widget) :
	QListWidget(parent_widget)
{
	m_ui = std::make_unique<Ui::DGameGrid>();
	m_ui->setupUi(this);
	SetViewStyle(STYLE_GRID);

	connect(this, SIGNAL(itemActivated(QListWidgetItem*)), this, SIGNAL(StartGame()));
}

DGameGrid::~DGameGrid()
{
	for (QListWidgetItem* i : m_items.keys())
		delete i;
}

GameFile* DGameGrid::SelectedGame()
{
	if (!selectedItems().empty())
		return m_items.value(selectedItems().at(0));
	else
		return nullptr;
}

void DGameGrid::SelectGame(GameFile* game)
{
	if (game == nullptr)
		return;
	if (!selectedItems().empty())
		selectedItems().at(0)->setSelected(false);
	m_items.key(game)->setSelected(true);
}

void DGameGrid::SetViewStyle(GameListStyle newStyle)
{
	if (newStyle == STYLE_GRID)
	{
		m_current_style = STYLE_GRID;
		setIconSize(QSize(GRID_BANNER_WIDTH, GRID_BANNER_HEIGHT));
		setViewMode(QListView::IconMode);
	}
	else
	{
		m_current_style = STYLE_ICON;
		setIconSize(QSize(ICON_BANNER_WIDTH, ICON_BANNER_HEIGHT));
		setViewMode(QListView::ListMode);
	}

	// QListView resets this when you change the view mode, so let's set it again
	setDragEnabled(false);
}

void DGameGrid::AddGame(GameFile* item)
{
	if (m_items.values().contains(item))
		return;
	m_items.values().append(item);

	QListWidgetItem* i = new QListWidgetItem;
	i->setIcon(QIcon(item->GetBitmap()
		.scaled(GRID_BANNER_WIDTH, GRID_BANNER_HEIGHT, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
	i->setText(item->GetName(0));
	if (item->IsCompressed())
		i->setTextColor(QColor("#00F"));

	addItem(i);
	m_items.insert(i, item);
}

void DGameGrid::RemoveGame(GameFile* item)
{
	if (!m_items.values().contains(item))
		return;

	QListWidgetItem* i = m_items.key(item);
	m_items.remove(i);
	delete i;
}
