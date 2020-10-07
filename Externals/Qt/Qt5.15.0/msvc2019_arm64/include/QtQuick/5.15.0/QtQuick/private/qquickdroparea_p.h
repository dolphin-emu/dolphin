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

#ifndef QQUICKDROPAREA_P_H
#define QQUICKDROPAREA_P_H

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

#include "qquickitem.h"

#include <QtGui/qevent.h>

QT_REQUIRE_CONFIG(quick_draganddrop);

QT_BEGIN_NAMESPACE

class QQuickDropAreaPrivate;
class QQuickDropEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(QObject *source READ source)
    Q_PROPERTY(QStringList keys READ keys)
    Q_PROPERTY(Qt::DropActions supportedActions READ supportedActions)
    Q_PROPERTY(Qt::DropActions proposedAction READ proposedAction)
    Q_PROPERTY(Qt::DropAction action READ action WRITE setAction RESET resetAction)
    Q_PROPERTY(bool accepted READ accepted WRITE setAccepted)
    Q_PROPERTY(bool hasColor READ hasColor)
    Q_PROPERTY(bool hasHtml READ hasHtml)
    Q_PROPERTY(bool hasText READ hasText)
    Q_PROPERTY(bool hasUrls READ hasUrls)
    Q_PROPERTY(QVariant colorData READ colorData)
    Q_PROPERTY(QString html READ html)
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(QList<QUrl> urls READ urls)
    Q_PROPERTY(QStringList formats READ formats)
    QML_ANONYMOUS
public:
    QQuickDropEvent(QQuickDropAreaPrivate *d, QDropEvent *event) : d(d), event(event) {}

    qreal x() const { return event->pos().x(); }
    qreal y() const { return event->pos().y(); }

    QObject *source() const;

    Qt::DropActions supportedActions() const { return event->possibleActions(); }
    Qt::DropActions proposedAction() const { return event->proposedAction(); }
    Qt::DropAction action() const { return event->dropAction(); }
    void setAction(Qt::DropAction action) { event->setDropAction(action); }
    void resetAction() { event->setDropAction(event->proposedAction()); }

    QStringList keys() const;

    bool accepted() const { return event->isAccepted(); }
    void setAccepted(bool accepted) { event->setAccepted(accepted); }

    bool hasColor() const;
    bool hasHtml() const;
    bool hasText() const;
    bool hasUrls() const;
    QVariant colorData() const;
    QString html() const;
    QString text() const;
    QList<QUrl> urls() const;
    QStringList formats() const;

    Q_INVOKABLE void getDataAsString(QQmlV4Function *);
    Q_INVOKABLE void getDataAsArrayBuffer(QQmlV4Function *);
    Q_INVOKABLE void acceptProposedAction(QQmlV4Function *);
    Q_INVOKABLE void accept(QQmlV4Function *);

private:
    QQuickDropAreaPrivate *d;
    QDropEvent *event;
};

class QQuickDropAreaDrag : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x NOTIFY positionChanged)
    Q_PROPERTY(qreal y READ y NOTIFY positionChanged)
    Q_PROPERTY(QObject *source READ source NOTIFY sourceChanged)
    QML_ANONYMOUS
public:
    QQuickDropAreaDrag(QQuickDropAreaPrivate *d, QObject *parent = 0);
    ~QQuickDropAreaDrag();

    qreal x() const;
    qreal y() const;
    QObject *source() const;

Q_SIGNALS:
    void positionChanged();
    void sourceChanged();

private:
    QQuickDropAreaPrivate *d;

    friend class QQuickDropArea;
    friend class QQuickDropAreaPrivate;
};

class QQuickDropAreaPrivate;
class Q_AUTOTEST_EXPORT QQuickDropArea : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool containsDrag READ containsDrag NOTIFY containsDragChanged)
    Q_PROPERTY(QStringList keys READ keys WRITE setKeys NOTIFY keysChanged)
    Q_PROPERTY(QQuickDropAreaDrag *drag READ drag CONSTANT)
    QML_NAMED_ELEMENT(DropArea)

public:
    QQuickDropArea(QQuickItem *parent=0);
    ~QQuickDropArea();

    bool containsDrag() const;
    void setContainsDrag(bool drag);

    QStringList keys() const;
    void setKeys(const QStringList &keys);

    QQuickDropAreaDrag *drag();

Q_SIGNALS:
    void containsDragChanged();
    void keysChanged();
    void sourceChanged();

    void entered(QQuickDropEvent *drag);
    void exited();
    void positionChanged(QQuickDropEvent *drag);
    void dropped(QQuickDropEvent *drop);

protected:
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    Q_DISABLE_COPY(QQuickDropArea)
    Q_DECLARE_PRIVATE(QQuickDropArea)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickDropEvent)
QML_DECLARE_TYPE(QQuickDropArea)

#endif // QQUICKDROPAREA_P_H
