// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ui_GameTree.h"

#include "Common/StdMakeUnique.h"

#include "DolphinQt/GameList/GameTree.h"

#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

// Game banner image size
static const u32 BANNER_WIDTH = 96;
static const u32 BANNER_HEIGHT = 32;

DGameTree::DGameTree(QWidget* parent_widget) :
	QTreeWidget(parent_widget)
{
	m_ui = std::make_unique<Ui::DGameTree>();
	m_ui->setupUi(this);

	SetViewStyle(STYLE_TREE);
	setIconSize(QSize(BANNER_WIDTH, BANNER_HEIGHT));
	sortByColumn(COL_TITLE);

	connect(this, SIGNAL(itemActivated(QTreeWidgetItem*, int)), this, SLOT(ItemActivated(QTreeWidgetItem*)));
}

DGameTree::~DGameTree()
{
	int count = topLevelItemCount();
	for (int a = 0; a < count; a++)
		takeTopLevelItem(0);

	for (QTreeWidgetItem* i : m_path_nodes.values())
	{
		count = i->childCount();
		for (int a = 0; a < count; a++)
			i->takeChild(0);
	}

	for (QTreeWidgetItem* i : m_path_nodes.values())
		delete i;
	for (QTreeWidgetItem* i : m_items.keys())
		delete i;
}

void DGameTree::ResizeAllCols()
{
	for (int i = 0; i < columnCount(); i++)
		resizeColumnToContents(i);
}

void DGameTree::ItemActivated(QTreeWidgetItem* item)
{
	if (!m_path_nodes.values().contains(item))
		emit StartGame();
}

GameFile* DGameTree::SelectedGame()
{
	if (!selectedItems().empty())
		return m_items.value(selectedItems().at(0));
	else
		return nullptr;
}

void DGameTree::SelectGame(GameFile* game)
{
	if (game == nullptr)
		return;
	if (!selectedItems().empty())
		selectedItems().at(0)->setSelected(false);
	m_items.key(game)->setSelected(true);
}

void DGameTree::SetViewStyle(GameListStyle newStyle)
{
	if (newStyle == STYLE_LIST)
	{
		m_current_style = STYLE_LIST;
		setIndentation(0);
		RebuildTree();
	}
	else
	{
		m_current_style = STYLE_TREE;
		setIndentation(20);
		RebuildTree();
	}
}

void DGameTree::AddGame(GameFile* item)
{
	if (m_items.values().contains(item))
		return;

	QString folder = item->GetFolderName();
	if (!m_path_nodes.contains(folder))
	{
		QTreeWidgetItem* i = new QTreeWidgetItem;
		i->setText(0, folder);
		m_path_nodes.insert(folder, i);
		if (m_current_style == STYLE_TREE)
			addTopLevelItem(i);
	}

	QTreeWidgetItem* i = new QTreeWidgetItem;
	i->setIcon(COL_TYPE, QIcon(Resources::GetPlatformPixmap(item->GetPlatform())));
	i->setIcon(COL_BANNER, QIcon(item->GetBitmap()));
	i->setText(COL_TITLE, item->GetName(0));
	i->setText(COL_DESCRIPTION, item->GetDescription());
	i->setIcon(COL_REGION, QIcon(Resources::GetRegionPixmap(item->GetCountry())));
	i->setText(COL_SIZE, NiceSizeFormat(item->GetFileSize()));
	if (item->IsCompressed())
	{
		for (int col = 0; col < columnCount(); col++)
			i->setTextColor(col, QColor("#00F"));
	}
	m_items.insert(i, item);

	RebuildTree(); // TODO: only call this once per group of items added
}

void DGameTree::RemoveGame(GameFile* item)
{
	if (!m_items.values().contains(item))
		return;
	QTreeWidgetItem* i = m_items.key(item);
	m_items.remove(i);
	delete i;
}

void DGameTree::RebuildTree()
{
	GameFile* currentGame = SelectedGame();

	int count = topLevelItemCount();
	for (int a = 0; a < count; a++)
		takeTopLevelItem(0);

	for (QTreeWidgetItem* i : m_path_nodes.values())
	{
		count = i->childCount();
		for (int a = 0; a < count; a++)
			i->takeChild(0);
	}

	if (m_current_style == STYLE_TREE)
	{
		for (QTreeWidgetItem* i : m_path_nodes.values())
			addTopLevelItem(i);
		for (QTreeWidgetItem* i : m_items.keys())
			m_path_nodes.value(m_items.value(i)->GetFolderName())->addChild(i);
	}
	else
	{
		for (QTreeWidgetItem* i : m_items.keys())
			addTopLevelItem(i);
	}

	expandAll();
	ResizeAllCols();
	SelectGame(currentGame);
}
