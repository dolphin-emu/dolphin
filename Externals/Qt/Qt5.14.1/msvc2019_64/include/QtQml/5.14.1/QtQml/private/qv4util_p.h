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
#ifndef QV4UTIL_H
#define QV4UTIL_H

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

#include <QtCore/QBitArray>
#include <algorithm>
#include <vector>

QT_BEGIN_NAMESPACE

namespace QV4 {

#if !defined(BROKEN_STD_VECTOR_BOOL_OR_BROKEN_STD_FIND)
// Sanity:
class BitVector
{
    std::vector<bool> bits;

public:
    BitVector(int size = 0, bool value = false)
        : bits(size, value)
    {}

    void clear()
    { bits = std::vector<bool>(bits.size(), false); }

    void reserve(int size)
    { bits.reserve(size); }

    int size() const
    {
        Q_ASSERT(bits.size() < INT_MAX);
        return static_cast<int>(bits.size());
    }

    void resize(int newSize)
    { bits.resize(newSize); }

    void resize(int newSize, bool newValue)
    { bits.resize(newSize, newValue); }

    void assign(int newSize, bool value)
    { bits.assign(newSize, value); }

    int findNext(int start, bool value, bool wrapAround) const
    {
        // The ++operator of std::vector<bool>::iterator in libc++ has a bug when using it on an
        // iterator pointing to the last element. It will not be set to ::end(), but beyond
        // that. (It will be set to the first multiple of the native word size that is bigger
        // than size().)
        //
        // See http://llvm.org/bugs/show_bug.cgi?id=19663
        //
        // The work-around is to calculate the distance, and compare it to the size() to see if it's
        // beyond the end, or take the minimum of the distance and the size.

        size_t pos = std::distance(bits.begin(),
                                   std::find(bits.begin() + start, bits.end(), value));
        if (wrapAround && pos >= static_cast<size_t>(size()))
            pos = std::distance(bits.begin(),
                                std::find(bits.begin(), bits.begin() + start, value));

        pos = qMin(pos, static_cast<size_t>(size()));

        Q_ASSERT(pos <= static_cast<size_t>(size()));
        Q_ASSERT(pos < INT_MAX);

        return static_cast<int>(pos);
    }

    bool at(int idx) const
    { return bits.at(idx); }

    void setBit(int idx)
    { bits[idx] = true; }

    void clearBit(int idx)
    { bits[idx] = false; }
};
#else // Insanity:
class BitVector
{
    QBitArray bits;

public:
    BitVector(int size = 0, bool value = false)
        : bits(size, value)
    {}

    void clear()
    { bits = QBitArray(bits.size(), false); }

    void reserve(int size)
    { Q_UNUSED(size); }

    int size() const
    { return bits.size(); }

    void resize(int newSize)
    { bits.resize(newSize); }

    void resize(int newSize, bool newValue)
    {
        int oldSize = bits.size();
        bits.resize(newSize);
        bits.fill(newValue, oldSize, bits.size());
    }

    void assign(int newSize, bool value)
    {
        bits.resize(newSize);
        bits.fill(value);
    }

    int findNext(int start, bool value, bool wrapAround) const
    {
        for (int i = start, ei = size(); i < ei; ++i) {
            if (at(i) == value)
                return i;
        }

        if (wrapAround) {
            for (int i = 0, ei = start; i < ei; ++i) {
                if (at(i) == value)
                    return i;
            }
        }

        return size();
    }

    bool at(int idx) const
    { return bits.at(idx); }

    void setBit(int idx)
    { bits[idx] = true; }

    void clearBit(int idx)
    { bits[idx] = false; }
};
#endif

}

QT_END_NAMESPACE

#endif // QV4UTIL_H
