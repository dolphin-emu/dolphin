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

#ifndef QQMLJSFIXEDPOOLARRAY_P_H
#define QQMLJSFIXEDPOOLARRAY_P_H

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
#include <private/qqmljsmemorypool_p.h>

QT_BEGIN_NAMESPACE

namespace QQmlJS {

template <typename T>
class FixedPoolArray
{
    T *data;
    int count = 0;

public:
    FixedPoolArray()
        : data(nullptr)
    {}

    FixedPoolArray(MemoryPool *pool, int size)
    { allocate(pool, size); }

    void allocate(MemoryPool *pool, int size)
    {
        count = size;
        data = reinterpret_cast<T*>(pool->allocate(count * sizeof(T)));
    }

    void allocate(MemoryPool *pool, const QVector<T> &vector)
    {
        count = vector.count();
        data = reinterpret_cast<T*>(pool->allocate(count * sizeof(T)));

        if (QTypeInfo<T>::isComplex) {
            for (int i = 0; i < count; ++i)
                new (data + i) T(vector.at(i));
        } else {
            memcpy(data, static_cast<const void*>(vector.constData()), count * sizeof(T));
        }
    }

    template <typename Container>
    void allocate(MemoryPool *pool, const Container &container)
    {
        count = container.count();
        data = reinterpret_cast<T*>(pool->allocate(count * sizeof(T)));
        typename Container::ConstIterator it = container.constBegin();
        for (int i = 0; i < count; ++i)
            new (data + i) T(*it++);
    }

    int size() const
    { return count; }

    const T &at(int index) const {
        Q_ASSERT(index >= 0 && index < count);
        return data[index];
    }

    T &at(int index) {
        Q_ASSERT(index >= 0 && index < count);
        return data[index];
    }

    T &operator[](int index) {
        return at(index);
    }


    int indexOf(const T &value) const {
        for (int i = 0; i < count; ++i)
            if (data[i] == value)
                return i;
        return -1;
    }

    const T *begin() const { return data; }
    const T *end() const { return data + count; }

    T *begin() { return data; }
    T *end() { return data + count; }
};

} // namespace QQmlJS

QT_END_NAMESPACE

#endif // QQMLJSFIXEDPOOLARRAY_P_H
