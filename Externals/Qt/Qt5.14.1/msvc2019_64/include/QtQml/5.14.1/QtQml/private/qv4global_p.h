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

#ifndef QV4GLOBAL_H
#define QV4GLOBAL_H

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
#include <private/qv4compilerglobal_p.h>
#include <QString>

#ifdef QT_NO_DEBUG
#define QML_NEARLY_ALWAYS_INLINE Q_ALWAYS_INLINE
#else
#define QML_NEARLY_ALWAYS_INLINE inline
#endif

#include <qtqmlglobal.h>
#include <private/qtqmlglobal_p.h>

#if defined(Q_CC_MSVC)
#include <float.h>
#include <math.h>

namespace std {

inline bool isinf(double d) { return !_finite(d) && !_isnan(d); }
inline bool isnan(double d) { return !!_isnan(d); }
inline bool isfinite(double d) { return _finite(d); }

} // namespace std

inline double trunc(double d) { return d > 0 ? floor(d) : ceil(d); }
#endif

// Do certain things depending on whether the JIT is enabled or disabled

#if QT_CONFIG(qml_jit)
#define ENABLE_YARR_JIT 1
#define ENABLE_JIT 1
#define ENABLE_ASSEMBLER 1
#else
#define ENABLE_YARR_JIT 0
#define ENABLE_ASSEMBLER 0
#define ENABLE_JIT 0
#endif

#if defined(Q_OS_QNX) && defined(_CPPLIB_VER)
#include <math.h>
#undef isnan
#undef isfinite
#undef isinf
#undef signbit
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Compiler {
    struct Module;
    struct Context;
    struct JSUnitGenerator;
    class Codegen;
}

namespace Moth {
    class BytecodeGenerator;
}

namespace Heap {
    struct Base;
    struct MemberData;
    struct ArrayData;

    struct StringOrSymbol;
    struct String;
    struct Symbol;
    struct Object;
    struct ObjectPrototype;

    struct ExecutionContext;
    struct CallContext;
    struct QmlContext;
    struct ScriptFunction;
    struct InternalClass;

    struct BooleanObject;
    struct NumberObject;
    struct StringObject;
    struct ArrayObject;
    struct DateObject;
    struct FunctionObject;
    struct ErrorObject;
    struct ArgumentsObject;
    struct QObjectWrapper;
    struct RegExpObject;
    struct RegExp;
    struct EvalFunction;

    struct SharedArrayBuffer;
    struct ArrayBuffer;
    struct DataView;
    struct TypedArray;

    struct MapObject;
    struct SetObject;

    struct PromiseObject;
    struct PromiseCapability;

    template <typename T, size_t> struct Pointer;
}

struct CppStackFrame;
class MemoryManager;
class ExecutableAllocator;
struct PropertyKey;
struct StringOrSymbol;
struct String;
struct Symbol;
struct Object;
struct ObjectPrototype;
struct ObjectIterator;
struct ExecutionContext;
struct CallContext;
struct QmlContext;
struct ScriptFunction;
struct InternalClass;
struct Property;
struct Value;
template<size_t> struct HeapValue;
template<size_t> struct ValueArray;
struct Lookup;
struct ArrayData;
struct VTable;
struct Function;

struct BooleanObject;
struct NumberObject;
struct StringObject;
struct ArrayObject;
struct DateObject;
struct FunctionObject;
struct ErrorObject;
struct ArgumentsObject;
struct Managed;
struct ExecutionEngine;
struct QObjectWrapper;
struct RegExpObject;
struct RegExp;
struct EvalFunction;

struct SharedArrayBuffer;
struct ArrayBuffer;
struct DataView;
struct TypedArray;

struct MapObject;
struct SetMapObject;

struct PromiseObject;
struct PromiseCapability;

struct CallData;
struct Scope;
struct ScopedValue;
template<typename T> struct Scoped;
typedef Scoped<String> ScopedString;
typedef Scoped<StringOrSymbol> ScopedStringOrSymbol;
typedef Scoped<Object> ScopedObject;
typedef Scoped<ArrayObject> ScopedArrayObject;
typedef Scoped<FunctionObject> ScopedFunctionObject;
typedef Scoped<ExecutionContext> ScopedContext;

struct PersistentValueStorage;
class PersistentValue;
class WeakValue;
struct MarkStack;

struct IdentifierTable;
class RegExpCache;
class MultiplyWrappedQObjectMap;

enum PropertyFlag {
    Attr_Data = 0,
    Attr_Accessor = 0x1,
    Attr_NotWritable = 0x2,
    Attr_NotEnumerable = 0x4,
    Attr_NotConfigurable = 0x8,
    Attr_ReadOnly = Attr_NotWritable|Attr_NotEnumerable|Attr_NotConfigurable,
    Attr_ReadOnly_ButConfigurable = Attr_NotWritable|Attr_NotEnumerable,
    Attr_Invalid = 0xff
};

Q_DECLARE_FLAGS(PropertyFlags, PropertyFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(PropertyFlags)

struct PropertyAttributes
{
    union {
        uchar m_all;
        struct {
            uchar m_flags : 4;
            uchar m_mask : 4;
        };
        struct {
            uchar m_type : 1;
            uchar m_writable : 1;
            uchar m_enumerable : 1;
            uchar m_configurable : 1;
            uchar type_set : 1;
            uchar writable_set : 1;
            uchar enumerable_set : 1;
            uchar configurable_set : 1;
        };
    };

    enum Type {
        Data = 0,
        Accessor = 1,
        Generic = 2
    };

    PropertyAttributes() : m_all(0) {}
    PropertyAttributes(PropertyFlag f) : m_all(0) {
        if (f != Attr_Invalid) {
            setType(f & Attr_Accessor ? Accessor : Data);
            if (!(f & Attr_Accessor))
                setWritable(!(f & Attr_NotWritable));
            setEnumerable(!(f & Attr_NotEnumerable));
            setConfigurable(!(f & Attr_NotConfigurable));
        }
    }
    PropertyAttributes(PropertyFlags f) : m_all(0) {
        if (f != Attr_Invalid) {
            setType(f & Attr_Accessor ? Accessor : Data);
            if (!(f & Attr_Accessor))
                setWritable(!(f & Attr_NotWritable));
            setEnumerable(!(f & Attr_NotEnumerable));
            setConfigurable(!(f & Attr_NotConfigurable));
        }
    }

    void setType(Type t) { m_type = t; type_set = true; }
    Type type() const { return type_set ? (Type)m_type : Generic; }

    bool isData() const { return type() == PropertyAttributes::Data || writable_set; }
    bool isAccessor() const { return type() == PropertyAttributes::Accessor; }
    bool isGeneric() const { return type() == PropertyAttributes::Generic && !writable_set; }

    bool hasType() const { return type_set; }
    bool hasWritable() const { return writable_set; }
    bool hasConfigurable() const { return configurable_set; }
    bool hasEnumerable() const { return enumerable_set; }

    void setWritable(bool b) { m_writable = b; writable_set = true; }
    void setConfigurable(bool b) { m_configurable = b; configurable_set = true; }
    void setEnumerable(bool b) { m_enumerable = b; enumerable_set = true; }

    void resolve() { m_mask = 0xf; if (m_type == Accessor) { m_writable = false; writable_set = false; } }

    bool isWritable() const { return m_type != Data || m_writable; }
    bool isEnumerable() const { return m_enumerable; }
    bool isConfigurable() const { return m_configurable; }

    void clearType() { m_type = Data; type_set = false; }
    void clearWritable() { m_writable = false; writable_set = false; }
    void clearEnumerable() { m_enumerable = false; enumerable_set = false; }
    void clearConfigurable() { m_configurable = false; configurable_set = false; }

    void clear() { m_all = 0; }
    bool isEmpty() const { return !m_all; }

    uint flags() const { return m_flags; }
    uint all() const { return m_all; }

    bool operator==(PropertyAttributes other) {
        return m_all == other.m_all;
    }
    bool operator!=(PropertyAttributes other) {
        return m_all != other.m_all;
    }
};

struct Q_QML_EXPORT StackFrame {
    QString source;
    QString function;
    int line = -1;
    int column = -1;
};
typedef QVector<StackFrame> StackTrace;

namespace JIT {

enum class CallResultDestination {
    Ignore,
    InAccumulator,
};

} // JIT namespace

} // QV4 namespace

Q_DECLARE_TYPEINFO(QV4::PropertyAttributes, Q_PRIMITIVE_TYPE);

QT_END_NAMESPACE

#endif // QV4GLOBAL_H
