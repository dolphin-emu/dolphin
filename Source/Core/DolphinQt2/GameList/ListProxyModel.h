// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QSortFilterProxyModel>

// This subclass of QSortFilterProxyModel transforms the raw data into a
// single-column large icon + name to be displayed in a QListView.
class ListProxyModel final : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	explicit ListProxyModel(QObject* parent = nullptr);
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
};
