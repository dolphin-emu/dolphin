#include "interqt.h"

InterQt::InterQt(QObject *parent) : QObject(parent)
{
    pathList = new QList<QString>;
    for (std::vector<std::string>::iterator it = SConfig::GetInstance().m_ISOFolder.begin(); it != SConfig::GetInstance().m_ISOFolder.end(); ++it)
        pathList->append(QString::fromStdString(*it));
    refreshList();
}

// TODO: Qtify this (where it makes sense)
void InterQt::refreshList()
{
    games.clear();
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
        //progressBar->SetRange(0, rFilenames.size());
        //progressBar->SetVisible(true);

        for (u32 i = 0; i < rFilenames.size(); i++)
        {
            std::string FileName;
            SplitPath(rFilenames[i], NULL, &FileName, NULL);

            //progressBar->SetValue(i, rFilenames[i]);
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
                    games.addGame(ISOFile);
            }
        }
        //progressBar->SetVisible(false);
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
                games.addGame(*Drive[i]);
        }
    }

//    std::sort(games.begin(), games.end());
}
void InterQt::startPauseEmu()
{
}
void InterQt::loadISO()
{
}
void InterQt::addISOFolder(QString path)
{
    pathList->append(path);
    SConfig::GetInstance().m_ISOFolder.clear();
    for (int i = 0; i < pathList->count(); ++i)
        SConfig::GetInstance().m_ISOFolder.push_back(pathList->at(i).toStdString());
    refreshList();
}
void InterQt::stopEmu()
{
}
void InterQt::switchFullscreen()
{
}
void InterQt::takeScreenshot()
{
}

