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

#ifndef QQMLABSTRACTBINDING_P_H
#define QQMLABSTRACTBINDING_P_H

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

#include <QtCore/qsharedpointer.h>
#include <QtCore/qshareddata.h>
#include <private/qtqmlglobal_p.h>
#include <private/qqmlproperty_p.h>

QT_BEGIN_NAMESPACE

class QQmlObjectCreator;

class Q_QML_PRIVATE_EXPORT QQmlAbstractBinding
{
protected:
    QQmlAbstractBinding();
public:
    virtual ~QQmlAbstractBinding();

    typedef QExplicitlySharedDataPointer<QQmlAbstractBinding> Ptr;

    virtual QString expression() const;

    virtual bool isValueTypeProxy() const;

    // Should return the encoded property index for the binding.  Should return this value
    // even if the binding is not enabled or added to an object.
    // Encoding is:  coreIndex | (valueTypeIndex << 16)
    QQmlPropertyIndex targetPropertyIndex() const { return m_targetIndex; }

    // Should return the object for the binding.  Should return this object even if the
    // binding is not enabled or added to the object.
    QObject *targetObject() const { return m_target.data(); }

    virtual void setEnabled(bool e, QQmlPropertyData::WriteFlags f = QQmlPropertyData::DontRemoveBinding) = 0;

    void addToObject();
    void removeFromObject();

    static void printBindingLoopError(QQmlProperty &prop);

    inline QQmlAbstractBinding *nextBinding() const;

    inline bool canUseAccessor() const
    { return m_nextBinding.flag2(); }

    struct RefCount {
        RefCount() {}
        int refCount = 0;
        void ref() { ++refCount; }
        int deref() { return --refCount; }
        operator int() const { return refCount; }
    };
    RefCount ref;

protected:
    friend class QQmlData;
    friend class QQmlValueTypeProxyBinding;
    friend class QQmlObjectCreator;

    inline void setAddedToObject(bool v);
    inline bool isAddedToObject() const;

    inline void setNextBinding(QQmlAbstractBinding *);

    QQmlPropertyIndex m_targetIndex;

    // Pointer is the target object to which the binding binds
    // flag1 is the updating flag
    // flag2 is the enabled flag
    QFlagPointer<QObject> m_target;

    // Pointer to the next binding in the linked list of bindings.
    // flag1 is used for addedToObject
    // flag2 indicates if an accessor is can be used (i.e. there is no interceptor on the target)
    QFlagPointer<QQmlAbstractBinding> m_nextBinding;
};

void QQmlAbstractBinding::setAddedToObject(bool v)
{
    m_nextBinding.setFlagValue(v);
}

bool QQmlAbstractBinding::isAddedToObject() const
{
    return m_nextBinding.flag();
}

QQmlAbstractBinding *QQmlAbstractBinding::nextBinding() const
{
    return m_nextBinding.data();
}

void QQmlAbstractBinding::setNextBinding(QQmlAbstractBinding *b)
{
    if (b)
        b->ref.ref();
    if (m_nextBinding.data() && !m_nextBinding->ref.deref())
        delete m_nextBinding.data();
    m_nextBinding = b;
}

QT_END_NAMESPACE

#endif // QQMLABSTRACTBINDING_P_H
