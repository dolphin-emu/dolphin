#include "GameList.h"
#include <QStringList>
#include <QDebug>

Game::Game(const GameListItem& item)
{
        int platform = item.GetPlatform();
        switch (platform)
        {
            case 0: m_type = "GC";
                    break;
            case 1: m_type = "Wii";
                    break;
            case 2: m_type = "WAD";
                    break;
            default: m_type = "Unknown";
                    break;
        }
        m_logo = "Banner";
        m_name = QString::fromStdString(item.GetName(0));
        m_desc = QString::fromStdString(item.GetDescription(0));

        int country = item.GetCountry();
        switch(country)
        {
            case DiscIO::IVolume::COUNTRY_JAPAN:    m_flag = "Japan";
                 break;
            case DiscIO::IVolume::COUNTRY_USA:      m_flag = "USA";
                 break;
            case DiscIO::IVolume::COUNTRY_TAIWAN:   m_flag = "Taiwan";
                 break;
            case DiscIO::IVolume::COUNTRY_KOREA:    m_flag = "Korea";
                 break;
            case DiscIO::IVolume::COUNTRY_FRANCE:   m_flag = "France";
                 break;
            case DiscIO::IVolume::COUNTRY_ITALY:    m_flag = "Italy";
                 break;
            default: m_flag = "PAL";
                 break;

        }

        QStringList list;
        list << "KB" << "MB" << "GB" << "TB";
        QStringListIterator i(list);
        QString unit("b");
        double num = item.GetFileSize();
        while(num >= 1024.0 && i.hasNext())
        {
            unit = i.next();
            num /= 1024.0;
        }
        m_size = QString().setNum(num, 'f', 1) + " " + unit;

        static const char* const emuState[] = { "Broken", "Intro", "In-Game", "Playable", "Perfect" };
        int rating;
        IniFile ini;
        ini.Load((std::string(File::GetUserPath(D_GAMECONFIG_IDX)) + (item.GetUniqueID()) + ".ini").c_str());
        qDebug() << (std::string(File::GetUserPath(D_GAMECONFIG_IDX)) + (item.GetUniqueID()) + ".ini").c_str();
        ini.Get("EmuState", "EmulationStateId", &rating);
        qDebug() << rating;
        if (rating > 0 && rating < 6)
            m_star = emuState[rating - 1];
        else
            m_star = "Unknown";
}


GameList::GameList(QObject *parent)
{
    QHash<int, QByteArray> roles;
    roles[TypeRole] = "type";
    roles[LogoRole] = "logo";
    roles[NameRole] = "name";
    roles[DescRole] = "desc";
    roles[FlagRole] = "flag";
    roles[SizeRole] = "size";
    roles[StarRole] = "star";
    setRoleNames(roles);
}
void GameList::addGame(const Game& item)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_games << item;
    endInsertRows();
    emit countChanged();
}
int GameList::rowCount(const QModelIndex & parent) const {
    return m_games.count();
}

QVariant GameList::data(const QModelIndex & index, int role) const {
    if (index.row() < 0 || index.row() > m_games.count())
        return QVariant();

    const Game &item = m_games[index.row()];
    if (role == TypeRole)
        return item.type();
    else if (role == LogoRole)
        return item.logo();
    else if (role == NameRole)
        return item.name();
    else if (role == DescRole)
        return item.desc();
    else if (role == FlagRole)
        return item.flag();
    else if (role == SizeRole)
        return item.size();
    else if (role == StarRole)
        return item.star();
    return QVariant();
}
void GameList::clear()
{
    beginResetModel();
    reset();
    endResetModel();
}
