#include <string>
#include <algorithm>
#include <QtGui>
#include "GameList.h"
#include "Resources.h"

#include "FileSearch.h"
#include "ConfigManager.h"
#include "CDUtils.h"
#include "IniFile.h"

#include "Thread.h"//remove

// TODO: Remove this
#include "../resources/no_banner.cpp"//remove
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
		case DAbstractGameList::COLUMN_TITLE:
				return 0 > strcasecmp(one.GetName(indexOne).c_str(),
								other.GetName(indexOther).c_str());
		case DAbstractGameList::COLUMN_NOTES:
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
		case DAbstractGameList::COLUMN_COUNTRY:
			return (one.GetCountry() < other.GetCountry());
		case DAbstractGameList::COLUMN_SIZE:
			return (one.GetFileSize() < other.GetFileSize());
		case DAbstractGameList::COLUMN_PLATFORM:
			return (one.GetPlatform() < other.GetPlatform());
		default:
			return 0 > strcasecmp(one.GetName(indexOne).c_str(),
					other.GetName(indexOther).c_str());
	}
}

void DAbstractGameList::Rescan(ProgressDialogCallbackClass* callback)
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

	void* dialog = callback->OnIsoScanStarted(rFilenames.size());

	if (rFilenames.size() > 0)
	{
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			callback->OnIsoScanProgress(dialog, i, FileName);

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
		callback->OnIsoScanFinished(dialog);
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


DGameList::DGameList()
{
	sourceModel = new QStandardItemModel(5, 7, this);
	setModel(sourceModel);
	setRootIsDecorated(false);
	setAlternatingRowColors(true);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setUniformRowHeights(true);
}

DGameList::~DGameList()
{

}

void DGameList::ScanForIsos()
{
	abstrGameList.Rescan(this);
	RebuildList();
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

QStandardItem* QStandardItemFromPixmap(QPixmap pixmap)
{
	QStandardItem* item = new QStandardItem;
	item->setBackground(QBrush(pixmap));
	item->setSelectable(false);
	item->setSizeHint(pixmap.size());
	return item;
}

class DImageCell : public QAbstractItemDelegate
{
public:
	DImageCell(QPixmap _pixmap, QObject* parent = NULL) : QAbstractItemDelegate(parent), pixmap(_pixmap)
	{

	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
        QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ?
            (option.state & QStyle::State_Active) ? QPalette::Normal : QPalette::Inactive : QPalette::Disabled;
        if (option.state & QStyle::State_Selected)
            painter->fillRect(option.rect, option.palette.color(cg, QPalette::Highlight));
		painter->drawPixmap(option.rect.x(), option.rect.y(), pixmap);

 //       drawFocus(painter, option, option.rect.adjusted(0, 0, -1, -1)); // since we draw the grid ourselves

    QPen pen = painter->pen();
    painter->setPen(option.palette.color(QPalette::Mid));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    painter->setPen(pen);
	}

	QSize sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
	{
		return pixmap.size();
	}

private:
	QPixmap pixmap;
};

void DGameList::RebuildList()
{
	std::vector<GameListItem>& items = abstrGameList.getItems();

	sourceModel->clear();
	sourceModel->setRowCount(items.size());

// Experimental custom item delegate which paints a QImage
//QPixmap pm = QPixmap::fromImage(QImage(items[0].GetImage(), 96, 32, QImage::Format_RGB888));
//setItemDelegateForColumn(0, new DImageCell(pm));

	for (int i = 0; i < (int)items.size(); ++i)
	{
		sourceModel->setItem(i, 0, QStandardItemFromPixmap(Resources::GetPlatformPixmap(items[i].GetPlatform())));

		if(items[i].GetImage())
		{
			sourceModel->setItem(i, 1, QStandardItemFromPixmap(QPixmap::fromImage(QImage(items[i].GetImage(), 96, 32, QImage::Format_RGB888))));
		}
		else
		{
			QPixmap banner;
			banner.loadFromData(no_banner_png, sizeof(no_banner_png));
			sourceModel->setItem(i, 1, QStandardItemFromPixmap(banner));
		}
		sourceModel->setItem(i, 2, new QStandardItem(items[i].GetName(0).c_str()));
		sourceModel->setItem(i, 3, new QStandardItem(items[i].GetDescription(0).c_str()));

		sourceModel->setItem(i, 4, QStandardItemFromPixmap(Resources::GetRegionPixmap(items[i].GetCountry())));

		sourceModel->setItem(i, 5, new QStandardItem(NiceSizeFormat(items[i].GetFileSize())));

		int state;
		IniFile ini;
		ini.Load((std::string(File::GetUserPath(D_GAMECONFIG_IDX)) + (items[i].GetUniqueID()) + ".ini").c_str());
		ini.Get("EmuState", "EmulationStateId", &state);
		sourceModel->setItem(i, 6, QStandardItemFromPixmap(Resources::GetRatingPixmap(state)));
	}
	QStringList columnTitles;
	columnTitles << tr("Platform") << tr("Banner") << tr("Title") << tr("Notes") << tr("Region") << tr("Size") << tr("State");
	sourceModel->setHorizontalHeaderLabels(columnTitles);

	for (int i = 0; i < sourceModel->columnCount(); ++i)
	resizeColumnToContents(i);
}

QString DGameList::GetSelectedFilename()
{
	QModelIndexList indexList = selectedIndexes();
	if (indexList.size() == 0) return QString();
	else return QString(abstrGameList.getItems()[indexList[0].	row()].GetFileName().c_str());
}

void* DGameList::OnIsoScanStarted(int numItems)
{
	QProgressDialog* dialog = new QProgressDialog(tr("Scanning..."), QString(), 0, numItems, this);
	dialog->show();
	return (void*)dialog;
}

void DGameList::OnIsoScanProgress(void* dialogptr, int item, std::string nowScanning)
{
	((QProgressDialog*)dialogptr)->setValue(item);
	((QProgressDialog*)dialogptr)->setLabelText(tr("Scanning %1").arg(nowScanning.c_str()));
}

void DGameList::OnIsoScanFinished(void* dialogptr)
{
	delete (QProgressDialog*)dialogptr;
}
