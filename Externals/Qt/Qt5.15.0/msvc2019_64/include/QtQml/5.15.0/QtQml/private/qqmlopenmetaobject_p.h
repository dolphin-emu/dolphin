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

#ifndef QQMLOPENMETAOBJECT_H
#define QQMLOPENMETAOBJECT_H

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

#include <QtCore/QMetaObject>
#include <QtCore/QObject>

#include <private/qqmlrefcount_p.h>
#include <private/qqmlcleanup_p.h>
#include <private/qtqmlglobal_p.h>
#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE


class QQmlEngine;
class QMetaPropertyBuilder;
class QQmlOpenMetaObjectTypePrivate;
class Q_QML_PRIVATE_EXPORT QQmlOpenMetaObjectType : public QQmlRefCount, public QQmlCleanup
{
public:
    QQmlOpenMetaObjectType(const QMetaObject *base, QQmlEngine *engine);
    ~QQmlOpenMetaObjectType() override;

    void createProperties(const QVector<QByteArray> &names);
    int createProperty(const QByteArray &name);

    int propertyOffset() const;
    int signalOffset() const;

    int propertyCount() const;
    QByteArray propertyName(int) const;
    QMetaObject *metaObject() const;

protected:
    virtual void propertyCreated(int, QMetaPropertyBuilder &);
    void clear() override;

private:
    QQmlOpenMetaObjectTypePrivate *d;
    friend class QQmlOpenMetaObject;
    friend class QQmlOpenMetaObjectPrivate;
};

class QQmlOpenMetaObjectPrivate;
class Q_QML_PRIVATE_EXPORT QQmlOpenMetaObject : public QAbstractDynamicMetaObject
{
public:
    QQmlOpenMetaObject(QObject *, const QMetaObject * = nullptr, bool = true);
    QQmlOpenMetaObject(QObject *, QQmlOpenMetaObjectType *, bool = true);
    ~QQmlOpenMetaObject() override;

    QVariant value(const QByteArray &) const;
    bool setValue(const QByteArray &, const QVariant &, bool force = false);
    QVariant value(int) const;
    void setValue(int, const QVariant &);
    QVariant &valueRef(const QByteArray &);
    bool hasValue(int) const;

    int count() const;
    QByteArray name(int) const;

    QObject *object() const;
    virtual QVariant initialValue(int);

    // Be careful - once setCached(true) is called createProperty() is no
    // longer automatically called for new properties.
    void setCached(bool);

    QQmlOpenMetaObjectType *type() const;

    void emitPropertyNotification(const QByteArray &propertyName);

protected:
    int metaCall(QObject *o, QMetaObject::Call _c, int _id, void **_a) override;
    int createProperty(const char *, const char *) override;

    virtual void propertyRead(int);
    virtual void propertyWrite(int);
    virtual QVariant propertyWriteValue(int, const QVariant &);
    virtual void propertyWritten(int);
    virtual void propertyCreated(int, QMetaPropertyBuilder &);

    QAbstractDynamicMetaObject *parent() const;

private:
    QQmlOpenMetaObjectPrivate *d;
    friend class QQmlOpenMetaObjectType;
};

QT_END_NAMESPACE

#endif // QQMLOPENMETAOBJECT_H
