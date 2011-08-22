#include <string>
#include <algorithm>
#include <QtGui>
#include "GameList.h"
#include "Resources.h"

#include "FileSearch.h"
#include "ConfigManager.h"
#include "CDUtils.h"
#include "IniFile.h"


// TODO: Clean this up!
static int currentColumn = 0;
bool operator < (const GameListItem &one, const GameListItem &other)
{
	int indexOne = 0;
	int indexOther = 0;

	switch (one.GetCountry())
	{
		case DiscIO::IVolume::COUNTRY_JAPAN:
		case DiscIO::IVolume::COUNTRY_USA:
			indexOne = 0;
			break;
		default:
			indexOne = SConfig::GetInstance().m_InterfaceLanguage;
	}

	switch (other.GetCountry())
	{
		case DiscIO::IVolume::COUNTRY_JAPAN:
		case DiscIO::IVolume::COUNTRY_USA:
			indexOther = 0;
			break;
		default:
			indexOther = SConfig::GetInstance().m_InterfaceLanguage;
	}

	switch(currentColumn)
	{
		case AbstractGameBrowser::COLUMN_TITLE:
				return 0 > strcasecmp(one.GetName(indexOne).c_str(),
								other.GetName(indexOther).c_str());
		case AbstractGameBrowser::COLUMN_NOTES:
			{
				// On Gamecube we show the company string, while it's empty on
				// other platforms, so we show the description instead
				std::string cmp1 =
					(one.GetPlatform() == GameListItem::GAMECUBE_DISC) ?
					one.GetCompany() : one.GetDescription(indexOne);
				std::string cmp2 =
					(other.GetPlatform() == GameListItem::GAMECUBE_DISC) ?
					other.GetCompany() : other.GetDescription(indexOther);
				return 0 > strcasecmp(cmp1.c_str(), cmp2.c_str());
			}
		case AbstractGameBrowser::COLUMN_COUNTRY:
			return (one.GetCountry() < other.GetCountry());
		case AbstractGameBrowser::COLUMN_SIZE:
			return (one.GetFileSize() < other.GetFileSize());
		case AbstractGameBrowser::COLUMN_PLATFORM:
			return (one.GetPlatform() < other.GetPlatform());
		default:
			return 0 > strcasecmp(one.GetName(indexOne).c_str(),
					other.GetName(indexOther).c_str());
	}
}

void AbstractGameBrowser::Rescan()
{
	items.clear();
	CFileSearch::XStringVector Directories(SConfig::GetInstance().m_ISOFolder);

	if (SConfig::GetInstance().m_RecursiveISOFolder)
	{
		for (u32 i = 0; i < Directories.size(); i++)
		{
			File::FSTEntry FST_Temp;
			File::ScanDirectoryTree(Directories.at(i).c_str(), FST_Temp);
			for (u32 j = 0; j < FST_Temp.children.size(); j++)
			{
				if (FST_Temp.children.at(j).isDirectory)
				{
					bool duplicate = false;
					for (u32 k = 0; k < Directories.size(); k++)
					{
						if (strcmp(Directories.at(k).c_str(), FST_Temp.children.at(j).physicalName.c_str()) == 0)
						{
							duplicate = true;
							break;
						}
					}
					if (!duplicate)
						Directories.push_back(FST_Temp.children.at(j).physicalName.c_str());
				}
			}
		}
	}

	CFileSearch::XStringVector Extensions;

	if (SConfig::GetInstance().m_ListGC)
		Extensions.push_back("*.gcm");
	if (SConfig::GetInstance().m_ListWii || SConfig::GetInstance().m_ListGC)
	{
		Extensions.push_back("*.iso");
		Extensions.push_back("*.ciso");
		Extensions.push_back("*.gcz");
	}
	if (SConfig::GetInstance().m_ListWad)
		Extensions.push_back("*.wad");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		progressBar->SetRange(0, rFilenames.size());
		progressBar->SetVisible(true);

		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			progressBar->SetValue(i, rFilenames[i]);
			GameListItem ISOFile(rFilenames[i]);
			if (ISOFile.IsValid())
			{
				bool list = true;

				switch(ISOFile.GetPlatform())
				{
					case GameListItem::WII_DISC:
						if (!SConfig::GetInstance().m_ListWii)
							list = false;
						break;
					case GameListItem::WII_WAD:
						if (!SConfig::GetInstance().m_ListWad)
							list = false;
						break;
					default:
						if (!SConfig::GetInstance().m_ListGC)
							list = false;
						break;
				}

				switch(ISOFile.GetCountry())
				{
					case DiscIO::IVolume::COUNTRY_TAIWAN:
						if (!SConfig::GetInstance().m_ListTaiwan)
							list = false;
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
					default:
						if (!SConfig::GetInstance().m_ListPal)
							list = false;
						break;
				}

				if (list)
					items.push_back(ISOFile);
			}
		}
		progressBar->SetVisible(false);
	}

	if (SConfig::GetInstance().m_ListDrives)
	{
		std::vector<std::string> drives = cdio_get_devices();
		GameListItem * Drive[24];
		// Another silly Windows limitation of 24 drive letters
		for (u32 i = 0; i < drives.size() && i < 24; i++)
		{
			Drive[i] = new GameListItem(drives[i].c_str());
			if (Drive[i]->IsValid())
				items.push_back(*Drive[i]);
		}
	}

	std::sort(items.begin(), items.end());
}

DGameList::DGameList(const AbstractGameBrowser& gameBrowser) : abstrGameBrowser(gameBrowser)
{
	sourceModel = new QStandardItemModel(this);
	setModel(sourceModel);
	setRootIsDecorated(false);
	setAlternatingRowColors(true);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setUniformRowHeights(true);

	// TODO: Is activated the correct signal?
	connect(this, SIGNAL(activated(const QModelIndex&)), this, SLOT(OnItemActivated(const QModelIndex&)));
}

DGameList::~DGameList()
{

}

QString NiceSizeFormat(s64 _size)
{
    const char* sizes[] = {"b", "KB", "MB", "GB", "TB", "PB", "EB"};
    int s = 0;
    int frac = 0;

    while (_size > (s64)1024)
    {
        s++;
        frac   = (int)_size & 1023;
        _size /= (s64)1024;
    }

    float f = (float)_size + ((float)frac / 1024.0f);

    char tempstr[32];
    sprintf(tempstr,"%3.1f %s", f, sizes[s]);
    QString NiceString(tempstr);
    return NiceString;
}

void DGameList::RefreshView()
{
	const std::vector<GameListItem>& items = abstrGameBrowser.getItems();

	sourceModel->clear();
	sourceModel->setRowCount(items.size());

	// TODO: Remove those /2 hacks, which are required because the source pixmaps use too much blank space
	for (int i = 0; i < (int)items.size(); ++i)
	{
		QStandardItem* platformItem = new QStandardItem;
		platformItem->setData(QVariant::fromValue(Resources::GetPlatformPixmap(items[i].GetPlatform())),Qt::DecorationRole);
		platformItem->setSizeHint(Resources::GetPlatformPixmap(items[i].GetPlatform()).size()/2);
		sourceModel->setItem(i, 0, platformItem);

		QPixmap bannerPixmap = (items[i].GetImage().empty()) ?
								Resources::GetPixmap(Resources::BANNER_MISSING) :
								QPixmap::fromImage(QImage(&(items[i].GetImage()[0]), 96, 32, QImage::Format_RGB888));
		QStandardItem* bannerItem = new QStandardItem;
		bannerItem->setData(QVariant::fromValue(bannerPixmap), Qt::DecorationRole);
		bannerItem->setSizeHint(QSize(96, 34));
		sourceModel->setItem(i, 1, bannerItem);

		sourceModel->setItem(i, 2, new QStandardItem(items[i].GetName(0).c_str()));
		sourceModel->setItem(i, 3, new QStandardItem(items[i].GetDescription(0).c_str()));

		QStandardItem* regionItem = new QStandardItem;
		regionItem->setData(QVariant::fromValue(Resources::GetRegionPixmap(items[i].GetCountry())), Qt::DecorationRole);
		regionItem->setSizeHint(Resources::GetRegionPixmap(items[i].GetCountry()).size()/3);
		sourceModel->setItem(i, 4, regionItem);

		sourceModel->setItem(i, 5, new QStandardItem(NiceSizeFormat(items[i].GetFileSize())));

		int state;
		IniFile ini;
		ini.Load((std::string(File::GetUserPath(D_GAMECONFIG_IDX)) + (items[i].GetUniqueID()) + ".ini").c_str());
		ini.Get("EmuState", "EmulationStateId", &state);
		QStandardItem* ratingItem = new QStandardItem;
		ratingItem->setData(QVariant::fromValue(Resources::GetRatingPixmap(state)), Qt::DecorationRole);
		ratingItem->setSizeHint(Resources::GetRatingPixmap(state).size()*0.6f);
		sourceModel->setItem(i, 6, ratingItem);
	}
	QStringList columnTitles;
	columnTitles << tr("Platform") << tr("Banner") << tr("Title") << tr("Notes") << tr("Region") << tr("Size") << tr("State");
	sourceModel->setHorizontalHeaderLabels(columnTitles);

	for (int i = 0; i < sourceModel->columnCount(); ++i)
		resizeColumnToContents(i);
}

GameListItem const* DGameList::GetSelectedISO() const
{
	// Usually, one would just use QTreeView::selectedIndices(), but there is a bug in the open source editions
	// of the QT SDK for Windows which causes a failed assertion.
	// Thus, we just loop through the whole game list and check for each item if it's the selected one.
	for (int i = 0; i < model()->rowCount(rootIndex()); ++i)
	{
		QModelIndex index = model()->index(i, 0, rootIndex());
		if (selectionModel()->isSelected(index))
			return &(abstrGameBrowser.getItems()[i]);
	}
	return NULL;
}

void DGameList::OnItemActivated(const QModelIndex& index)
{
	// TODO: Untested
	if (index.row() < (int)abstrGameBrowser.getItems().size())
		emit StartGame();
}


DGameTable::DGameTable(const AbstractGameBrowser& gameBrowser) : abstrGameBrowser(gameBrowser), num_columns(5)
{
	sourceModel = new QStandardItemModel(this);
	setModel(sourceModel);
	setAlternatingRowColors(true);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	horizontalHeader()->setVisible(false);
	verticalHeader()->setVisible(false);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setGridStyle(Qt::NoPen);

	// TODO: Is activated the correct signal?
	connect(this, SIGNAL(activated(QModelIndex)), this, SLOT(OnItemActivated(QModelIndex)));
}

DGameTable::~DGameTable()
{

}

void DGameTable::RefreshView()
{
	// TODO: There's still some problems if you scroll away from a cell and back to it => banner won't get redrawn
	const std::vector<GameListItem>& items = abstrGameBrowser.getItems();

	// Use 146 if there are no columns, yet, and the actual column width otherwise
	num_columns = viewport()->width() / qMax(146, columnWidth(0));

	sourceModel->clear();
	sourceModel->setRowCount(qMax<int>(0, items.size()-1) / num_columns + 1);
	sourceModel->setColumnCount(num_columns);

	for (int i = 0; i < (int)items.size(); ++i)
	{
		QStandardItem* item = new QStandardItem;
		if(!items[i].GetImage().empty())
		{
			u8 const* src = &(items[i].GetImage()[0]);
			QMap<u8 const*,QPixmap>::iterator it = pixmap_cache.find(src);
			if (it == pixmap_cache.end())
				it = pixmap_cache.insert(src, QPixmap::fromImage(QImage(src, 96, 32, QImage::Format_RGB888)).scaled(QSize(144,48), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			item->setData(QVariant::fromValue(it.value()), Qt::DecorationRole);
		}
		else
		{
			QPixmap banner = Resources::GetPixmap(Resources::BANNER_MISSING);
			banner = banner.scaled(QSize(144,48), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			item->setData(QVariant::fromValue(banner), Qt::DecorationRole);
		}
		item->setSizeHint(QSize(146, 50));
		sourceModel->setItem(i / num_columns, i % num_columns, item);
	}
	// TODO: Set the background colors of the remaining cells to the background color

	resizeColumnsToContents();
	resizeRowsToContents();
}

GameListItem const* DGameTable::GetSelectedISO() const
{
	// Usually, one would just use QTreeView::selectedIndices(), but there is a bug in the open source editions
	// of the QT SDK for Windows which causes a failed assertion.
	// Thus, we just loop through the whole game list and check for each item if it's the selected one.
	for (unsigned int i = 0; i < abstrGameBrowser.getItems().size(); ++i)
	{
		QModelIndex index = model()->index(i / num_columns, i % num_columns, rootIndex());
		if (selectionModel()->isSelected(index))
			return &(abstrGameBrowser.getItems()[i]);
	}
	return NULL;
}

void DGameTable::OnItemActivated(const QModelIndex& index)
{
	// TODO: Do the same when enter is pressed!
	if (index.row() * num_columns + index.column() < abstrGameBrowser.getItems().size())
		emit StartGame();
}

void DGameTable::resizeEvent(QResizeEvent* event)
{
	// Dynamically adjust the number of columns so that the central area is always filled
	if (num_columns > 1)
		if (event->size().width() < columnViewportPosition(num_columns-1) + columnWidth(0) - columnViewportPosition(0))
			RefreshView();

	if (event->size().width() > columnViewportPosition(num_columns-1) + 2*columnWidth(0) - columnViewportPosition(0))
		RefreshView();
}

class DGameListProgressBar : public QProgressBar, public AbstractProgressBar
{
public:
	DGameListProgressBar(QWidget* parent = NULL) : QProgressBar(parent) {}
	virtual ~DGameListProgressBar() {}

	void SetValue(int value, std::string FileName) { setValue(value); setFormat(tr("Scanning ") + QString(FileName.c_str())); }
	void SetRange(int min, int max) { setRange(min, max); }
	void SetVisible(bool visible) { setVisible(visible); }
};

DGameBrowser::DGameBrowser(Style initialStyle, QWidget* parent) : QWidget(parent), progBar(new DGameListProgressBar), abstrGameBrowser(progBar), gameBrowser(NULL), style(initialStyle)
{
	progBar->SetVisible(false);

	mainLayout = new QGridLayout;
	mainLayout->addWidget(progBar, 1, 0);

	SetStyle(style);

	setLayout(mainLayout);
}

DGameBrowser::~DGameBrowser()
{

}

GameListItem const* DGameBrowser::GetSelectedISO() const
{
	return gameBrowser->GetSelectedISO();
}

void DGameBrowser::ScanForIsos()
{
	dynamic_cast<QWidget*>(gameBrowser)->setEnabled(false);
	abstrGameBrowser.Rescan();
	gameBrowser->RefreshView();
	dynamic_cast<QWidget*>(gameBrowser)->setEnabled(true);
}

void DGameBrowser::SetStyle(Style layout)
{
	if (style == layout && gameBrowser) return;
	style = layout;

	if (gameBrowser) dynamic_cast<QWidget*>(gameBrowser)->close();
	if (layout == Style_List) gameBrowser = new DGameList(abstrGameBrowser);
	else if (layout == Style_Grid) gameBrowser = new DGameTable(abstrGameBrowser);

	gameBrowser->RefreshView();
	connect(dynamic_cast<QObject*>(gameBrowser), SIGNAL(StartGame()), this, SIGNAL(StartGame()));

	mainLayout->addWidget(dynamic_cast<QWidget*>(gameBrowser), 0, 0);
}

DGameBrowser::Style DGameBrowser::GetStyle()
{
	return style;
}
