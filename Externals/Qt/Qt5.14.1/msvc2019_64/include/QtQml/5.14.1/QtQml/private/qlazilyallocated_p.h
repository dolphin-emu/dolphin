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

#ifndef QLAZILYALLOCATED_P_H
#define QLAZILYALLOCATED_P_H

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

#include <QtCore/qglobal.h>

#include <private/qflagpointer_p.h>

QT_BEGIN_NAMESPACE

template<typename T>
class QLazilyAllocated {
public:
    inline QLazilyAllocated();
    inline ~QLazilyAllocated();

    inline bool isAllocated() const;

    inline T *operator->() const;

    inline T &value();
    inline const T &value() const;

    inline bool flag() const;
    inline void setFlag();
    inline void clearFlag();
    inline void setFlagValue(bool);
private:
    mutable QFlagPointer<T> d;
};

template<typename T>
QLazilyAllocated<T>::QLazilyAllocated()
{
}

template<typename T>
QLazilyAllocated<T>::~QLazilyAllocated()
{
    delete *d;
}

template<typename T>
bool QLazilyAllocated<T>::isAllocated() const
{
    return !d.isNull();
}

template<typename T>
T &QLazilyAllocated<T>::value()
{
    if (d.isNull()) d = new T;
    return *(*d);
}

template<typename T>
const T &QLazilyAllocated<T>::value() const
{
    if (d.isNull()) d = new T;
    return *(*d);
}

template<typename T>
T *QLazilyAllocated<T>::operator->() const
{
    return *d;
}

template<typename T>
bool QLazilyAllocated<T>::flag() const
{
    return d.flag();
}

template<typename T>
void QLazilyAllocated<T>::setFlag()
{
    d.setFlag();
}

template<typename T>
void QLazilyAllocated<T>::clearFlag()
{
    d.clearFlag();
}

template<typename T>
void QLazilyAllocated<T>::setFlagValue(bool v)
{
    d.setFlagValue(v);
}

QT_END_NAMESPACE

#endif // QLAZILYALLOCATED_P_H
