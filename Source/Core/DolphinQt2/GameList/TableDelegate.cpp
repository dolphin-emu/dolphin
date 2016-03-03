// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QPainter>

#include "DolphinQt2/Resources.h"
#include "DolphinQt2/GameList/GameListModel.h"
#include "DolphinQt2/GameList/TableDelegate.h"

static QSize NORMAL_BANNER_SIZE(96, 32);

// Convert an integer size to a friendly string representation.
static QString FormatSize(qint64 size)
{
	QStringList units{
		QStringLiteral("KB"),
		QStringLiteral("MB"),
		QStringLiteral("GB"),
		QStringLiteral("TB")
	};
	QStringListIterator i(units);
	QString unit = QStringLiteral("B");
	double num = (double) size;
	while (num > 1024.0 && i.hasNext())
	{
		unit = i.next();
		num /= 1024.0;
	}
	return QStringLiteral("%1 %2").arg(QString::number(num, 'f', 1)).arg(unit);
}

TableDelegate::TableDelegate(QWidget* parent) : QStyledItemDelegate(parent)
{
}

void TableDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QVariant data = index.data(Qt::DisplayRole);
	switch (index.column())
	{
	case GameListModel::COL_PLATFORM:
		DrawPixmap(painter, option.rect, Resources::GetPlatform(data.toInt()));
		break;
	case GameListModel::COL_COUNTRY:
		DrawPixmap(painter, option.rect, Resources::GetCountry(data.toInt()));
		break;
	case GameListModel::COL_RATING:
		DrawPixmap(painter, option.rect, Resources::GetRating(data.toInt()));
		break;
	case GameListModel::COL_BANNER:
		DrawPixmap(painter, option.rect, data.value<QPixmap>().scaled(
					NORMAL_BANNER_SIZE,
					Qt::KeepAspectRatio,
					Qt::SmoothTransformation));
		break;
	case GameListModel::COL_SIZE:
		painter->drawText(option.rect, Qt::AlignCenter, FormatSize(data.toULongLong()));
		break;
	// Fall through.
	case GameListModel::COL_ID:
	case GameListModel::COL_TITLE:
	case GameListModel::COL_DESCRIPTION:
	case GameListModel::COL_MAKER:
		painter->drawText(option.rect, Qt::AlignVCenter, data.toString());
		break;
	default: break;
	}
}

QSize TableDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	switch (index.column())
	{
	case GameListModel::COL_PLATFORM:
		return Resources::GetPlatform(0).size();
	case GameListModel::COL_COUNTRY:
		return Resources::GetCountry(0).size();
	case GameListModel::COL_RATING:
		return Resources::GetRating(0).size();
	case GameListModel::COL_BANNER:
		return NORMAL_BANNER_SIZE;
	default: return QSize(0, 0);
	}
}

void TableDelegate::DrawPixmap(QPainter* painter, const QRect& rect, const QPixmap& pixmap) const
{
	// We don't want to stretch the pixmap out, so center it in the rect.
	painter->drawPixmap(
			rect.left() + (rect.width() - pixmap.width()) / 2,
			rect.top() + (rect.height() - pixmap.height()) / 2,
			pixmap);
}
