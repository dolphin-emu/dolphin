// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QStringListIterator>

#include "Utils.h"

QString NiceSizeFormat(s64 size)
{
	QStringList list = {
		QStringLiteral("KB"),
		QStringLiteral("MB"),
		QStringLiteral("GB"),
		QStringLiteral("TB"),
		QStringLiteral("PB"),
		QStringLiteral("EB")
	};
	QStringListIterator i(list);
	QString unit = QStringLiteral("b");
	double num = size;
	while (num >= 1024.0 && i.hasNext())
	{
		unit = i.next();
		num /= 1024.0;
	}
	return QStringLiteral("%1 %2").arg(QString::number(num, 'f', 1)).arg(unit);
}
