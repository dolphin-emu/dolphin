/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QQMLGUARD_P_H
#define QQMLGUARD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of qapplication_*.cpp, qwidget*.cpp and qfiledialog.cpp.  This header
// file may change from version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <QtCore/qvariant.h>
#include <private/qqmldata_p.h>

QT_BEGIN_NAMESPACE

class QQmlGuardImpl
{
public:
    inline QQmlGuardImpl();
    inline QQmlGuardImpl(QObject *);
    inline QQmlGuardImpl(const QQmlGuardImpl &);
    inline ~QQmlGuardImpl();

    QObject *o = nullptr;
    QQmlGuardImpl  *next = nullptr;
    QQmlGuardImpl **prev = nullptr;

    inline void addGuard();
    inline void remGuard();
};

class QObject;
template<class T>
class QQmlGuard : private QQmlGuardImpl
{
    friend class QQmlData;
public:
    inline QQmlGuard();
    inline QQmlGuard(T *);
    inline QQmlGuard(const QQmlGuard<T> &);
    inline virtual ~QQmlGuard();

    inline QQmlGuard<T> &operator=(const QQmlGuard<T> &o);
    inline QQmlGuard<T> &operator=(T *);

    inline T *object() const;
    inline void setObject(T *g);

    inline bool isNull() const
        { return !o; }

    inline T* operator->() const
        { return static_cast<T*>(const_cast<QObject*>(o)); }
    inline T& operator*() const
        { return *static_cast<T*>(const_cast<QObject*>(o)); }
    inline operator T*() const
        { return static_cast<T*>(const_cast<QObject*>(o)); }
    inline T* data() const
        { return static_cast<T*>(const_cast<QObject*>(o)); }

protected:
    virtual void objectDestroyed(T *) {}
};

template <typename T>
class QQmlStrongJSQObjectReference : public QQmlGuard<T>
{
public:
    void setObject(T *o, QObject *parent) {
        T *old = this->object();
        if (o == old)
            return;

        if (m_jsOwnership && old && old->parent() == parent)
            QQml_setParent_noEvent(old, nullptr);

        this->QQmlGuard<T>::operator=(o);

        if (o && !o->parent() && !QQmlData::keepAliveDuringGarbageCollection(o)) {
            m_jsOwnership = true;
            QQml_setParent_noEvent(o, parent);
        } else {
            m_jsOwnership = false;
        }
    }

private:
    using QQmlGuard<T>::setObject;
    using QQmlGuard<T>::operator=;
    bool m_jsOwnership = false;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlGuard<QObject>)

QT_BEGIN_NAMESPACE

QQmlGuardImpl::QQmlGuardImpl()
{
}

QQmlGuardImpl::QQmlGuardImpl(QObject *g)
: o(g)
{
    if (o) addGuard();
}

QQmlGuardImpl::QQmlGuardImpl(const QQmlGuardImpl &g)
: o(g.o)
{
    if (o) addGuard();
}

QQmlGuardImpl::~QQmlGuardImpl()
{
    if (prev) remGuard();
    o = nullptr;
}

void QQmlGuardImpl::addGuard()
{
    Q_ASSERT(!prev);

    if (QObjectPrivate::get(o)->wasDeleted)
        return;

    QQmlData *data = QQmlData::get(o, true);
    next = data->guards;
    if (next) next->prev = &next;
    data->guards = this;
    prev = &data->guards;
}

void QQmlGuardImpl::remGuard()
{
    Q_ASSERT(prev);

    if (next) next->prev = prev;
    *prev = next;
    next = nullptr;
    prev = nullptr;
}

template<class T>
QQmlGuard<T>::QQmlGuard()
{
}

template<class T>
QQmlGuard<T>::QQmlGuard(T *g)
: QQmlGuardImpl(g)
{
}

template<class T>
QQmlGuard<T>::QQmlGuard(const QQmlGuard<T> &g)
: QQmlGuardImpl(g)
{
}

template<class T>
QQmlGuard<T>::~QQmlGuard()
{
}

template<class T>
QQmlGuard<T> &QQmlGuard<T>::operator=(const QQmlGuard<T> &g)
{
    setObject(g.object());
    return *this;
}

template<class T>
QQmlGuard<T> &QQmlGuard<T>::operator=(T *g)
{
    setObject(g);
    return *this;
}

template<class T>
T *QQmlGuard<T>::object() const
{
    return static_cast<T *>(o);
}

template<class T>
void QQmlGuard<T>::setObject(T *g)
{
    if (g != o) {
        if (prev) remGuard();
        o = g;
        if (o) addGuard();
    }
}

QT_END_NAMESPACE

#endif // QQMLGUARD_P_H
