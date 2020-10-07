/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
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

#ifndef QVARIANT_P_H
#define QVARIANT_P_H

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
#include <QtCore/qvariant.h>
#include <QtCore/private/qmetatype_p.h>
#include <QtCore/qdebug.h>

#include "qmetatypeswitcher_p.h"

QT_BEGIN_NAMESPACE

template<typename T>
struct QVariantIntegrator
{
    static const bool CanUseInternalSpace = sizeof(T) <= sizeof(QVariant::Private::Data)
            && ((QTypeInfoQuery<T>::isRelocatable) || std::is_enum<T>::value);
    typedef std::integral_constant<bool, CanUseInternalSpace> CanUseInternalSpace_t;
};
Q_STATIC_ASSERT(QVariantIntegrator<double>::CanUseInternalSpace);
Q_STATIC_ASSERT(QVariantIntegrator<long int>::CanUseInternalSpace);
Q_STATIC_ASSERT(QVariantIntegrator<qulonglong>::CanUseInternalSpace);

#ifdef Q_CC_SUN // Sun CC picks the wrong overload, so introduce awful hack

// takes a type, returns the internal void* pointer cast
// to a pointer of the input type
template <typename T>
inline T *v_cast(const QVariant::Private *nd, T * = 0)
{
    QVariant::Private *d = const_cast<QVariant::Private *>(nd);
    return !QVariantIntegrator<T>::CanUseInternalSpace
            ? static_cast<T *>(d->data.shared->ptr)
            : static_cast<T *>(static_cast<void *>(&d->data.c));
}

#else // every other compiler in this world

template <typename T>
inline const T *v_cast(const QVariant::Private *d, T * = nullptr)
{
    return !QVariantIntegrator<T>::CanUseInternalSpace
            ? static_cast<const T *>(d->data.shared->ptr)
            : static_cast<const T *>(static_cast<const void *>(&d->data.c));
}

template <typename T>
inline T *v_cast(QVariant::Private *d, T * = nullptr)
{
    return !QVariantIntegrator<T>::CanUseInternalSpace
            ? static_cast<T *>(d->data.shared->ptr)
            : static_cast<T *>(static_cast<void *>(&d->data.c));
}

#endif

enum QVariantConstructionFlags : uint {
    Default = 0x0,
    PointerType = 0x1,
    ShouldDeleteVariantData = 0x2 // only used in Q*Iterable
};

//a simple template that avoids to allocate 2 memory chunks when creating a QVariant
template <class T> class QVariantPrivateSharedEx : public QVariant::PrivateShared
{
public:
    QVariantPrivateSharedEx() : QVariant::PrivateShared(&m_t), m_t() { }
    QVariantPrivateSharedEx(const T&t) : QVariant::PrivateShared(&m_t), m_t(t) { }

private:
    T m_t;
};

template <class T>
inline void v_construct_helper(QVariant::Private *x, const T &t, std::true_type)
{
    new (&x->data) T(t);
    x->is_shared = false;
}

template <class T>
inline void v_construct_helper(QVariant::Private *x, const T &t, std::false_type)
{
    x->data.shared = new QVariantPrivateSharedEx<T>(t);
    x->is_shared = true;
}

template <class T>
inline void v_construct_helper(QVariant::Private *x, std::true_type)
{
    new (&x->data) T();
    x->is_shared = false;
}

template <class T>
inline void v_construct_helper(QVariant::Private *x, std::false_type)
{
    x->data.shared = new QVariantPrivateSharedEx<T>;
    x->is_shared = true;
}

template <class T>
inline void v_construct(QVariant::Private *x, const T &t)
{
    // dispatch
    v_construct_helper(x, t, typename QVariantIntegrator<T>::CanUseInternalSpace_t());
}

// constructs a new variant if copy is 0, otherwise copy-constructs
template <class T>
inline void v_construct(QVariant::Private *x, const void *copy, T * = nullptr)
{
    if (copy)
        v_construct<T>(x, *static_cast<const T *>(copy));
    else
        v_construct_helper<T>(x, typename QVariantIntegrator<T>::CanUseInternalSpace_t());
}

// deletes the internal structures
template <class T>
inline void v_clear(QVariant::Private *d, T* = nullptr)
{

    if (!QVariantIntegrator<T>::CanUseInternalSpace) {
        //now we need to cast
        //because QVariant::PrivateShared doesn't have a virtual destructor
        delete static_cast<QVariantPrivateSharedEx<T>*>(d->data.shared);
    } else {
        v_cast<T>(d)->~T();
    }

}

template <typename T>
struct PrimitiveIsNull
{
public:
    static bool isNull(const QVariant::Private *d)
    {
        return d->is_null;
    }
};

template <typename T>
struct PrimitiveIsNull<T*>
{
public:
    static bool isNull(const QVariant::Private *d)
    {
        return d->is_null || d->data.ptr == nullptr;
    }
};

template <>
struct PrimitiveIsNull<std::nullptr_t>
{
public:
    static bool isNull(const QVariant::Private *)
    {
        return true;
    }
};

template<class Filter>
class QVariantComparator {
    template<typename T, bool IsAcceptedType = Filter::template Acceptor<T>::IsAccepted>
    struct FilteredComparator {
        static bool compare(const QVariant::Private *a, const QVariant::Private *b)
        {
            return *v_cast<T>(a) == *v_cast<T>(b);
        }
    };
    template<typename T>
    struct FilteredComparator<T, /* IsAcceptedType = */ false> {
        static bool compare(const QVariant::Private *, const QVariant::Private *)
        {
            // It is not possible to construct a QVariant containing not fully defined type
            Q_ASSERT(false);
            return false;
        }
    };
public:
    QVariantComparator(const QVariant::Private *a, const QVariant::Private *b)
        : m_a(a), m_b(b)
    {
        Q_ASSERT(a->type == b->type);
    }

    template<typename T>
    bool delegate(const T*)
    {
        return FilteredComparator<T>::compare(m_a, m_b);
    }

    bool delegate(const void*) { Q_ASSERT(false); return true; }
    bool delegate(const QMetaTypeSwitcher::UnknownType*)
    {
        return true; // for historical reason invalid variant == invalid variant
    }
    bool delegate(const QMetaTypeSwitcher::NotBuiltinType*) { return false; }
protected:
    const QVariant::Private *m_a;
    const QVariant::Private *m_b;
};


Q_CORE_EXPORT const QVariant::Handler *qcoreVariantHandler();

template<class Filter>
class QVariantIsNull
{
    /// \internal
    /// This class checks if a type T has method called isNull. Result is kept in the Value property
    /// TODO Can we somehow generalize it? A macro version?
    template<typename T>
    class HasIsNullMethod {
        struct Yes { char unused[1]; };
        struct No { char unused[2]; };
        Q_STATIC_ASSERT(sizeof(Yes) != sizeof(No));

        template<class C> static decltype(static_cast<const C*>(nullptr)->isNull(), Yes()) test(int);
        template<class C> static No test(...);
    public:
        static const bool Value = (sizeof(test<T>(0)) == sizeof(Yes));
    };

    // TODO This part should go to autotests during HasIsNullMethod generalization.
    Q_STATIC_ASSERT(!HasIsNullMethod<bool>::Value);
    struct SelfTest1 { bool isNull() const; };
    Q_STATIC_ASSERT(HasIsNullMethod<SelfTest1>::Value);
    struct SelfTest2 {};
    Q_STATIC_ASSERT(!HasIsNullMethod<SelfTest2>::Value);
    struct SelfTest3 : public SelfTest1 {};
    Q_STATIC_ASSERT(HasIsNullMethod<SelfTest3>::Value);
    struct SelfTestFinal1 final { bool isNull() const; };
    Q_STATIC_ASSERT(HasIsNullMethod<SelfTestFinal1>::Value);
    struct SelfTestFinal2 final {};
    Q_STATIC_ASSERT(!HasIsNullMethod<SelfTestFinal2>::Value);
    struct SelfTestFinal3 final : public SelfTest1 {};
    Q_STATIC_ASSERT(HasIsNullMethod<SelfTestFinal3>::Value);

    template<typename T, bool HasIsNull = HasIsNullMethod<T>::Value>
    struct CallFilteredIsNull
    {
        static bool isNull(const QVariant::Private *d)
        {
            return v_cast<T>(d)->isNull();
        }
    };
    template<typename T>
    struct CallFilteredIsNull<T, /* HasIsNull = */ false>
    {
        static bool isNull(const QVariant::Private *d)
        {
            return PrimitiveIsNull<T>::isNull(d);
        }
    };

    template<typename T, bool IsAcceptedType = Filter::template Acceptor<T>::IsAccepted>
    struct CallIsNull
    {
        static bool isNull(const QVariant::Private *d)
        {
            return CallFilteredIsNull<T>::isNull(d);
        }
    };
    template<typename T>
    struct CallIsNull<T, /* IsAcceptedType = */ false>
    {
        static bool isNull(const QVariant::Private *d)
        {
            return CallFilteredIsNull<T, false>::isNull(d);
        }
    };

public:
    QVariantIsNull(const QVariant::Private *d)
        : m_d(d)
    {}
    template<typename T>
    bool delegate(const T*)
    {
        return CallIsNull<T>::isNull(m_d);
    }
    // we need that as sizof(void) is undefined and it is needed in HasIsNullMethod
    bool delegate(const void *) { Q_ASSERT(false); return m_d->is_null; }
    bool delegate(const QMetaTypeSwitcher::UnknownType *) { return m_d->is_null; }
    bool delegate(const QMetaTypeSwitcher::NotBuiltinType *)
    {
        // QVariantIsNull is used only for built-in types
        Q_ASSERT(false);
        return m_d->is_null;
    }
protected:
    const QVariant::Private *m_d;
};

template<class Filter>
class QVariantConstructor
{
    template<typename T, bool IsAcceptedType = Filter::template Acceptor<T>::IsAccepted>
    struct FilteredConstructor {
        FilteredConstructor(const QVariantConstructor &tc)
        {
            v_construct<T>(tc.m_x, tc.m_copy);
            tc.m_x->is_null = !tc.m_copy;
        }
    };
    template<typename T>
    struct FilteredConstructor<T, /* IsAcceptedType = */ false> {
        FilteredConstructor(const QVariantConstructor &tc)
        {
            // ignore types that lives outside of the current library
            tc.m_x->type = QMetaType::UnknownType;
        }
    };
public:
    QVariantConstructor(QVariant::Private *x, const void *copy)
        : m_x(x)
        , m_copy(copy)
    {}

    template<typename T>
    void delegate(const T*)
    {
        FilteredConstructor<T>(*this);
    }

    void delegate(const QMetaTypeSwitcher::NotBuiltinType*)
    {
        // QVariantConstructor is used only for built-in types.
        Q_ASSERT(false);
    }

    void delegate(const void*)
    {
        qWarning("Trying to create a QVariant instance of QMetaType::Void type, an invalid QVariant will be constructed instead");
        m_x->type = QMetaType::UnknownType;
        m_x->is_shared = false;
        m_x->is_null = !m_copy;
    }

    void delegate(const QMetaTypeSwitcher::UnknownType*)
    {
        if (m_x->type != QMetaType::UnknownType) {
            qWarning("Trying to construct an instance of an invalid type, type id: %i", m_x->type);
            m_x->type = QMetaType::UnknownType;
        }
        m_x->is_shared = false;
        m_x->is_null = !m_copy;
    }
private:
    QVariant::Private *m_x;
    const void *m_copy;
};

template<class Filter>
class QVariantDestructor
{
    template<typename T, bool IsAcceptedType = Filter::template Acceptor<T>::IsAccepted>
    struct FilteredDestructor {
        FilteredDestructor(QVariant::Private *d)
        {
            v_clear<T>(d);
        }
    };
    template<typename T>
    struct FilteredDestructor<T, /* IsAcceptedType = */ false> {
        FilteredDestructor(QVariant::Private *)
        {
            // It is not possible to create not accepted type
            Q_ASSERT(false);
        }
    };

public:
    QVariantDestructor(QVariant::Private *d)
        : m_d(d)
    {}
    ~QVariantDestructor()
    {
        m_d->type = QMetaType::UnknownType;
        m_d->is_null = true;
        m_d->is_shared = false;
    }

    template<typename T>
    void delegate(const T*)
    {
        FilteredDestructor<T> cleaner(m_d);
    }

    void delegate(const QMetaTypeSwitcher::NotBuiltinType*)
    {
        // QVariantDestructor class is used only for a built-in type
        Q_ASSERT(false);
    }
    // Ignore nonconstructible type
    void delegate(const QMetaTypeSwitcher::UnknownType*) {}
    void delegate(const void*) { Q_ASSERT(false); }
private:
    QVariant::Private *m_d;
};

namespace QVariantPrivate {
Q_CORE_EXPORT void registerHandler(const int /* Modules::Names */ name, const QVariant::Handler *handler);
}

#if !defined(QT_NO_DEBUG_STREAM)
template<class Filter>
class QVariantDebugStream
{
    template<typename T, bool IsAcceptedType = Filter::template Acceptor<T>::IsAccepted>
    struct Filtered {
        Filtered(QDebug dbg, QVariant::Private *d)
        {
            dbg.nospace() << *v_cast<T>(d);
        }
    };
    template<typename T>
    struct Filtered<T, /* IsAcceptedType = */ false> {
        Filtered(QDebug /* dbg */, QVariant::Private *)
        {
            // It is not possible to construct not acccepted type, QVariantConstructor creates an invalid variant for them
            Q_ASSERT(false);
        }
    };

public:
    QVariantDebugStream(QDebug dbg, QVariant::Private *d)
        : m_debugStream(dbg)
        , m_d(d)
    {}

    template<typename T>
    void delegate(const T*)
    {
        Filtered<T> streamIt(m_debugStream, m_d);
        Q_UNUSED(streamIt);
    }

    void delegate(const QMetaTypeSwitcher::NotBuiltinType*)
    {
        // QVariantDebugStream class is used only for a built-in type
        Q_ASSERT(false);
    }
    void delegate(const QMetaTypeSwitcher::UnknownType*)
    {
        m_debugStream.nospace() << "QVariant::Invalid";
    }
    void delegate(const void*) { Q_ASSERT(false); }
private:
    QDebug m_debugStream;
    QVariant::Private *m_d;
};
#endif

QT_END_NAMESPACE

#endif // QVARIANT_P_H
