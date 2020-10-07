/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QDATETIMEEDIT_P_H
#define QDATETIMEEDIT_P_H

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

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include <QtCore/qcalendar.h>
#include "QtWidgets/qcalendarwidget.h"
#include "QtWidgets/qspinbox.h"
#include "QtWidgets/qtoolbutton.h"
#include "QtWidgets/qmenu.h"
#include "QtWidgets/qdatetimeedit.h"
#include "private/qabstractspinbox_p.h"
#include "private/qdatetimeparser_p.h"

#include "qdebug.h"

QT_BEGIN_NAMESPACE

class QCalendarPopup;
class Q_AUTOTEST_EXPORT QDateTimeEditPrivate : public QAbstractSpinBoxPrivate, public QDateTimeParser
{
    Q_DECLARE_PUBLIC(QDateTimeEdit)
public:
    QDateTimeEditPrivate();

    void init(const QVariant &var);
    void readLocaleSettings();

    QDateTime validateAndInterpret(QString &input, int &, QValidator::State &state,
                                   bool fixup = false) const;
    void clearSection(int index);

    // Override QAbstractSpinBoxPrivate:
    void emitSignals(EmitPolicy ep, const QVariant &old) override;
    QString textFromValue(const QVariant &f) const override;
    QVariant valueFromText(const QString &f) const override;
    void _q_editorCursorPositionChanged(int oldpos, int newpos) override;
    void interpret(EmitPolicy ep) override;
    void clearCache() const override;
    QStyle::SubControl newHoverControl(const QPoint &pos) override;
    void updateEditFieldGeometry() override;
    QVariant getZeroVariant() const override;
    void setRange(const QVariant &min, const QVariant &max) override;
    void updateEdit() override;

    // Override QDateTimeParser:
    QString displayText() const override { return edit->text(); }
    QDateTime getMinimum() const override
    {
        if (keyboardTracking)
            return minimum.toDateTime();
        if (spec != Qt::LocalTime)
            return QDateTime(QDATETIMEEDIT_DATE_MIN.startOfDay(spec));
        return QDateTimeParser::getMinimum();
    }
    QDateTime getMaximum() const override
    {
        if (keyboardTracking)
            return maximum.toDateTime();
        if (spec != Qt::LocalTime)
            return QDateTime(QDATETIMEEDIT_DATE_MIN.startOfDay(spec));
        return QDateTimeParser::getMaximum();
    }
    QLocale locale() const override { return q_func()->locale(); }
    QString getAmPmText(AmPm ap, Case cs) const override;
    int cursorPosition() const override { return edit ? edit->cursorPosition() : -1; }

    int absoluteIndex(QDateTimeEdit::Section s, int index) const;
    int absoluteIndex(const SectionNode &s) const;
    QDateTime stepBy(int index, int steps, bool test = false) const;
    int sectionAt(int pos) const;
    int closestSection(int index, bool forward) const;
    int nextPrevSection(int index, bool forward) const;
    void setSelected(int index, bool forward = false);

    void updateCache(const QVariant &val, const QString &str) const;

    void updateTimeSpec();
    QString valueToText(const QVariant &var) const { return textFromValue(var); }

    void _q_resetButton();
    void updateArrow(QStyle::StateFlag state);
    bool calendarPopupEnabled() const;
    void syncCalendarWidget();

    bool isSeparatorKey(const QKeyEvent *k) const;

    static QDateTimeEdit::Sections convertSections(QDateTimeParser::Sections s);
    static QDateTimeEdit::Section convertToPublic(QDateTimeParser::Section s);

    void initCalendarPopup(QCalendarWidget *cw = nullptr);
    void positionCalendarPopup();

    QDateTimeEdit::Sections sections;
    mutable bool cacheGuard;

    QString defaultDateFormat, defaultTimeFormat, defaultDateTimeFormat, unreversedFormat;
    mutable QVariant conflictGuard;
    bool hasHadFocus, formatExplicitlySet, calendarPopup;
    QStyle::StateFlag arrowState;
    QCalendarPopup *monthCalendar;

#ifdef QT_KEYPAD_NAVIGATION
    bool focusOnButton;
#endif

    Qt::TimeSpec spec = Qt::LocalTime;
};


class QCalendarPopup : public QWidget
{
    Q_OBJECT
public:
    explicit QCalendarPopup(QWidget *parent = nullptr, QCalendarWidget *cw = nullptr,
                            QCalendar ca = QCalendar());
    QDate selectedDate() { return verifyCalendarInstance()->selectedDate(); }
    void setDate(QDate date);
    void setDateRange(QDate min, QDate max);
    void setFirstDayOfWeek(Qt::DayOfWeek dow) { verifyCalendarInstance()->setFirstDayOfWeek(dow); }
    QCalendarWidget *calendarWidget() const { return const_cast<QCalendarPopup*>(this)->verifyCalendarInstance(); }
    void setCalendarWidget(QCalendarWidget *cw);
Q_SIGNALS:
    void activated(QDate date);
    void newDateSelected(QDate newDate);
    void hidingCalendar(QDate oldDate);
    void resetButton();

private Q_SLOTS:
    void dateSelected(QDate date);
    void dateSelectionChanged();

protected:
    void hideEvent(QHideEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    bool event(QEvent *e) override;

private:
    QCalendarWidget *verifyCalendarInstance();

    QPointer<QCalendarWidget> calendar;
    QDate oldDate;
    bool dateChanged;
    QCalendar calendarSystem;
};

QT_END_NAMESPACE

#endif // QDATETIMEEDIT_P_H
