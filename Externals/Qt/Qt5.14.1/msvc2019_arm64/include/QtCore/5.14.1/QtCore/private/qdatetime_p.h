/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QDATETIME_P_H
#define QDATETIME_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/private/qglobal_p.h>
#include "qplatformdefs.h"
#include "QtCore/qatomic.h"
#include "QtCore/qdatetime.h"
#include "QtCore/qshareddata.h"

#if QT_CONFIG(timezone)
#include "qtimezone.h"
#endif

QT_BEGIN_NAMESPACE

class QDateTimePrivate : public QSharedData
{
public:
    // forward the declarations from QDateTime (this makes them public)
    typedef QDateTime::ShortData QDateTimeShortData;
    typedef QDateTime::Data QDateTimeData;

    // Never change or delete this enum, it is required for backwards compatible
    // serialization of QDateTime before 5.2, so is essentially public API
    enum Spec {
        LocalUnknown = -1,
        LocalStandard = 0,
        LocalDST = 1,
        UTC = 2,
        OffsetFromUTC = 3,
        TimeZone = 4
    };

    // Daylight Time Status
    enum DaylightStatus {
        UnknownDaylightTime = -1,
        StandardTime = 0,
        DaylightTime = 1
    };

    // Status of date/time
    enum StatusFlag {
        ShortData           = 0x01,

        ValidDate           = 0x02,
        ValidTime           = 0x04,
        ValidDateTime       = 0x08,

        TimeSpecMask        = 0x30,

        SetToStandardTime   = 0x40,
        SetToDaylightTime   = 0x80
    };
    Q_DECLARE_FLAGS(StatusFlags, StatusFlag)

    enum {
        TimeSpecShift = 4,
        ValidityMask        = ValidDate | ValidTime | ValidDateTime,
        DaylightMask        = SetToStandardTime | SetToDaylightTime
    };

    static QDateTime::Data create(const QDate &toDate, const QTime &toTime, Qt::TimeSpec toSpec,
                                  int offsetSeconds);

#if QT_CONFIG(timezone)
    static QDateTime::Data create(const QDate &toDate, const QTime &toTime, const QTimeZone & timeZone);
#endif // timezone

    StatusFlags m_status = StatusFlag(Qt::LocalTime << TimeSpecShift);
    qint64 m_msecs = 0;
    int m_offsetFromUtc = 0;
#if QT_CONFIG(timezone)
    QTimeZone m_timeZone;
#endif // timezone

#if QT_CONFIG(timezone)
    static qint64 zoneMSecsToEpochMSecs(qint64 msecs, const QTimeZone &zone,
                                        DaylightStatus hint = UnknownDaylightTime,
                                        QDate *localDate = nullptr, QTime *localTime = nullptr);

    // Inlined for its one caller in qdatetime.cpp
    inline void setUtcOffsetByTZ(qint64 atMSecsSinceEpoch);
#endif // timezone
};

QT_END_NAMESPACE

#endif // QDATETIME_P_H
