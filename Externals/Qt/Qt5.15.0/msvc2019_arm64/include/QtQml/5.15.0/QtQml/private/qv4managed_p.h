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
#ifndef QMLJS_MANAGED_H
#define QMLJS_MANAGED_H

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

#include "qv4global_p.h"
#include "qv4value_p.h"
#include "qv4enginebase_p.h"
#include <private/qv4heap_p.h>
#include <private/qv4vtable_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

#define Q_MANAGED_CHECK \
    template <typename Type> inline void qt_check_for_QMANAGED_macro(const Type *_q_argument) const \
    { int i = qYouForgotTheQ_MANAGED_Macro(this, _q_argument); i = i + 1; }

template <typename T>
inline int qYouForgotTheQ_MANAGED_Macro(T, T) { return 0; }

template <typename T1, typename T2>
inline void qYouForgotTheQ_MANAGED_Macro(T1, T2) {}

#define V4_MANAGED_SIZE_TEST void __dataTest() { static_assert (sizeof(*this) == sizeof(Managed), "Classes derived from Managed can't have own data members."); }

#define V4_NEEDS_DESTROY static void virtualDestroy(QV4::Heap::Base *b) { static_cast<Data *>(b)->destroy(); }


#define V4_MANAGED_ITSELF(DataClass, superClass) \
    public: \
        Q_MANAGED_CHECK \
        typedef QV4::Heap::DataClass Data; \
        typedef superClass SuperClass; \
        static const QV4::VTable static_vtbl; \
        static inline const QV4::VTable *staticVTable() { return &static_vtbl; } \
        V4_MANAGED_SIZE_TEST \
        QV4::Heap::DataClass *d_unchecked() const { return static_cast<QV4::Heap::DataClass *>(m()); } \
        QV4::Heap::DataClass *d() const { \
            QV4::Heap::DataClass *dptr = d_unchecked(); \
            dptr->_checkIsInitialized(); \
            return dptr; \
        }

#define V4_MANAGED(DataClass, superClass) \
    private: \
        DataClass() Q_DECL_EQ_DELETE; \
        Q_DISABLE_COPY(DataClass) \
        V4_MANAGED_ITSELF(DataClass, superClass) \
        Q_STATIC_ASSERT(std::is_trivial< QV4::Heap::DataClass >::value);

#define Q_MANAGED_TYPE(type) \
    public: \
        enum { MyType = Type_##type };

#define V4_INTERNALCLASS(c) \
    static Heap::InternalClass *defaultInternalClass(QV4::EngineBase *e) \
        { return e->internalClasses(QV4::EngineBase::Class_##c); }

struct Q_QML_PRIVATE_EXPORT Managed : Value, VTableBase
{
    V4_MANAGED_ITSELF(Base, Managed)
    enum {
        IsExecutionContext = false,
        IsString = false,
        IsStringOrSymbol = false,
        IsObject = false,
        IsFunctionObject = false,
        IsErrorObject = false,
        IsArrayData = false
    };
private:
    void *operator new(size_t);
    Managed() Q_DECL_EQ_DELETE;
    Q_DISABLE_COPY(Managed)

public:
    enum { NInlineProperties = 0 };

    enum Type {
        Type_Invalid,
        Type_String,
        Type_Object,
        Type_Symbol,
        Type_ArrayObject,
        Type_FunctionObject,
        Type_GeneratorObject,
        Type_BooleanObject,
        Type_NumberObject,
        Type_StringObject,
        Type_SymbolObject,
        Type_DateObject,
        Type_RegExpObject,
        Type_ErrorObject,
        Type_ArgumentsObject,
        Type_JsonObject,
        Type_MathObject,
        Type_ProxyObject,

        Type_ExecutionContext,
        Type_InternalClass,
        Type_SetIteratorObject,
        Type_MapIteratorObject,
        Type_ArrayIteratorObject,
        Type_StringIteratorObject,
        Type_ForInIterator,
        Type_RegExp,

        Type_QmlSequence
    };
    Q_MANAGED_TYPE(Invalid)

    Heap::InternalClass *internalClass() const { return d()->internalClass; }
    const VTable *vtable() const { return d()->internalClass->vtable; }
    inline ExecutionEngine *engine() const { return internalClass()->engine; }

    bool isListType() const { return d()->internalClass->vtable->type == Type_QmlSequence; }
    bool isArrayLike() const { return isArrayObject() || isListType(); }

    bool isArrayObject() const { return d()->internalClass->vtable->type == Type_ArrayObject; }
    bool isStringObject() const { return d()->internalClass->vtable->type == Type_StringObject; }
    bool isSymbolObject() const { return d()->internalClass->vtable->type == Type_SymbolObject; }

    QString className() const;

    bool isEqualTo(const Managed *other) const
    { return d()->internalClass->vtable->isEqualTo(const_cast<Managed *>(this), const_cast<Managed *>(other)); }

    bool inUse() const { return d()->inUse(); }
    bool markBit() const { return d()->isMarked(); }
    inline void mark(MarkStack *markStack);

    Q_ALWAYS_INLINE Heap::Base *heapObject() const {
        return m();
    }

    template<typename T> inline T *cast() {
        return static_cast<T *>(this);
    }
    template<typename T> inline const T *cast() const {
        return static_cast<const T *>(this);
    }

protected:
    static bool virtualIsEqualTo(Managed *m, Managed *other);

private:
    friend class MemoryManager;
    friend struct Identifiers;
    friend struct ObjectIterator;
};

inline void Managed::mark(MarkStack *markStack)
{
    Q_ASSERT(m());
    m()->mark(markStack);
}

template<>
inline const Managed *Value::as() const {
    return managed();
}

template<>
inline const Object *Value::as() const {
    return objectValue();
}


struct InternalClass : Managed
{
    V4_MANAGED_ITSELF(InternalClass, Managed)
    Q_MANAGED_TYPE(InternalClass)
    V4_INTERNALCLASS(Empty)
    V4_NEEDS_DESTROY

    Q_REQUIRED_RESULT Heap::InternalClass *changeVTable(const VTable *vt) {
        return d()->changeVTable(vt);
    }
    Q_REQUIRED_RESULT Heap::InternalClass *changePrototype(Heap::Object *proto) {
        return d()->changePrototype(proto);
    }
    Q_REQUIRED_RESULT Heap::InternalClass *addMember(PropertyKey identifier, PropertyAttributes data, InternalClassEntry *entry = nullptr) {
        return d()->addMember(identifier, data, entry);
    }

    Q_REQUIRED_RESULT Heap::InternalClass *changeMember(PropertyKey identifier, PropertyAttributes data, InternalClassEntry *entry = nullptr) {
        return d()->changeMember(identifier, data, entry);
    }

    void operator =(Heap::InternalClass *ic) {
        Value::operator=(ic);
    }
};

}


QT_END_NAMESPACE

#endif
