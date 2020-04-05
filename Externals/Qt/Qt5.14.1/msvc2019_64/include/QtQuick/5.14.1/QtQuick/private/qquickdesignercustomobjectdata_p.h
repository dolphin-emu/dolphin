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

#ifndef DESIGNERCUSTOMOBJECTDATA_H
#define DESIGNERCUSTOMOBJECTDATA_H

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

#include "qquickdesignersupport_p.h"

#include <QHash>
#include <QObject>
#include <QVariant>

#include <private/qqmlbinding_p.h>

QT_BEGIN_NAMESPACE
class QQmlContext;

class QQuickDesignerCustomObjectData
{
public:
    static void registerData(QObject *object);
    static QQuickDesignerCustomObjectData *get(QObject *object);
    static QVariant getResetValue(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName);
    static void doResetProperty(QObject *object, QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName);
    static bool hasValidResetBinding(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName);
    static bool hasBindingForProperty(QObject *object,
                                      QQmlContext *context,
                                      const QQuickDesignerSupport::PropertyName &propertyName,
                                      bool *hasChanged);
    static void setPropertyBinding(QObject *object,
                                   QQmlContext *context,
                                   const QQuickDesignerSupport::PropertyName &propertyName,
                                   const QString &expression);
    static void keepBindingFromGettingDeleted(QObject *object,
                                              QQmlContext *context,
                                              const QQuickDesignerSupport::PropertyName &propertyName);
    void handleDestroyed();

private:
    QQuickDesignerCustomObjectData(QObject *object);
    void populateResetHashes();
    QObject *object() const;
    QVariant getResetValue(const QQuickDesignerSupport::PropertyName &propertyName) const;
    void doResetProperty(QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName);
    bool hasValidResetBinding(const QQuickDesignerSupport::PropertyName &propertyName) const;
    QQmlAbstractBinding *getResetBinding(const QQuickDesignerSupport::PropertyName &propertyName) const;
    bool hasBindingForProperty(QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName, bool *hasChanged) const;
    void setPropertyBinding(QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName, const QString &expression);
    void keepBindingFromGettingDeleted(QQmlContext *context, const QQuickDesignerSupport::PropertyName &propertyName);

    QObject *m_object;
    QHash<QQuickDesignerSupport::PropertyName, QVariant> m_resetValueHash;
    QHash<QQuickDesignerSupport::PropertyName, QQmlAbstractBinding::Ptr> m_resetBindingHash;
    mutable QHash<QQuickDesignerSupport::PropertyName, bool> m_hasBindingHash;
};

QT_END_NAMESPACE

#endif // DESIGNERCUSTOMOBJECTDATA_H
