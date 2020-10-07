/****************************************************************************
**
** Copyright (C) 2013 John Layt <jlayt@kde.org>
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


#ifndef QTIMEZONEPRIVATE_P_H
#define QTIMEZONEPRIVATE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of internal files.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include "qtimezone.h"
#include "private/qlocale_p.h"
#include "qvector.h"

#if QT_CONFIG(icu)
#include <unicode/ucal.h>
#endif

#ifdef Q_OS_DARWIN
Q_FORWARD_DECLARE_OBJC_CLASS(NSTimeZone);
#endif // Q_OS_DARWIN

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif // Q_OS_WIN

#if defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_EMBEDDED)
#include <QtCore/private/qjni_p.h>
#endif

QT_BEGIN_NAMESPACE

class Q_AUTOTEST_EXPORT QTimeZonePrivate : public QSharedData
{
public:
    //Version of QTimeZone::OffsetData struct using msecs for efficiency
    struct Data {
        QString abbreviation;
        qint64 atMSecsSinceEpoch;
        int offsetFromUtc;
        int standardTimeOffset;
        int daylightTimeOffset;
    };
    typedef QVector<Data> DataList;

    // Create null time zone
    QTimeZonePrivate();
    QTimeZonePrivate(const QTimeZonePrivate &other);
    virtual ~QTimeZonePrivate();

    virtual QTimeZonePrivate *clone() const;

    bool operator==(const QTimeZonePrivate &other) const;
    bool operator!=(const QTimeZonePrivate &other) const;

    bool isValid() const;

    QByteArray id() const;
    virtual QLocale::Country country() const;
    virtual QString comment() const;

    virtual QString displayName(qint64 atMSecsSinceEpoch,
                                QTimeZone::NameType nameType,
                                const QLocale &locale) const;
    virtual QString displayName(QTimeZone::TimeType timeType,
                                QTimeZone::NameType nameType,
                                const QLocale &locale) const;
    virtual QString abbreviation(qint64 atMSecsSinceEpoch) const;

    virtual int offsetFromUtc(qint64 atMSecsSinceEpoch) const;
    virtual int standardTimeOffset(qint64 atMSecsSinceEpoch) const;
    virtual int daylightTimeOffset(qint64 atMSecsSinceEpoch) const;

    virtual bool hasDaylightTime() const;
    virtual bool isDaylightTime(qint64 atMSecsSinceEpoch) const;

    virtual Data data(qint64 forMSecsSinceEpoch) const;
    Data dataForLocalTime(qint64 forLocalMSecs, int hint) const;

    virtual bool hasTransitions() const;
    virtual Data nextTransition(qint64 afterMSecsSinceEpoch) const;
    virtual Data previousTransition(qint64 beforeMSecsSinceEpoch) const;
    DataList transitions(qint64 fromMSecsSinceEpoch, qint64 toMSecsSinceEpoch) const;

    virtual QByteArray systemTimeZoneId() const;

    virtual bool isTimeZoneIdAvailable(const QByteArray &ianaId) const;
    virtual QList<QByteArray> availableTimeZoneIds() const;
    virtual QList<QByteArray> availableTimeZoneIds(QLocale::Country country) const;
    virtual QList<QByteArray> availableTimeZoneIds(int utcOffset) const;

    virtual void serialize(QDataStream &ds) const;

    // Static Utility Methods
    static inline qint64 maxMSecs() { return std::numeric_limits<qint64>::max(); }
    static inline qint64 minMSecs() { return std::numeric_limits<qint64>::min() + 1; }
    static inline qint64 invalidMSecs() { return std::numeric_limits<qint64>::min(); }
    static inline qint64 invalidSeconds() { return std::numeric_limits<int>::min(); }
    static Data invalidData();
    static QTimeZone::OffsetData invalidOffsetData();
    static QTimeZone::OffsetData toOffsetData(const Data &data);
    static bool isValidId(const QByteArray &ianaId);
    static QString isoOffsetFormat(int offsetFromUtc);

    static QByteArray ianaIdToWindowsId(const QByteArray &ianaId);
    static QByteArray windowsIdToDefaultIanaId(const QByteArray &windowsId);
    static QByteArray windowsIdToDefaultIanaId(const QByteArray &windowsId,
                                                QLocale::Country country);
    static QList<QByteArray> windowsIdToIanaIds(const QByteArray &windowsId);
    static QList<QByteArray> windowsIdToIanaIds(const QByteArray &windowsId,
                                                 QLocale::Country country);

    // returns "UTC" QString and QByteArray
    Q_REQUIRED_RESULT static inline QString utcQString()
    {
        return QStringLiteral("UTC");
    }

    Q_REQUIRED_RESULT static inline QByteArray utcQByteArray()
    {
        return QByteArrayLiteral("UTC");
    }

protected:
    QByteArray m_id;
};
Q_DECLARE_TYPEINFO(QTimeZonePrivate::Data, Q_MOVABLE_TYPE);

template<> QTimeZonePrivate *QSharedDataPointer<QTimeZonePrivate>::clone();

class Q_AUTOTEST_EXPORT QUtcTimeZonePrivate final : public QTimeZonePrivate
{
public:
    // Create default UTC time zone
    QUtcTimeZonePrivate();
    // Create named time zone
    QUtcTimeZonePrivate(const QByteArray &utcId);
    // Create offset from UTC
    QUtcTimeZonePrivate(int offsetSeconds);
    // Create custom offset from UTC
    QUtcTimeZonePrivate(const QByteArray &zoneId, int offsetSeconds, const QString &name,
                        const QString &abbreviation, QLocale::Country country,
                        const QString &comment);
    QUtcTimeZonePrivate(const QUtcTimeZonePrivate &other);
    virtual ~QUtcTimeZonePrivate();

    // Fall-back for UTC[+-]\d+(:\d+){,2} IDs.
    static qint64 offsetFromUtcString(const QByteArray &id);

    QUtcTimeZonePrivate *clone() const override;

    Data data(qint64 forMSecsSinceEpoch) const override;

    QLocale::Country country() const override;
    QString comment() const override;

    QString displayName(QTimeZone::TimeType timeType,
                        QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString abbreviation(qint64 atMSecsSinceEpoch) const override;

    int standardTimeOffset(qint64 atMSecsSinceEpoch) const override;
    int daylightTimeOffset(qint64 atMSecsSinceEpoch) const override;

    QByteArray systemTimeZoneId() const override;

    bool isTimeZoneIdAvailable(const QByteArray &ianaId) const override;
    QList<QByteArray> availableTimeZoneIds() const override;
    QList<QByteArray> availableTimeZoneIds(QLocale::Country country) const override;
    QList<QByteArray> availableTimeZoneIds(int utcOffset) const override;

    void serialize(QDataStream &ds) const override;

private:
    void init(const QByteArray &zoneId);
    void init(const QByteArray &zoneId, int offsetSeconds, const QString &name,
              const QString &abbreviation, QLocale::Country country,
              const QString &comment);

    QString m_name;
    QString m_abbreviation;
    QString m_comment;
    QLocale::Country m_country;
    int m_offsetFromUtc;
};

#if QT_CONFIG(icu)
class Q_AUTOTEST_EXPORT QIcuTimeZonePrivate final : public QTimeZonePrivate
{
public:
    // Create default time zone
    QIcuTimeZonePrivate();
    // Create named time zone
    QIcuTimeZonePrivate(const QByteArray &ianaId);
    QIcuTimeZonePrivate(const QIcuTimeZonePrivate &other);
    ~QIcuTimeZonePrivate();

    QIcuTimeZonePrivate *clone() const override;

    QString displayName(QTimeZone::TimeType timeType, QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString abbreviation(qint64 atMSecsSinceEpoch) const override;

    int offsetFromUtc(qint64 atMSecsSinceEpoch) const override;
    int standardTimeOffset(qint64 atMSecsSinceEpoch) const override;
    int daylightTimeOffset(qint64 atMSecsSinceEpoch) const override;

    bool hasDaylightTime() const override;
    bool isDaylightTime(qint64 atMSecsSinceEpoch) const override;

    Data data(qint64 forMSecsSinceEpoch) const override;

    bool hasTransitions() const override;
    Data nextTransition(qint64 afterMSecsSinceEpoch) const override;
    Data previousTransition(qint64 beforeMSecsSinceEpoch) const override;

    QByteArray systemTimeZoneId() const override;

    QList<QByteArray> availableTimeZoneIds() const override;
    QList<QByteArray> availableTimeZoneIds(QLocale::Country country) const override;
    QList<QByteArray> availableTimeZoneIds(int offsetFromUtc) const override;

private:
    void init(const QByteArray &ianaId);

    UCalendar *m_ucal;
};
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_EMBEDDED))
struct QTzTransitionTime
{
    qint64 atMSecsSinceEpoch;
    quint8 ruleIndex;
};
Q_DECLARE_TYPEINFO(QTzTransitionTime, Q_PRIMITIVE_TYPE);
struct QTzTransitionRule
{
    int stdOffset;
    int dstOffset;
    quint8 abbreviationIndex;
};
Q_DECLARE_TYPEINFO(QTzTransitionRule, Q_PRIMITIVE_TYPE);
Q_DECL_CONSTEXPR inline bool operator==(const QTzTransitionRule &lhs, const QTzTransitionRule &rhs) noexcept
{ return lhs.stdOffset == rhs.stdOffset && lhs.dstOffset == rhs.dstOffset && lhs.abbreviationIndex == rhs.abbreviationIndex; }
Q_DECL_CONSTEXPR inline bool operator!=(const QTzTransitionRule &lhs, const QTzTransitionRule &rhs) noexcept
{ return !operator==(lhs, rhs); }

// These are stored separately from QTzTimeZonePrivate so that they can be
// cached, avoiding the need to re-parse them from disk constantly.
struct QTzTimeZoneCacheEntry
{
    QVector<QTzTransitionTime> m_tranTimes;
    QVector<QTzTransitionRule> m_tranRules;
    QList<QByteArray> m_abbreviations;
    QByteArray m_posixRule;
};

class Q_AUTOTEST_EXPORT QTzTimeZonePrivate final : public QTimeZonePrivate
{
    QTzTimeZonePrivate(const QTzTimeZonePrivate &) = default;
public:
    // Create default time zone
    QTzTimeZonePrivate();
    // Create named time zone
    QTzTimeZonePrivate(const QByteArray &ianaId);
    ~QTzTimeZonePrivate();

    QTzTimeZonePrivate *clone() const override;

    QLocale::Country country() const override;
    QString comment() const override;

    QString displayName(qint64 atMSecsSinceEpoch,
                        QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString displayName(QTimeZone::TimeType timeType,
                        QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString abbreviation(qint64 atMSecsSinceEpoch) const override;

    int offsetFromUtc(qint64 atMSecsSinceEpoch) const override;
    int standardTimeOffset(qint64 atMSecsSinceEpoch) const override;
    int daylightTimeOffset(qint64 atMSecsSinceEpoch) const override;

    bool hasDaylightTime() const override;
    bool isDaylightTime(qint64 atMSecsSinceEpoch) const override;

    Data data(qint64 forMSecsSinceEpoch) const override;

    bool hasTransitions() const override;
    Data nextTransition(qint64 afterMSecsSinceEpoch) const override;
    Data previousTransition(qint64 beforeMSecsSinceEpoch) const override;

    QByteArray systemTimeZoneId() const override;

    bool isTimeZoneIdAvailable(const QByteArray &ianaId) const override;
    QList<QByteArray> availableTimeZoneIds() const override;
    QList<QByteArray> availableTimeZoneIds(QLocale::Country country) const override;

private:
    void init(const QByteArray &ianaId);
    QVector<QTimeZonePrivate::Data> getPosixTransitions(qint64 msNear) const;

    Data dataForTzTransition(QTzTransitionTime tran) const;
#if QT_CONFIG(icu)
    mutable QSharedDataPointer<QTimeZonePrivate> m_icu;
#endif
    QTzTimeZoneCacheEntry cached_data;
    QVector<QTzTransitionTime> tranCache() const { return cached_data.m_tranTimes; }
};
#endif // Q_OS_UNIX

#ifdef Q_OS_MAC
class Q_AUTOTEST_EXPORT QMacTimeZonePrivate final : public QTimeZonePrivate
{
public:
    // Create default time zone
    QMacTimeZonePrivate();
    // Create named time zone
    QMacTimeZonePrivate(const QByteArray &ianaId);
    QMacTimeZonePrivate(const QMacTimeZonePrivate &other);
    ~QMacTimeZonePrivate();

    QMacTimeZonePrivate *clone() const override;

    QString comment() const override;

    QString displayName(QTimeZone::TimeType timeType, QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString abbreviation(qint64 atMSecsSinceEpoch) const override;

    int offsetFromUtc(qint64 atMSecsSinceEpoch) const override;
    int standardTimeOffset(qint64 atMSecsSinceEpoch) const override;
    int daylightTimeOffset(qint64 atMSecsSinceEpoch) const override;

    bool hasDaylightTime() const override;
    bool isDaylightTime(qint64 atMSecsSinceEpoch) const override;

    Data data(qint64 forMSecsSinceEpoch) const override;

    bool hasTransitions() const override;
    Data nextTransition(qint64 afterMSecsSinceEpoch) const override;
    Data previousTransition(qint64 beforeMSecsSinceEpoch) const override;

    QByteArray systemTimeZoneId() const override;

    QList<QByteArray> availableTimeZoneIds() const override;

    NSTimeZone *nsTimeZone() const;

private:
    void init(const QByteArray &zoneId);

    NSTimeZone *m_nstz;
};
#endif // Q_OS_MAC

#ifdef Q_OS_WIN
class Q_AUTOTEST_EXPORT QWinTimeZonePrivate final : public QTimeZonePrivate
{
public:
    struct QWinTransitionRule {
        int startYear;
        int standardTimeBias;
        int daylightTimeBias;
        SYSTEMTIME standardTimeRule;
        SYSTEMTIME daylightTimeRule;
    };

    // Create default time zone
    QWinTimeZonePrivate();
    // Create named time zone
    QWinTimeZonePrivate(const QByteArray &ianaId);
    QWinTimeZonePrivate(const QWinTimeZonePrivate &other);
    ~QWinTimeZonePrivate();

    QWinTimeZonePrivate *clone() const override;

    QString comment() const override;

    QString displayName(QTimeZone::TimeType timeType, QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString abbreviation(qint64 atMSecsSinceEpoch) const override;

    int offsetFromUtc(qint64 atMSecsSinceEpoch) const override;
    int standardTimeOffset(qint64 atMSecsSinceEpoch) const override;
    int daylightTimeOffset(qint64 atMSecsSinceEpoch) const override;

    bool hasDaylightTime() const override;
    bool isDaylightTime(qint64 atMSecsSinceEpoch) const override;

    Data data(qint64 forMSecsSinceEpoch) const override;

    bool hasTransitions() const override;
    Data nextTransition(qint64 afterMSecsSinceEpoch) const override;
    Data previousTransition(qint64 beforeMSecsSinceEpoch) const override;

    QByteArray systemTimeZoneId() const override;

    QList<QByteArray> availableTimeZoneIds() const override;

private:
    void init(const QByteArray &ianaId);
    QTimeZonePrivate::Data ruleToData(const QWinTransitionRule &rule, qint64 atMSecsSinceEpoch,
                                      QTimeZone::TimeType type, bool fakeDst = false) const;

    QByteArray m_windowsId;
    QString m_displayName;
    QString m_standardName;
    QString m_daylightName;
    QList<QWinTransitionRule> m_tranRules;
};
#endif // Q_OS_WIN

#if defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_EMBEDDED)
class QAndroidTimeZonePrivate final : public QTimeZonePrivate
{
public:
    // Create default time zone
    QAndroidTimeZonePrivate();
    // Create named time zone
    QAndroidTimeZonePrivate(const QByteArray &ianaId);
    QAndroidTimeZonePrivate(const QAndroidTimeZonePrivate &other);
    ~QAndroidTimeZonePrivate();

    QAndroidTimeZonePrivate *clone() const override;

    QString displayName(QTimeZone::TimeType timeType, QTimeZone::NameType nameType,
                        const QLocale &locale) const override;
    QString abbreviation(qint64 atMSecsSinceEpoch) const override;

    int offsetFromUtc(qint64 atMSecsSinceEpoch) const override;
    int standardTimeOffset(qint64 atMSecsSinceEpoch) const override;
    int daylightTimeOffset(qint64 atMSecsSinceEpoch) const override;

    bool hasDaylightTime() const override;
    bool isDaylightTime(qint64 atMSecsSinceEpoch) const override;

    Data data(qint64 forMSecsSinceEpoch) const override;

    bool hasTransitions() const override;
    Data nextTransition(qint64 afterMSecsSinceEpoch) const override;
    Data previousTransition(qint64 beforeMSecsSinceEpoch) const override;

    QByteArray systemTimeZoneId() const override;

    QList<QByteArray> availableTimeZoneIds() const override;

private:
    void init(const QByteArray &zoneId);

    QJNIObjectPrivate androidTimeZone;

};
#endif // Q_OS_ANDROID

QT_END_NAMESPACE

#endif // QTIMEZONEPRIVATE_P_H
