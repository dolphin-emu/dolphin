// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QSortFilterProxyModel>

// This subclass of QSortFilterProxyModel transforms the raw GameFile data
// into presentable icons, and allows for sorting and filtering.
// For instance, the GameListModel exposes country as an integer, so this
// class converts that into a flag, while still allowing sorting on the
// underlying integer.
class GameListProxyModel final : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	GameListProxyModel(QObject* parent = nullptr);
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
};
