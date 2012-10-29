#include "GameList.h"

Game::Game(const std::string& fileName)
{
    GameListItem item(fileName);
    m_type = "Wii";
    m_logo = "Banner";
    m_name = QString::fromStdString(item.GetName(0));
    m_desc = QString::fromStdString(item.GetDescription(0));
    m_flag = "EU";
    m_size = "0 MB";
    m_star = QString::fromStdString(item.GetIssues());
}
Game::Game(const GameListItem& item)
{
        m_type = "Wii";
        m_logo = "Banner";
        m_name = QString::fromStdString(item.GetName(0));
        m_desc = QString::fromStdString(item.GetDescription(0));
        m_flag = "EU";
        m_size = "0 MB";
        m_star = QString::fromStdString(item.GetIssues());
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
