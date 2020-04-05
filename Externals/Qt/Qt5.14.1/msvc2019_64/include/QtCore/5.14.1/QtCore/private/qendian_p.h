/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QENDIAN_P_H
#define QENDIAN_P_H

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

#include <QtCore/qendian.h>

QT_BEGIN_NAMESPACE

// Note if using multiple of these bitfields in a union; the underlying storage type must
// match. Since we always use an unsigned storage type, unsigned and signed versions may
// be used together, but different bit-widths may not.
template<class S, int pos, int width>
class QSpecialIntegerBitfield
{
protected:
    typedef typename S::StorageType T;
    typedef typename std::make_unsigned<T>::type UT;

    static Q_DECL_CONSTEXPR UT mask()
    {
        return ((UT(1) << width) - 1) << pos;
    }
public:
    // FIXME: val is public until qtdeclarative is fixed to not access it directly.
    UT val;

    QSpecialIntegerBitfield &operator =(T t)
    {
        UT i = S::fromSpecial(val);
        i &= ~mask();
        i |= (UT(t) << pos) & mask();
        val  = S::toSpecial(i);
        return *this;
    }
    operator T() const
    {
        if (std::is_signed<T>::value) {
            UT i = S::fromSpecial(val);
            i <<= (sizeof(T) * 8) - width - pos;
            T t = T(i);
            t >>= (sizeof(T) * 8) - width;
            return t;
        }
        return (S::fromSpecial(val) & mask()) >> pos;
    }

    bool operator !() const { return !(val & S::toSpecial(mask())); }
    bool operator ==(QSpecialIntegerBitfield<S, pos, width> i) const
    {   return ((val ^ i.val) & S::toSpecial(mask())) == 0; }
    bool operator !=(QSpecialIntegerBitfield<S, pos, width> i) const
    {   return ((val ^ i.val) & S::toSpecial(mask())) != 0; }

    QSpecialIntegerBitfield &operator +=(T i)
    {   return (*this = (T(*this) + i)); }
    QSpecialIntegerBitfield &operator -=(T i)
    {   return (*this = (T(*this) - i)); }
    QSpecialIntegerBitfield &operator *=(T i)
    {   return (*this = (T(*this) * i)); }
    QSpecialIntegerBitfield &operator /=(T i)
    {   return (*this = (T(*this) / i)); }
    QSpecialIntegerBitfield &operator %=(T i)
    {   return (*this = (T(*this) % i)); }
    QSpecialIntegerBitfield &operator |=(T i)
    {   return (*this = (T(*this) | i)); }
    QSpecialIntegerBitfield &operator &=(T i)
    {   return (*this = (T(*this) & i)); }
    QSpecialIntegerBitfield &operator ^=(T i)
    {   return (*this = (T(*this) ^ i)); }
    QSpecialIntegerBitfield &operator >>=(T i)
    {   return (*this = (T(*this) >> i)); }
    QSpecialIntegerBitfield &operator <<=(T i)
    {   return (*this = (T(*this) << i)); }
};

template<typename T, int pos, int width>
using QLEIntegerBitfield = QSpecialIntegerBitfield<QLittleEndianStorageType<T>, pos, width>;

template<typename T, int pos, int width>
using QBEIntegerBitfield = QSpecialIntegerBitfield<QBigEndianStorageType<T>, pos, width>;

template<int pos, int width>
using qint32_le_bitfield = QLEIntegerBitfield<int, pos, width>;
template<int pos, int width>
using quint32_le_bitfield = QLEIntegerBitfield<uint, pos, width>;
template<int pos, int width>
using qint32_be_bitfield = QBEIntegerBitfield<int, pos, width>;
template<int pos, int width>
using quint32_be_bitfield = QBEIntegerBitfield<uint, pos, width>;


QT_END_NAMESPACE

#endif // QENDIAN_P_H
