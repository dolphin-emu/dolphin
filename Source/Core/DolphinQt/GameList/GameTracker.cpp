// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CDUtils.h"
#include "Common/FileSearch.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/GameList/GameGrid.h"
#include "DolphinQt/GameList/GameTracker.h"
#include "DolphinQt/GameList/GameTree.h"

void AbstractGameList::AddGames(QList<GameFile*> items)
{
	for (GameFile* o : items)
		AddGame(o);
}
void AbstractGameList::RemoveGames(QList<GameFile*> items)
{
	for (GameFile* o : items)
		RemoveGame(o);
}


DGameTracker::DGameTracker(QWidget* parent_widget)
	: QStackedWidget(parent_widget),
	  m_watcher(this)
{
	connect(&m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(ScanForGames()));

	m_tree_widget = new DGameTree(this);
	addWidget(m_tree_widget);
	connect(m_tree_widget, SIGNAL(StartGame()), this, SIGNAL(StartGame()));

	m_grid_widget = new DGameGrid(this);
	addWidget(m_grid_widget);
	connect(m_grid_widget, SIGNAL(StartGame()), this, SIGNAL(StartGame()));

	SetViewStyle(STYLE_LIST);
}

DGameTracker::~DGameTracker()
{
	for (GameFile* file : m_games.values())
		delete file;
}

void DGameTracker::SetViewStyle(GameListStyle newStyle)
{
	if (newStyle == m_current_style)
		return;
	m_current_style = newStyle;

	if (newStyle == STYLE_LIST || newStyle == STYLE_TREE)
	{
		m_tree_widget->SelectGame(SelectedGame());
		setCurrentWidget(m_tree_widget);
		m_tree_widget->SetViewStyle(newStyle);
	}
	else
	{
		m_grid_widget->SelectGame(SelectedGame());
		setCurrentWidget(m_grid_widget);
		m_grid_widget->SetViewStyle(newStyle);
	}
}

GameFile* DGameTracker::SelectedGame()
{
	if (currentWidget() == m_grid_widget)
		return m_grid_widget->SelectedGame();
	else
		return m_tree_widget->SelectedGame();
}

void DGameTracker::ScanForGames()
{
	setDisabled(true);

	CFileSearch::XStringVector dirs(SConfig::GetInstance().m_ISOFolder);

	if (SConfig::GetInstance().m_RecursiveISOFolder)
	{
		for (u32 i = 0; i < dirs.size(); i++)
		{
			File::FSTEntry FST_Temp;
			File::ScanDirectoryTree(dirs[i], FST_Temp);
			for (auto& entry : FST_Temp.children)
			{
				if (entry.isDirectory)
				{
					bool duplicate = false;
					for (auto& dir : dirs)
					{
						if (dir == entry.physicalName)
						{
							duplicate = true;
							break;
						}
					}
					if (!duplicate)
						dirs.push_back(entry.physicalName);
				}
			}
		}
	}

	for (std::string& dir : dirs)
		m_watcher.addPath(QString::fromStdString(dir));

	CFileSearch::XStringVector exts;
	if (SConfig::GetInstance().m_ListGC)
	{
		exts.push_back("*.gcm");
		exts.push_back("*.gcz");
	}
	if (SConfig::GetInstance().m_ListWii || SConfig::GetInstance().m_ListGC)
	{
		exts.push_back("*.iso");
		exts.push_back("*.ciso");
		exts.push_back("*.wbfs");
	}
	if (SConfig::GetInstance().m_ListWad)
		exts.push_back("*.wad");

	CFileSearch FileSearch(exts, dirs);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();
	QList<GameFile*> newItems;
	QStringList allItems;

	if (!rFilenames.empty())
	{
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], nullptr, &FileName, nullptr);
			QString NameAndPath = QString::fromStdString(rFilenames[i]);
			allItems.append(NameAndPath);

			if (m_games.keys().contains(NameAndPath))
				continue;

			GameFile* obj = new GameFile(rFilenames[i]);
			if (obj->IsValid())
			{
				bool list = true;

				switch(obj->GetCountry())
				{
					case DiscIO::IVolume::COUNTRY_AUSTRALIA:
						if (!SConfig::GetInstance().m_ListAustralia)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_GERMANY:
						if (!SConfig::GetInstance().m_ListGermany)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_RUSSIA:
						if (!SConfig::GetInstance().m_ListRussia)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_UNKNOWN:
						if (!SConfig::GetInstance().m_ListUnknown)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_TAIWAN:
						if (!SConfig::GetInstance().m_ListTaiwan)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_KOREA:
						if (!SConfig::GetInstance().m_ListKorea)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_JAPAN:
						if (!SConfig::GetInstance().m_ListJap)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_USA:
						if (!SConfig::GetInstance().m_ListUsa)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_FRANCE:
						if (!SConfig::GetInstance().m_ListFrance)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_ITALY:
						if (!SConfig::GetInstance().m_ListItaly)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_SPAIN:
						if (!SConfig::GetInstance().m_ListSpain)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_NETHERLANDS:
						if (!SConfig::GetInstance().m_ListNetherlands)
							list = false;
						break;
					default:
						if (!SConfig::GetInstance().m_ListPal)
							list = false;
						break;
				}

				if (list)
					newItems.append(obj);
			}
		}
	}

	// Process all the new GameFiles
	for (GameFile* o : newItems)
		m_games.insert(o->GetFileName(), o);

	// Check for games that were removed
	QList<GameFile*> removedGames;
	for (QString& path : m_games.keys())
	{
		if (!allItems.contains(path))
		{
			removedGames.append(m_games.value(path));
			m_games.remove(path);
		}
	}

	m_tree_widget->AddGames(newItems);
	m_grid_widget->AddGames(newItems);

	m_tree_widget->RemoveGames(removedGames);
	m_grid_widget->RemoveGames(removedGames);

	for (GameFile* file : removedGames)
		delete file;

	setDisabled(false);
}

void DGameTracker::SelectLastBootedGame()
{
	if (!SConfig::GetInstance().m_LastFilename.empty() && File::Exists(SConfig::GetInstance().m_LastFilename))
	{
		QString lastfilename = QString::fromStdString(SConfig::GetInstance().m_LastFilename);
		for (GameFile* game : m_games.values())
		{
			if (game->GetFileName() == lastfilename)
			{
				m_tree_widget->SelectGame(game);
				break;
			}

		}
	}
}
