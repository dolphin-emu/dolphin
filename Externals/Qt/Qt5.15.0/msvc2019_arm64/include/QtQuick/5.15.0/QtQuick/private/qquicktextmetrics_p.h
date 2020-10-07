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

#ifndef QQUICKTEXTMETRICS_H
#define QQUICKTEXTMETRICS_H

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

class Q_AUTOTEST_EXPORT QQuickTextMetrics : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged FINAL)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged FINAL)
    Q_PROPERTY(qreal advanceWidth READ advanceWidth NOTIFY metricsChanged FINAL)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY metricsChanged FINAL)
    Q_PROPERTY(qreal width READ width NOTIFY metricsChanged FINAL)
    Q_PROPERTY(qreal height READ height NOTIFY metricsChanged FINAL)
    Q_PROPERTY(QRectF tightBoundingRect READ tightBoundingRect NOTIFY metricsChanged FINAL)
    Q_PROPERTY(QString elidedText READ elidedText NOTIFY metricsChanged FINAL)
    Q_PROPERTY(Qt::TextElideMode elide READ elide WRITE setElide NOTIFY elideChanged FINAL)
    Q_PROPERTY(qreal elideWidth READ elideWidth WRITE setElideWidth NOTIFY elideWidthChanged FINAL)
    QML_NAMED_ELEMENT(TextMetrics)
    QML_ADDED_IN_MINOR_VERSION(4)

public:
    explicit QQuickTextMetrics(QObject *parent = 0);
    ~QQuickTextMetrics();

    QFont font() const;
    void setFont(const QFont &font);

    QString text() const;
    void setText(const QString &text);

    Qt::TextElideMode elide() const;
    void setElide(Qt::TextElideMode elide);

    qreal elideWidth() const;
    void setElideWidth(qreal elideWidth);

    qreal advanceWidth() const;
    QRectF boundingRect() const;
    qreal width() const;
    qreal height() const;
    QRectF tightBoundingRect() const;
    QString elidedText() const;

Q_SIGNALS:
    void fontChanged();
    void textChanged();
    void elideChanged();
    void elideWidthChanged();
    void metricsChanged();

private:
    QString m_text;
    QFont m_font;
    QFontMetricsF m_metrics;
    Qt::TextElideMode m_elide;
    qreal m_elideWidth;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTextMetrics)

#endif // QQUICKTEXTMETRICS_H
