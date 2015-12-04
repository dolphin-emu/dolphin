// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QSortFilterProxyModel>

class ListProxyModel final : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	ListProxyModel(QObject* parent = nullptr);
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
};
