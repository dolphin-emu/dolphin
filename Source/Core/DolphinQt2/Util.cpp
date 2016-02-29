// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QString>
#include <QStringList>

#include "DolphinQt2/Util.h"

// Convert an integer size to a friendly string representation.
QString FormatSize(qint64 size)
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
