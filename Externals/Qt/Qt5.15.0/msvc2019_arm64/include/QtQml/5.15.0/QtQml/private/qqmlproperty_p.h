/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLPROPERTY_P_H
#define QQMLPROPERTY_P_H

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

#include "qqmlproperty.h"
#include "qqmlengine.h"

#include <private/qobject_p.h>
#include <private/qtqmlglobal_p.h>
#include <private/qqmlrefcount_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlboundsignalexpressionpointer_p.h>
#include <private/qqmlpropertydata_p.h>

QT_BEGIN_NAMESPACE

class QQmlContext;
class QQmlEnginePrivate;
class QQmlJavaScriptExpression;
class QQmlMetaObject;

class Q_QML_PRIVATE_EXPORT QQmlPropertyPrivate : public QQmlRefCount
{
public:
    QQmlContextData *context;
    QPointer<QQmlEngine> engine;
    QPointer<QObject> object;

    QQmlPropertyData core;
    QQmlPropertyData valueTypeData;

    bool isNameCached:1;
    QString nameCache;

    QQmlPropertyPrivate();

    QQmlPropertyIndex encodedIndex() const
    { return encodedIndex(core, valueTypeData); }
    static QQmlPropertyIndex encodedIndex(const QQmlPropertyData &core, const QQmlPropertyData &valueTypeData)
    { return QQmlPropertyIndex(core.coreIndex(), valueTypeData.coreIndex()); }

    inline QQmlContextData *effectiveContext() const;

    void initProperty(QObject *obj, const QString &name);
    void initDefault(QObject *obj);

    bool isValueType() const;
    int propertyType() const;
    QQmlProperty::Type type() const;
    QQmlProperty::PropertyTypeCategory propertyTypeCategory() const;

    QVariant readValueProperty();
    bool writeValueProperty(const QVariant &, QQmlPropertyData::WriteFlags);

    static QQmlMetaObject rawMetaObjectForType(QQmlEnginePrivate *, int);
    static bool writeEnumProperty(const QMetaProperty &prop, int idx, QObject *object,
                                  const QVariant &value, int flags);
    static bool writeValueProperty(QObject *,
                                   const QQmlPropertyData &, const QQmlPropertyData &valueTypeData,
                                   const QVariant &, QQmlContextData *,
                                   QQmlPropertyData::WriteFlags flags = {});
    static bool write(QObject *, const QQmlPropertyData &, const QVariant &,
                      QQmlContextData *, QQmlPropertyData::WriteFlags flags = {});
    static void findAliasTarget(QObject *, QQmlPropertyIndex, QObject **, QQmlPropertyIndex *);

    enum BindingFlag {
        None = 0,
        DontEnable = 0x1
    };
    Q_DECLARE_FLAGS(BindingFlags, BindingFlag)

    static void setBinding(QQmlAbstractBinding *binding, BindingFlags flags = None,
                           QQmlPropertyData::WriteFlags writeFlags = QQmlPropertyData::DontRemoveBinding);

    static void removeBinding(const QQmlProperty &that);
    static void removeBinding(QObject *o, QQmlPropertyIndex index);
    static void removeBinding(QQmlAbstractBinding *b);
    static QQmlAbstractBinding *binding(QObject *, QQmlPropertyIndex index);

    static QQmlProperty restore(QObject *, const QQmlPropertyData &, const QQmlPropertyData *, QQmlContextData *);

    int signalIndex() const;

    static inline QQmlPropertyPrivate *get(const QQmlProperty &p) { return p.d; }

    // "Public" (to QML) methods
    static QQmlAbstractBinding *binding(const QQmlProperty &that);
    static void setBinding(const QQmlProperty &that, QQmlAbstractBinding *);
    static QQmlBoundSignalExpression *signalExpression(const QQmlProperty &that);
    static void setSignalExpression(const QQmlProperty &that, QQmlBoundSignalExpression *);
    static void takeSignalExpression(const QQmlProperty &that, QQmlBoundSignalExpression *);
    static bool write(const QQmlProperty &that, const QVariant &, QQmlPropertyData::WriteFlags);
    static QQmlPropertyIndex propertyIndex(const QQmlProperty &that);
    static QMetaMethod findSignalByName(const QMetaObject *mo, const QByteArray &);
    static bool connect(const QObject *sender, int signal_index,
                        const QObject *receiver, int method_index,
                        int type = 0, int *types = nullptr);
    static void flushSignal(const QObject *sender, int signal_index);

    static QVariant resolvedUrlSequence(const QVariant &value, QQmlContextData *context);
    static QQmlProperty create(QObject *target, const QString &propertyName, QQmlContextData *context);

};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQmlPropertyPrivate::BindingFlags)

QT_END_NAMESPACE

#endif // QQMLPROPERTY_P_H
