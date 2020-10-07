/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKFONTMETRICS_H
#define QQUICKFONTMETRICS_H

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

#include <qqml.h>

#include <QtGui/QFontMetricsF>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QFont;

class Q_AUTOTEST_EXPORT QQuickFontMetrics : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(qreal ascent READ ascent NOTIFY fontChanged)
    Q_PROPERTY(qreal descent READ descent NOTIFY fontChanged)
    Q_PROPERTY(qreal height READ height NOTIFY fontChanged)
    Q_PROPERTY(qreal leading READ leading NOTIFY fontChanged)
    Q_PROPERTY(qreal lineSpacing READ lineSpacing NOTIFY fontChanged)
    Q_PROPERTY(qreal minimumLeftBearing READ minimumLeftBearing NOTIFY fontChanged)
    Q_PROPERTY(qreal minimumRightBearing READ minimumRightBearing NOTIFY fontChanged)
    Q_PROPERTY(qreal maximumCharacterWidth READ maximumCharacterWidth NOTIFY fontChanged)
    Q_PROPERTY(qreal xHeight READ xHeight NOTIFY fontChanged)
    Q_PROPERTY(qreal averageCharacterWidth READ averageCharacterWidth NOTIFY fontChanged)
    Q_PROPERTY(qreal underlinePosition READ underlinePosition NOTIFY fontChanged)
    Q_PROPERTY(qreal overlinePosition READ overlinePosition NOTIFY fontChanged)
    Q_PROPERTY(qreal strikeOutPosition READ strikeOutPosition NOTIFY fontChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth NOTIFY fontChanged)
    QML_NAMED_ELEMENT(FontMetrics)
    QML_ADDED_IN_MINOR_VERSION(4)
public:
    explicit QQuickFontMetrics(QObject *parent = nullptr);
    ~QQuickFontMetrics();

    QFont font() const;
    void setFont(const QFont &font);

    qreal ascent() const;
    qreal descent() const;
    qreal height() const;
    qreal leading() const;
    qreal lineSpacing() const;
    qreal minimumLeftBearing() const;
    qreal minimumRightBearing() const;
    qreal maximumCharacterWidth() const;

    qreal xHeight() const;
    qreal averageCharacterWidth() const;

    qreal underlinePosition() const;
    qreal overlinePosition() const;
    qreal strikeOutPosition() const;
    qreal lineWidth() const;

    Q_INVOKABLE qreal advanceWidth(const QString &text) const;
    Q_INVOKABLE QRectF boundingRect(const QString &text) const;
    Q_INVOKABLE QRectF tightBoundingRect(const QString &text) const;
    Q_INVOKABLE QString elidedText(const QString &text, Qt::TextElideMode mode, qreal width, int flags = 0) const;

Q_SIGNALS:
    void fontChanged(const QFont &font);

private:
    QFont m_font;
    QFontMetricsF m_metrics;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickFontMetrics)

#endif // QQUICKFONTMETRICS_H
