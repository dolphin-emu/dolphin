// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QStyledItemDelegate>

class TableDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	explicit TableDelegate(QWidget* parent = nullptr);
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;

private:
	void DrawPixmap(QPainter* painter, const QRect& rect, const QPixmap& pixmap) const;
};
