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
#ifndef QV4HEAP_P_H
#define QV4HEAP_P_H

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

#include <QtCore/QString>
#include <private/qv4global_p.h>
#include <private/qv4mmdefs_p.h>
#include <private/qv4writebarrier_p.h>
#include <private/qv4vtable_p.h>
#include <QSharedPointer>

// To check if Heap::Base::init is called (meaning, all subclasses did their init and called their
// parent's init all up the inheritance chain), define QML_CHECK_INIT_DESTROY_CALLS below.
#undef QML_CHECK_INIT_DESTROY_CALLS

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

template <typename T, size_t o>
struct Pointer {
    static Q_CONSTEXPR size_t offset = o;
    T operator->() const { return get(); }
    operator T () const { return get(); }

    Base *base();

    void set(EngineBase *e, T newVal) {
        WriteBarrier::write(e, base(), &ptr, reinterpret_cast<Base *>(newVal));
    }

    T get() const { return reinterpret_cast<T>(ptr); }

    template <typename Type>
    Type *cast() { return static_cast<Type *>(ptr); }

    Base *heapObject() const { return ptr; }

private:
    Base *ptr;
};
typedef Pointer<char *, 0> V4PointerCheck;
Q_STATIC_ASSERT(std::is_trivial< V4PointerCheck >::value);

struct Q_QML_EXPORT Base {
    void *operator new(size_t) = delete;

    static void markObjects(Base *, MarkStack *);

    Pointer<InternalClass *, 0> internalClass;

    inline ReturnedValue asReturnedValue() const;
    inline void mark(QV4::MarkStack *markStack);

    inline bool isMarked() const {
        const HeapItem *h = reinterpret_cast<const HeapItem *>(this);
        Chunk *c = h->chunk();
        Q_ASSERT(!Chunk::testBit(c->extendsBitmap, h - c->realBase()));
        return Chunk::testBit(c->blackBitmap, h - c->realBase());
    }
    inline void setMarkBit() {
        const HeapItem *h = reinterpret_cast<const HeapItem *>(this);
        Chunk *c = h->chunk();
        Q_ASSERT(!Chunk::testBit(c->extendsBitmap, h - c->realBase()));
        return Chunk::setBit(c->blackBitmap, h - c->realBase());
    }
    inline void setGrayBit() {
        const HeapItem *h = reinterpret_cast<const HeapItem *>(this);
        Chunk *c = h->chunk();
        Q_ASSERT(!Chunk::testBit(c->extendsBitmap, h - c->realBase()));
        return Chunk::setBit(c->grayBitmap, h - c->realBase());
    }

    inline bool inUse() const {
        const HeapItem *h = reinterpret_cast<const HeapItem *>(this);
        Chunk *c = h->chunk();
        Q_ASSERT(!Chunk::testBit(c->extendsBitmap, h - c->realBase()));
        return Chunk::testBit(c->objectBitmap, h - c->realBase());
    }

    void *operator new(size_t, Managed *m) { return m; }
    void *operator new(size_t, Base *m) { return m; }
    void operator delete(void *, Base *) {}

    void init() { _setInitialized(); }
    void destroy() { _setDestroyed(); }
#ifdef QML_CHECK_INIT_DESTROY_CALLS
    enum { Uninitialized = 0, Initialized, Destroyed } _livenessStatus;
    void _checkIsInitialized() {
        if (_livenessStatus == Uninitialized)
            fprintf(stderr, "ERROR: use of object '%s' before call to init() !!\n",
                    vtable()->className);
        else if (_livenessStatus == Destroyed)
            fprintf(stderr, "ERROR: use of object '%s' after call to destroy() !!\n",
                    vtable()->className);
        Q_ASSERT(_livenessStatus == Initialized);
    }
    void _checkIsDestroyed() {
        if (_livenessStatus == Initialized)
            fprintf(stderr, "ERROR: object '%s' was never destroyed completely !!\n",
                    vtable()->className);
        Q_ASSERT(_livenessStatus == Destroyed);
    }
    void _setInitialized() { Q_ASSERT(_livenessStatus == Uninitialized); _livenessStatus = Initialized; }
    void _setDestroyed() {
        if (_livenessStatus == Uninitialized)
            fprintf(stderr, "ERROR: attempting to destroy an uninitialized object '%s' !!\n",
                    vtable()->className);
        else if (_livenessStatus == Destroyed)
            fprintf(stderr, "ERROR: attempting to destroy repeatedly object '%s' !!\n",
                    vtable()->className);
        Q_ASSERT(_livenessStatus == Initialized);
        _livenessStatus = Destroyed;
    }
#else
    Q_ALWAYS_INLINE void _checkIsInitialized() {}
    Q_ALWAYS_INLINE void _checkIsDestroyed() {}
    Q_ALWAYS_INLINE void _setInitialized() {}
    Q_ALWAYS_INLINE void _setDestroyed() {}
#endif
};
Q_STATIC_ASSERT(std::is_trivial< Base >::value);
// This class needs to consist only of pointer sized members to allow
// for a size/offset translation when cross-compiling between 32- and
// 64-bit.
Q_STATIC_ASSERT(std::is_standard_layout<Base>::value);
Q_STATIC_ASSERT(offsetof(Base, internalClass) == 0);
Q_STATIC_ASSERT(sizeof(Base) == QT_POINTER_SIZE);

inline
void Base::mark(QV4::MarkStack *markStack)
{
    Q_ASSERT(inUse());
    const HeapItem *h = reinterpret_cast<const HeapItem *>(this);
    Chunk *c = h->chunk();
    size_t index = h - c->realBase();
    Q_ASSERT(!Chunk::testBit(c->extendsBitmap, index));
    quintptr *bitmap = c->blackBitmap + Chunk::bitmapIndex(index);
    quintptr bit = Chunk::bitForIndex(index);
    if (!(*bitmap & bit)) {
        *bitmap |= bit;
        markStack->push(this);
    }
}

template<typename T, size_t o>
Base *Pointer<T, o>::base() {
    Base *base = reinterpret_cast<Base *>(this) - (offset/sizeof(Base *));
    Q_ASSERT(base->inUse());
    return base;
}

}

#ifdef QT_NO_QOBJECT
template <class T>
struct QQmlQPointer {
};
#else
template <class T>
struct QQmlQPointer {
    void init()
    {
        d = nullptr;
        qObject = nullptr;
    }

    void init(T *o)
    {
        Q_ASSERT(d == nullptr);
        Q_ASSERT(qObject == nullptr);
        if (o) {
            d = QtSharedPointer::ExternalRefCountData::getAndRef(o);
            qObject = o;
        }
    }

    void destroy()
    {
        if (d && !d->weakref.deref())
            delete d;
        d = nullptr;
        qObject = nullptr;
    }

    T *data() const {
        return d == nullptr || d->strongref.loadRelaxed() == 0 ? nullptr : qObject;
    }
    operator T*() const { return data(); }
    inline T* operator->() const { return data(); }
    QQmlQPointer &operator=(T *o)
    {
        if (d)
            destroy();
        init(o);
        return *this;
    }
    bool isNull() const Q_DECL_NOTHROW
    {
        return d == nullptr || qObject == nullptr || d->strongref.loadRelaxed() == 0;
    }

private:
    QtSharedPointer::ExternalRefCountData *d;
    QObject *qObject;
};
Q_STATIC_ASSERT(std::is_trivial< QQmlQPointer<QObject> >::value);
#endif

}

QT_END_NAMESPACE

#endif
