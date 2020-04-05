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

#ifndef QFINITESTACK_P_H
#define QFINITESTACK_P_H

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

QT_BEGIN_NAMESPACE

template<typename T>
struct QFiniteStack {
    inline QFiniteStack();
    inline ~QFiniteStack();

    inline void deallocate();
    inline void allocate(int size);

    inline int capacity() const { return _alloc; }

    inline bool isEmpty() const;
    inline const T &top() const;
    inline T &top();
    inline void push(const T &o);
    inline T pop();
    inline int count() const;
    inline const T &at(int index) const;
    inline T &operator[](int index);
private:
    T *_array;
    int _alloc;
    int _size;
};

template<typename T>
QFiniteStack<T>::QFiniteStack()
: _array(nullptr), _alloc(0), _size(0)
{
}

template<typename T>
QFiniteStack<T>::~QFiniteStack()
{
    deallocate();
}

template<typename T>
bool QFiniteStack<T>::isEmpty() const
{
    return _size == 0;
}

template<typename T>
const T &QFiniteStack<T>::top() const
{
    return _array[_size - 1];
}

template<typename T>
T &QFiniteStack<T>::top()
{
    return _array[_size - 1];
}

template<typename T>
void QFiniteStack<T>::push(const T &o)
{
    Q_ASSERT(_size < _alloc);
    if (QTypeInfo<T>::isComplex) {
        new (_array + _size++) T(o);
    } else {
        _array[_size++] = o;
    }
}

template<typename T>
T QFiniteStack<T>::pop()
{
    Q_ASSERT(_size > 0);
    --_size;

    if (QTypeInfo<T>::isComplex) {
        T rv = _array[_size];
        (_array + _size)->~T();
        return rv;
    } else {
        return _array[_size];
    }
}

template<typename T>
int QFiniteStack<T>::count() const
{
    return _size;
}

template<typename T>
const T &QFiniteStack<T>::at(int index) const
{
    return _array[index];
}

template<typename T>
T &QFiniteStack<T>::operator[](int index)
{
    return _array[index];
}

template<typename T>
void QFiniteStack<T>::allocate(int size)
{
    Q_ASSERT(_array == nullptr);
    Q_ASSERT(_alloc == 0);
    Q_ASSERT(_size == 0);

    if (!size) return;

    _array = (T *)malloc(size * sizeof(T));
    _alloc = size;
}

template<typename T>
void QFiniteStack<T>::deallocate()
{
    if (QTypeInfo<T>::isComplex) {
        T *i = _array + _size;
        while (i != _array)
            (--i)->~T();
    }

    free(_array);

    _array = nullptr;
    _alloc = 0;
    _size = 0;
}

QT_END_NAMESPACE

#endif // QFINITESTACK_P_H

