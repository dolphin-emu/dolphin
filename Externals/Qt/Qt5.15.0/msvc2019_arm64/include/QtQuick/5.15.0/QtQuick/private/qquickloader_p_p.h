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

#ifndef QQUICKLOADER_P_P_H
#define QQUICKLOADER_P_P_H

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

#include "qquickloader_p.h"
#include "qquickimplicitsizeitem_p_p.h"
#include "qquickitemchangelistener_p.h"
#include <qqmlincubator.h>

#include <private/qv4value_p.h>

QT_BEGIN_NAMESPACE


class QQuickLoaderPrivate;
class QQuickLoaderIncubator : public QQmlIncubator
{
public:
    QQuickLoaderIncubator(QQuickLoaderPrivate *l, IncubationMode mode) : QQmlIncubator(mode), loader(l) {}

protected:
    void statusChanged(Status) override;
    void setInitialState(QObject *) override;

private:
    QQuickLoaderPrivate *loader;
};

class QQmlContext;
class QQuickLoaderPrivate : public QQuickImplicitSizeItemPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickLoader)

public:
    QQuickLoaderPrivate();
    ~QQuickLoaderPrivate();

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &oldGeometry) override;
    void itemImplicitWidthChanged(QQuickItem *) override;
    void itemImplicitHeightChanged(QQuickItem *) override;
    void clear();
    void initResize();
    void load();

    void incubatorStateChanged(QQmlIncubator::Status status);
    void setInitialState(QObject *o);
    void disposeInitialPropertyValues();
    static QUrl resolveSourceUrl(QQmlV4Function *args);
    QV4::ReturnedValue extractInitialPropertyValues(QQmlV4Function *args, QObject *loader, bool *error);
    QQuickLoader::Status computeStatus() const;
    void updateStatus();

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    QUrl source;
    QQuickItem *item;
    QObject *object;
    QQmlStrongJSQObjectReference<QQmlComponent> component;
    QQmlContext *itemContext;
    QQuickLoaderIncubator *incubator;
    QV4::PersistentValue initialPropertyValues;
    QV4::PersistentValue qmlCallingContext;
    bool updatingSize: 1;
    bool active : 1;
    bool loadingFromSource : 1;
    bool asynchronous : 1;
    // We need to use char instead of QQuickLoader::Status
    // as otherwise the size of the class would increase
    // on 32-bit systems, as sizeof(Status) == sizeof(int)
    // and sizeof(int) > remaining padding on 32 bit
    char status;

    void _q_sourceLoaded();
    void _q_updateSize(bool loaderGeometryChanged = true);
};

QT_END_NAMESPACE

#endif // QQUICKLOADER_P_P_H
