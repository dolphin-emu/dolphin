/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QMAKEARRAY_P_H
#define QMAKEARRAY_P_H

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

#include "QtCore/qglobal.h"

#include <array>
#include <type_traits>
#include <utility>

QT_BEGIN_NAMESPACE

namespace QtPrivate {
template<typename T>
constexpr T&& Forward(typename std::remove_reference<T>::type& t) noexcept
{
    return static_cast<T&&>(t);
}

template<typename T>
constexpr T&& Forward(typename std::remove_reference<T>::type&& t) noexcept
{
    static_assert(!std::is_lvalue_reference<T>::value,
                  "template argument substituting T is an lvalue reference type");
    return static_cast<T&&>(t);
}

template <typename ManualType, typename ...>
struct ArrayTypeHelper
{
    using type = ManualType;
};

template <typename ... Types>
struct ArrayTypeHelper<void, Types...> : std::common_type<Types...> { };

template <typename ManualType, typename... Types>
using ArrayType = std::array<typename ArrayTypeHelper<ManualType, Types...>::type,
                             sizeof...(Types)>;

template<typename ... Values>
struct QuickSortData { };

template <template <typename> class Predicate,
          typename ... Values>
struct QuickSortFilter;

template <typename ... Right, typename ... Left>
constexpr QuickSortData<Right..., Left...> quickSortConcat(
    QuickSortData<Right...>, QuickSortData<Left...>) noexcept;

template<typename ... Right, typename Middle, typename ... Left>
constexpr QuickSortData<Right..., Middle, Left...> quickSortConcat(
    QuickSortData<Right...>,
    QuickSortData<Middle>,
    QuickSortData<Left...>) noexcept;

template <template <typename> class Predicate,
          typename Head, typename ... Tail>
struct QuickSortFilter<Predicate, QuickSortData<Head, Tail...>>
{
    using TailFilteredData = typename QuickSortFilter<
        Predicate, QuickSortData<Tail...>>::Type;

    using Type = typename std::conditional<
        Predicate<Head>::value,
        decltype(quickSortConcat(QuickSortData<Head> {}, TailFilteredData{})),
        TailFilteredData>::type;
};

template <template <typename> class Predicate>
struct QuickSortFilter<Predicate, QuickSortData<>>
{
    using Type = QuickSortData<>;
};

template <typename ... Values>
struct QuickSort;

template <typename Pivot, typename ... Values>
struct QuickSort<QuickSortData<Pivot, Values...>>
{
    template <typename Left>
    struct LessThan {
        static constexpr const bool value = Left::data() <= Pivot::data();
    };

    template <typename Left>
    struct MoreThan {
        static constexpr const bool value = !(Left::data() <= Pivot::data());
    };

    using LeftSide = typename QuickSortFilter<LessThan, QuickSortData<Values...>>::Type;
    using RightSide = typename QuickSortFilter<MoreThan, QuickSortData<Values...>>::Type;

    using LeftQS = typename QuickSort<LeftSide>::Type;
    using RightQS = typename QuickSort<RightSide>::Type;

    using Type = decltype(quickSortConcat(LeftQS{}, QuickSortData<Pivot> {}, RightQS{}));

};

template <>
struct QuickSort<QuickSortData<>>
{
    using Type = QuickSortData<>;
};
} // namespace QtPrivate

template <typename ManualType = void, typename ... Types>
constexpr QtPrivate::ArrayType<ManualType, Types...> qMakeArray(Types && ... t) noexcept
{
    return {{QtPrivate::Forward<typename QtPrivate::ArrayType<ManualType, Types...>::value_type>(t)...}};
}

template<typename ... Values>
struct QSortedData {
    using Data = typename QtPrivate::QuickSort<typename QtPrivate::QuickSortData<Values...>>::Type;
};

template<typename ... Values>
constexpr auto qMakeArray(QtPrivate::QuickSortData<Values...>) noexcept -> decltype(qMakeArray(Values::data()...))
{
    return qMakeArray(Values::data() ...);
}


QT_END_NAMESPACE

#endif // QMAKEARRAY_P_H
