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
#ifndef QV4ENGINE_H
#define QV4ENGINE_H

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
#include "qv4managed_p.h"
#include "qv4context_p.h"
#include "qv4stackframe_p.h"
#include <private/qintrusivelist_p.h>
#include "qv4enginebase_p.h"
#include <private/qqmlrefcount_p.h>
#include <private/qqmldelayedcallqueue_p.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qmutex.h>

#include "qv4function_p.h"
#include <private/qv4compileddata_p.h>
#include <private/qv4executablecompilationunit_p.h>

namespace WTF {
class BumpPointerAllocator;
class PageAllocation;
}

#define V4_DEFINE_EXTENSION(dataclass, datafunction) \
    static inline dataclass *datafunction(QV4::ExecutionEngine *engine) \
    { \
        static int extensionId = -1; \
        if (extensionId == -1) { \
            QV4::ExecutionEngine::registrationMutex()->lock(); \
            if (extensionId == -1) \
                extensionId = QV4::ExecutionEngine::registerExtension(); \
            QV4::ExecutionEngine::registrationMutex()->unlock(); \
        } \
        dataclass *rv = (dataclass *)engine->extensionData(extensionId); \
        if (!rv) { \
            rv = new dataclass(engine); \
            engine->setExtensionData(extensionId, rv); \
        } \
        return rv; \
    } \


QT_BEGIN_NAMESPACE

#if QT_CONFIG(qml_network)
class QNetworkAccessManager;

namespace QV4 {
struct QObjectMethod;
namespace detail {
QNetworkAccessManager *getNetworkAccessManager(ExecutionEngine *engine);
}
}
#else
namespace QV4 { struct QObjectMethod; }
#endif // qml_network

// Used to allow a QObject method take and return raw V4 handles without having to expose
// 48 in the public API.
// Use like this:
//     class MyClass : public QObject {
//         Q_OBJECT
//         ...
//         Q_INVOKABLE void myMethod(QQmlV4Function*);
//     };
// The QQmlV8Function - and consequently the arguments and return value - only remains
// valid during the call.  If the return value isn't set within myMethod(), the will return
// undefined.

class QQmlV4Function
{
public:
    int length() const { return callData->argc(); }
    QV4::ReturnedValue operator[](int idx) const { return (idx < callData->argc() ? callData->args[idx].asReturnedValue() : QV4::Encode::undefined()); }
    void setReturnValue(QV4::ReturnedValue rv) { *retVal = rv; }
    QV4::ExecutionEngine *v4engine() const { return e; }
private:
    friend struct QV4::QObjectMethod;
    QQmlV4Function();
    QQmlV4Function(const QQmlV4Function &);
    QQmlV4Function &operator=(const QQmlV4Function &);

    QQmlV4Function(QV4::CallData *callData, QV4::Value *retVal, QV4::ExecutionEngine *e)
        : callData(callData), retVal(retVal), e(e)
    {
        callData->thisObject = QV4::Encode::undefined();
    }

    QV4::CallData *callData;
    QV4::Value *retVal;
    QV4::ExecutionEngine *e;
};

class QQmlError;
class QJSEngine;
class QQmlEngine;
class QQmlContextData;

namespace QV4 {
namespace Debugging {
class Debugger;
} // namespace Debugging
namespace Profiling {
class Profiler;
} // namespace Profiling
namespace CompiledData {
struct CompilationUnit;
}

namespace Heap {
struct Module;
};

struct Function;

namespace Promise {
class ReactionHandler;
};

struct Q_QML_EXPORT ExecutionEngine : public EngineBase
{
private:
    static qint32 maxCallDepth;

    friend struct ExecutionContextSaver;
    friend struct ExecutionContext;
    friend struct Heap::ExecutionContext;
public:
    ExecutableAllocator *executableAllocator;
    ExecutableAllocator *regExpAllocator;

    WTF::BumpPointerAllocator *bumperPointerAllocator; // Used by Yarr Regex engine.

    WTF::PageAllocation *jsStack;

    WTF::PageAllocation *gcStack;

    QML_NEARLY_ALWAYS_INLINE Value *jsAlloca(int nValues) {
        Value *ptr = jsStackTop;
        jsStackTop = ptr + nValues;
        return ptr;
    }

    Function *globalCode;

    QJSEngine *jsEngine() const { return publicEngine; }
    QQmlEngine *qmlEngine() const { return m_qmlEngine; }
    QJSEngine *publicEngine;

    enum JSObjects {
        RootContext,
        ScriptContext,
        IntegerNull, // Has to come after the RootContext to make the context stack safe
        ObjectProto,
        SymbolProto,
        ArrayProto,
        ArrayProtoValues,
        PropertyListProto,
        StringProto,
        NumberProto,
        BooleanProto,
        DateProto,
        FunctionProto,
        GeneratorProto,
        RegExpProto,
        ErrorProto,
        EvalErrorProto,
        RangeErrorProto,
        ReferenceErrorProto,
        SyntaxErrorProto,
        TypeErrorProto,
        URIErrorProto,
        PromiseProto,
        VariantProto,
#if QT_CONFIG(qml_sequence_object)
        SequenceProto,
#endif
        SharedArrayBufferProto,
        ArrayBufferProto,
        DataViewProto,
        WeakSetProto,
        SetProto,
        WeakMapProto,
        MapProto,
        IntrinsicTypedArrayProto,
        ValueTypeProto,
        SignalHandlerProto,
        IteratorProto,
        ForInIteratorProto,
        SetIteratorProto,
        MapIteratorProto,
        ArrayIteratorProto,
        StringIteratorProto,

        Object_Ctor,
        String_Ctor,
        Symbol_Ctor,
        Number_Ctor,
        Boolean_Ctor,
        Array_Ctor,
        Function_Ctor,
        GeneratorFunction_Ctor,
        Date_Ctor,
        RegExp_Ctor,
        Error_Ctor,
        EvalError_Ctor,
        RangeError_Ctor,
        ReferenceError_Ctor,
        SyntaxError_Ctor,
        TypeError_Ctor,
        URIError_Ctor,
        SharedArrayBuffer_Ctor,
        Promise_Ctor,
        ArrayBuffer_Ctor,
        DataView_Ctor,
        WeakSet_Ctor,
        Set_Ctor,
        WeakMap_Ctor,
        Map_Ctor,
        IntrinsicTypedArray_Ctor,

        GetSymbolSpecies,

        Eval_Function,
        GetStack_Function,
        ThrowerObject,
        NJSObjects
    };
    Value *jsObjects;
    enum { NTypedArrayTypes = 9 }; // == TypedArray::NValues, avoid header dependency

    ExecutionContext *rootContext() const { return reinterpret_cast<ExecutionContext *>(jsObjects + RootContext); }
    ExecutionContext *scriptContext() const { return reinterpret_cast<ExecutionContext *>(jsObjects + ScriptContext); }
    void setScriptContext(ReturnedValue c) { jsObjects[ScriptContext] = c; }
    FunctionObject *objectCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Object_Ctor); }
    FunctionObject *stringCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + String_Ctor); }
    FunctionObject *symbolCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Symbol_Ctor); }
    FunctionObject *numberCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Number_Ctor); }
    FunctionObject *booleanCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Boolean_Ctor); }
    FunctionObject *arrayCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Array_Ctor); }
    FunctionObject *functionCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Function_Ctor); }
    FunctionObject *generatorFunctionCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + GeneratorFunction_Ctor); }
    FunctionObject *dateCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Date_Ctor); }
    FunctionObject *regExpCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + RegExp_Ctor); }
    FunctionObject *errorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Error_Ctor); }
    FunctionObject *evalErrorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + EvalError_Ctor); }
    FunctionObject *rangeErrorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + RangeError_Ctor); }
    FunctionObject *referenceErrorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + ReferenceError_Ctor); }
    FunctionObject *syntaxErrorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + SyntaxError_Ctor); }
    FunctionObject *typeErrorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + TypeError_Ctor); }
    FunctionObject *uRIErrorCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + URIError_Ctor); }
    FunctionObject *sharedArrayBufferCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + SharedArrayBuffer_Ctor); }
    FunctionObject *promiseCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Promise_Ctor); }
    FunctionObject *arrayBufferCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + ArrayBuffer_Ctor); }
    FunctionObject *dataViewCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + DataView_Ctor); }
    FunctionObject *weakSetCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + WeakSet_Ctor); }
    FunctionObject *setCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Set_Ctor); }
    FunctionObject *weakMapCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + WeakMap_Ctor); }
    FunctionObject *mapCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + Map_Ctor); }
    FunctionObject *intrinsicTypedArrayCtor() const { return reinterpret_cast<FunctionObject *>(jsObjects + IntrinsicTypedArray_Ctor); }
    FunctionObject *typedArrayCtors;

    FunctionObject *getSymbolSpecies() const { return reinterpret_cast<FunctionObject *>(jsObjects + GetSymbolSpecies); }

    Object *objectPrototype() const { return reinterpret_cast<Object *>(jsObjects + ObjectProto); }
    Object *symbolPrototype() const { return reinterpret_cast<Object *>(jsObjects + SymbolProto); }
    Object *arrayPrototype() const { return reinterpret_cast<Object *>(jsObjects + ArrayProto); }
    Object *arrayProtoValues() const { return reinterpret_cast<Object *>(jsObjects + ArrayProtoValues); }
    Object *propertyListPrototype() const { return reinterpret_cast<Object *>(jsObjects + PropertyListProto); }
    Object *stringPrototype() const { return reinterpret_cast<Object *>(jsObjects + StringProto); }
    Object *numberPrototype() const { return reinterpret_cast<Object *>(jsObjects + NumberProto); }
    Object *booleanPrototype() const { return reinterpret_cast<Object *>(jsObjects + BooleanProto); }
    Object *datePrototype() const { return reinterpret_cast<Object *>(jsObjects + DateProto); }
    Object *functionPrototype() const { return reinterpret_cast<Object *>(jsObjects + FunctionProto); }
    Object *generatorPrototype() const { return reinterpret_cast<Object *>(jsObjects + GeneratorProto); }
    Object *regExpPrototype() const { return reinterpret_cast<Object *>(jsObjects + RegExpProto); }
    Object *errorPrototype() const { return reinterpret_cast<Object *>(jsObjects + ErrorProto); }
    Object *evalErrorPrototype() const { return reinterpret_cast<Object *>(jsObjects + EvalErrorProto); }
    Object *rangeErrorPrototype() const { return reinterpret_cast<Object *>(jsObjects + RangeErrorProto); }
    Object *referenceErrorPrototype() const { return reinterpret_cast<Object *>(jsObjects + ReferenceErrorProto); }
    Object *syntaxErrorPrototype() const { return reinterpret_cast<Object *>(jsObjects + SyntaxErrorProto); }
    Object *typeErrorPrototype() const { return reinterpret_cast<Object *>(jsObjects + TypeErrorProto); }
    Object *uRIErrorPrototype() const { return reinterpret_cast<Object *>(jsObjects + URIErrorProto); }
    Object *promisePrototype() const { return reinterpret_cast<Object *>(jsObjects + PromiseProto); }
    Object *variantPrototype() const { return reinterpret_cast<Object *>(jsObjects + VariantProto); }
#if QT_CONFIG(qml_sequence_object)
    Object *sequencePrototype() const { return reinterpret_cast<Object *>(jsObjects + SequenceProto); }
#endif

    Object *sharedArrayBufferPrototype() const { return reinterpret_cast<Object *>(jsObjects + SharedArrayBufferProto); }
    Object *arrayBufferPrototype() const { return reinterpret_cast<Object *>(jsObjects + ArrayBufferProto); }
    Object *dataViewPrototype() const { return reinterpret_cast<Object *>(jsObjects + DataViewProto); }
    Object *weakSetPrototype() const { return reinterpret_cast<Object *>(jsObjects + WeakSetProto); }
    Object *setPrototype() const { return reinterpret_cast<Object *>(jsObjects + SetProto); }
    Object *weakMapPrototype() const { return reinterpret_cast<Object *>(jsObjects + WeakMapProto); }
    Object *mapPrototype() const { return reinterpret_cast<Object *>(jsObjects + MapProto); }
    Object *intrinsicTypedArrayPrototype() const { return reinterpret_cast<Object *>(jsObjects + IntrinsicTypedArrayProto); }
    Object *typedArrayPrototype;

    Object *valueTypeWrapperPrototype() const { return reinterpret_cast<Object *>(jsObjects + ValueTypeProto); }
    Object *signalHandlerPrototype() const { return reinterpret_cast<Object *>(jsObjects + SignalHandlerProto); }
    Object *iteratorPrototype() const { return reinterpret_cast<Object *>(jsObjects + IteratorProto); }
    Object *forInIteratorPrototype() const { return reinterpret_cast<Object *>(jsObjects + ForInIteratorProto); }
    Object *setIteratorPrototype() const { return reinterpret_cast<Object *>(jsObjects + SetIteratorProto); }
    Object *mapIteratorPrototype() const { return reinterpret_cast<Object *>(jsObjects + MapIteratorProto); }
    Object *arrayIteratorPrototype() const { return reinterpret_cast<Object *>(jsObjects + ArrayIteratorProto); }
    Object *stringIteratorPrototype() const { return reinterpret_cast<Object *>(jsObjects + StringIteratorProto); }

    EvalFunction *evalFunction() const { return reinterpret_cast<EvalFunction *>(jsObjects + Eval_Function); }
    FunctionObject *getStackFunction() const { return reinterpret_cast<FunctionObject *>(jsObjects + GetStack_Function); }
    FunctionObject *thrower() const { return reinterpret_cast<FunctionObject *>(jsObjects + ThrowerObject); }

#if QT_CONFIG(qml_network)
    QNetworkAccessManager* (*networkAccessManager)(ExecutionEngine*)  = detail::getNetworkAccessManager;
#endif

    enum JSStrings {
        String_Empty,
        String_undefined,
        String_null,
        String_true,
        String_false,
        String_boolean,
        String_number,
        String_string,
        String_default,
        String_symbol,
        String_object,
        String_function,
        String_length,
        String_prototype,
        String_constructor,
        String_arguments,
        String_caller,
        String_callee,
        String_this,
        String___proto__,
        String_enumerable,
        String_configurable,
        String_writable,
        String_value,
        String_get,
        String_set,
        String_eval,
        String_uintMax,
        String_name,
        String_index,
        String_input,
        String_toString,
        String_toLocaleString,
        String_destroy,
        String_valueOf,
        String_byteLength,
        String_byteOffset,
        String_buffer,
        String_lastIndex,
        String_next,
        String_done,
        String_return,
        String_throw,
        String_global,
        String_ignoreCase,
        String_multiline,
        String_unicode,
        String_sticky,
        String_source,
        String_flags,

        NJSStrings
    };
    Value *jsStrings;

    enum JSSymbols {
        Symbol_hasInstance,
        Symbol_isConcatSpreadable,
        Symbol_iterator,
        Symbol_match,
        Symbol_replace,
        Symbol_search,
        Symbol_species,
        Symbol_split,
        Symbol_toPrimitive,
        Symbol_toStringTag,
        Symbol_unscopables,
        Symbol_revokableProxy,
        NJSSymbols
    };
    Value *jsSymbols;

    String *id_empty() const { return reinterpret_cast<String *>(jsStrings + String_Empty); }
    String *id_undefined() const { return reinterpret_cast<String *>(jsStrings + String_undefined); }
    String *id_null() const { return reinterpret_cast<String *>(jsStrings + String_null); }
    String *id_true() const { return reinterpret_cast<String *>(jsStrings + String_true); }
    String *id_false() const { return reinterpret_cast<String *>(jsStrings + String_false); }
    String *id_boolean() const { return reinterpret_cast<String *>(jsStrings + String_boolean); }
    String *id_number() const { return reinterpret_cast<String *>(jsStrings + String_number); }
    String *id_string() const { return reinterpret_cast<String *>(jsStrings + String_string); }
    String *id_default() const { return reinterpret_cast<String *>(jsStrings + String_default); }
    String *id_symbol() const { return reinterpret_cast<String *>(jsStrings + String_symbol); }
    String *id_object() const { return reinterpret_cast<String *>(jsStrings + String_object); }
    String *id_function() const { return reinterpret_cast<String *>(jsStrings + String_function); }
    String *id_length() const { return reinterpret_cast<String *>(jsStrings + String_length); }
    String *id_prototype() const { return reinterpret_cast<String *>(jsStrings + String_prototype); }
    String *id_constructor() const { return reinterpret_cast<String *>(jsStrings + String_constructor); }
    String *id_arguments() const { return reinterpret_cast<String *>(jsStrings + String_arguments); }
    String *id_caller() const { return reinterpret_cast<String *>(jsStrings + String_caller); }
    String *id_callee() const { return reinterpret_cast<String *>(jsStrings + String_callee); }
    String *id_this() const { return reinterpret_cast<String *>(jsStrings + String_this); }
    String *id___proto__() const { return reinterpret_cast<String *>(jsStrings + String___proto__); }
    String *id_enumerable() const { return reinterpret_cast<String *>(jsStrings + String_enumerable); }
    String *id_configurable() const { return reinterpret_cast<String *>(jsStrings + String_configurable); }
    String *id_writable() const { return reinterpret_cast<String *>(jsStrings + String_writable); }
    String *id_value() const { return reinterpret_cast<String *>(jsStrings + String_value); }
    String *id_get() const { return reinterpret_cast<String *>(jsStrings + String_get); }
    String *id_set() const { return reinterpret_cast<String *>(jsStrings + String_set); }
    String *id_eval() const { return reinterpret_cast<String *>(jsStrings + String_eval); }
    String *id_uintMax() const { return reinterpret_cast<String *>(jsStrings + String_uintMax); }
    String *id_name() const { return reinterpret_cast<String *>(jsStrings + String_name); }
    String *id_index() const { return reinterpret_cast<String *>(jsStrings + String_index); }
    String *id_input() const { return reinterpret_cast<String *>(jsStrings + String_input); }
    String *id_toString() const { return reinterpret_cast<String *>(jsStrings + String_toString); }
    String *id_toLocaleString() const { return reinterpret_cast<String *>(jsStrings + String_toLocaleString); }
    String *id_destroy() const { return reinterpret_cast<String *>(jsStrings + String_destroy); }
    String *id_valueOf() const { return reinterpret_cast<String *>(jsStrings + String_valueOf); }
    String *id_byteLength() const { return reinterpret_cast<String *>(jsStrings + String_byteLength); }
    String *id_byteOffset() const { return reinterpret_cast<String *>(jsStrings + String_byteOffset); }
    String *id_buffer() const { return reinterpret_cast<String *>(jsStrings + String_buffer); }
    String *id_lastIndex() const { return reinterpret_cast<String *>(jsStrings + String_lastIndex); }
    String *id_next() const { return reinterpret_cast<String *>(jsStrings + String_next); }
    String *id_done() const { return reinterpret_cast<String *>(jsStrings + String_done); }
    String *id_return() const { return reinterpret_cast<String *>(jsStrings + String_return); }
    String *id_throw() const { return reinterpret_cast<String *>(jsStrings + String_throw); }
    String *id_global() const { return reinterpret_cast<String *>(jsStrings + String_global); }
    String *id_ignoreCase() const { return reinterpret_cast<String *>(jsStrings + String_ignoreCase); }
    String *id_multiline() const { return reinterpret_cast<String *>(jsStrings + String_multiline); }
    String *id_unicode() const { return reinterpret_cast<String *>(jsStrings + String_unicode); }
    String *id_sticky() const { return reinterpret_cast<String *>(jsStrings + String_sticky); }
    String *id_source() const { return reinterpret_cast<String *>(jsStrings + String_source); }
    String *id_flags() const { return reinterpret_cast<String *>(jsStrings + String_flags); }

    Symbol *symbol_hasInstance() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_hasInstance); }
    Symbol *symbol_isConcatSpreadable() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_isConcatSpreadable); }
    Symbol *symbol_iterator() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_iterator); }
    Symbol *symbol_match() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_match); }
    Symbol *symbol_replace() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_replace); }
    Symbol *symbol_search() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_search); }
    Symbol *symbol_species() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_species); }
    Symbol *symbol_split() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_split); }
    Symbol *symbol_toPrimitive() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_toPrimitive); }
    Symbol *symbol_toStringTag() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_toStringTag); }
    Symbol *symbol_unscopables() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_unscopables); }
    Symbol *symbol_revokableProxy() const { return reinterpret_cast<Symbol *>(jsSymbols + Symbol_revokableProxy); }

    QIntrusiveList<ExecutableCompilationUnit, &ExecutableCompilationUnit::nextCompilationUnit> compilationUnits;

    quint32 m_engineId;

    RegExpCache *regExpCache;

    // Scarce resources are "exceptionally high cost" QVariant types where allowing the
    // normal JavaScript GC to clean them up is likely to lead to out-of-memory or other
    // out-of-resource situations.  When such a resource is passed into JavaScript we
    // add it to the scarceResources list and it is destroyed when we return from the
    // JavaScript execution that created it.  The user can prevent this behavior by
    // calling preserve() on the object which removes it from this scarceResource list.
    class ScarceResourceData {
    public:
        ScarceResourceData(const QVariant &data = QVariant()) : data(data) {}
        QVariant data;
        QIntrusiveListNode node;
    };
    QIntrusiveList<ScarceResourceData, &ScarceResourceData::node> scarceResources;

    // Normally the JS wrappers for QObjects are stored in the QQmlData/QObjectPrivate,
    // but any time a QObject is wrapped a second time in another engine, we have to do
    // bookkeeping.
    MultiplyWrappedQObjectMap *m_multiplyWrappedQObjects;
#if QT_CONFIG(qml_jit)
    const bool m_canAllocateExecutableMemory;
#endif

    quintptr protoIdCount = 1;

    ExecutionEngine(QJSEngine *jsEngine = nullptr);
    ~ExecutionEngine();

#if !QT_CONFIG(qml_debug)
    QV4::Debugging::Debugger *debugger() const { return nullptr; }
    QV4::Profiling::Profiler *profiler() const { return nullptr; }

    void setDebugger(Debugging::Debugger *) {}
    void setProfiler(Profiling::Profiler *) {}
#else
    QV4::Debugging::Debugger *debugger() const { return m_debugger.data(); }
    QV4::Profiling::Profiler *profiler() const { return m_profiler.data(); }

    void setDebugger(Debugging::Debugger *debugger);
    void setProfiler(Profiling::Profiler *profiler);
#endif // QT_CONFIG(qml_debug)

    ExecutionContext *currentContext() const;

    // ensure we always get odd prototype IDs. This helps make marking in QV4::Lookup fast
    quintptr newProtoId() { return (protoIdCount += 2); }

    Heap::InternalClass *newInternalClass(const VTable *vtable, Object *prototype);

    Heap::Object *newObject();
    Heap::Object *newObject(Heap::InternalClass *internalClass);

    Heap::String *newString(const QString &s = QString());
    Heap::String *newIdentifier(const QString &text);

    Heap::Object *newStringObject(const String *string);
    Heap::Object *newSymbolObject(const Symbol *symbol);
    Heap::Object *newNumberObject(double value);
    Heap::Object *newBooleanObject(bool b);

    Heap::ArrayObject *newArrayObject(int count = 0);
    Heap::ArrayObject *newArrayObject(const Value *values, int length);
    Heap::ArrayObject *newArrayObject(const QStringList &list);
    Heap::ArrayObject *newArrayObject(Heap::InternalClass *ic);

    Heap::ArrayBuffer *newArrayBuffer(const QByteArray &array);
    Heap::ArrayBuffer *newArrayBuffer(size_t length);

    Heap::DateObject *newDateObject(const Value &value);
    Heap::DateObject *newDateObject(const QDateTime &dt);
    Heap::DateObject *newDateObjectFromTime(const QTime &t);

    Heap::RegExpObject *newRegExpObject(const QString &pattern, int flags);
    Heap::RegExpObject *newRegExpObject(RegExp *re);
    Heap::RegExpObject *newRegExpObject(const QRegExp &re);
#if QT_CONFIG(regularexpression)
    Heap::RegExpObject *newRegExpObject(const QRegularExpression &re);
#endif

    Heap::Object *newErrorObject(const Value &value);
    Heap::Object *newErrorObject(const QString &message);
    Heap::Object *newSyntaxErrorObject(const QString &message, const QString &fileName, int line, int column);
    Heap::Object *newSyntaxErrorObject(const QString &message);
    Heap::Object *newReferenceErrorObject(const QString &message);
    Heap::Object *newReferenceErrorObject(const QString &message, const QString &fileName, int line, int column);
    Heap::Object *newTypeErrorObject(const QString &message);
    Heap::Object *newRangeErrorObject(const QString &message);
    Heap::Object *newURIErrorObject(const QString &message);
    Heap::Object *newURIErrorObject(const Value &message);
    Heap::Object *newEvalErrorObject(const QString &message);

    Heap::PromiseObject *newPromiseObject();
    Heap::Object *newPromiseObject(const QV4::FunctionObject *thisObject, const QV4::PromiseCapability *capability);
    Promise::ReactionHandler *getPromiseReactionHandler();

    Heap::Object *newVariantObject(const QVariant &v);

    Heap::Object *newForInIteratorObject(Object *o);
    Heap::Object *newSetIteratorObject(Object *o);
    Heap::Object *newMapIteratorObject(Object *o);
    Heap::Object *newArrayIteratorObject(Object *o);

    Heap::QmlContext *qmlContext() const;
    QObject *qmlScopeObject() const;
    QQmlContextData *callingQmlContext() const;


    StackTrace stackTrace(int frameLimit = -1) const;
    QUrl resolvedUrl(const QString &file);

    void markObjects(MarkStack *markStack);

    void initRootContext();

    Heap::InternalClass *newClass(Heap::InternalClass *other);

    StackTrace exceptionStackTrace;

    ReturnedValue throwError(const Value &value);
    ReturnedValue catchException(StackTrace *trace = nullptr);

    ReturnedValue throwError(const QString &message);
    ReturnedValue throwSyntaxError(const QString &message);
    ReturnedValue throwSyntaxError(const QString &message, const QString &fileName, int lineNumber, int column);
    ReturnedValue throwTypeError();
    ReturnedValue throwTypeError(const QString &message);
    ReturnedValue throwReferenceError(const Value &value);
    ReturnedValue throwReferenceError(const QString &name);
    ReturnedValue throwReferenceError(const QString &value, const QString &fileName, int lineNumber, int column);
    ReturnedValue throwRangeError(const Value &value);
    ReturnedValue throwRangeError(const QString &message);
    ReturnedValue throwURIError(const Value &msg);
    ReturnedValue throwUnimplemented(const QString &message);

    // Use only inside catch(...) -- will re-throw if no JS exception
    QQmlError catchExceptionAsQmlError();

    // variant conversions
    QVariant toVariant(const QV4::Value &value, int typeHint, bool createJSValueForObjects = true);
    QV4::ReturnedValue fromVariant(const QVariant &);

    QVariantMap variantMapFromJS(const QV4::Object *o);

    bool metaTypeFromJS(const Value *value, int type, void *data);
    QV4::ReturnedValue metaTypeToJS(int type, const void *data);

    int maxJSStackSize() const;
    int maxGCStackSize() const;

    bool checkStackLimits();

    bool canJIT(Function *f = nullptr)
    {
#if QT_CONFIG(qml_jit)
        if (!m_canAllocateExecutableMemory)
            return false;
        if (f)
            return !f->isGenerator() && f->interpreterCallCount >= jitCallCountThreshold;
        return true;
#else
        Q_UNUSED(f);
        return false;
#endif
    }

    QV4::ReturnedValue global();
    void initQmlGlobalObject();
    void initializeGlobal();

    void freezeObject(const QV4::Value &value);

    // Return the list of illegal id names (the names of the properties on the global object)
    const QSet<QString> &illegalNames() const;

#if QT_CONFIG(qml_xml_http_request)
    void *xmlHttpRequestData() const { return m_xmlHttpRequestData; }
#endif

    void setQmlEngine(QQmlEngine *engine);

    QQmlDelayedCallQueue *delayedCallQueue() { return &m_delayedCallQueue; }

    // used for console.time(), console.timeEnd()
    void startTimer(const QString &timerName);
    qint64 stopTimer(const QString &timerName, bool *wasRunning);

    // used for console.count()
    int consoleCountHelper(const QString &file, quint16 line, quint16 column);

    struct Deletable {
        virtual ~Deletable() {}
    };

    static QMutex *registrationMutex();
    static int registerExtension();

    void setExtensionData(int, Deletable *);
    Deletable *extensionData(int index) const
    {
        if (index < m_extensionData.count())
            return m_extensionData[index];
        else
            return nullptr;
    }

    double localTZA = 0.0; // local timezone, initialized at startup

    QQmlRefPointer<ExecutableCompilationUnit> compileModule(const QUrl &url);
    QQmlRefPointer<ExecutableCompilationUnit> compileModule(
            const QUrl &url, const QString &sourceCode, const QDateTime &sourceTimeStamp);

    mutable QMutex moduleMutex;
    QHash<QUrl, QQmlRefPointer<ExecutableCompilationUnit>> modules;
    void injectModule(const QQmlRefPointer<ExecutableCompilationUnit> &moduleUnit);
    QQmlRefPointer<ExecutableCompilationUnit> moduleForUrl(const QUrl &_url, const ExecutableCompilationUnit *referrer = nullptr) const;
    QQmlRefPointer<ExecutableCompilationUnit> loadModule(const QUrl &_url, const ExecutableCompilationUnit *referrer = nullptr);

private:
#if QT_CONFIG(qml_debug)
    QScopedPointer<QV4::Debugging::Debugger> m_debugger;
    QScopedPointer<QV4::Profiling::Profiler> m_profiler;
#endif
    QSet<QString> m_illegalNames;
    int jitCallCountThreshold;

    // used by generated Promise objects to handle 'then' events
    QScopedPointer<QV4::Promise::ReactionHandler> m_reactionHandler;

#if QT_CONFIG(qml_xml_http_request)
    void *m_xmlHttpRequestData;
#endif

    QQmlEngine *m_qmlEngine;

    QQmlDelayedCallQueue m_delayedCallQueue;

    QElapsedTimer m_time;
    QHash<QString, qint64> m_startedTimers;

    QHash<QString, quint32> m_consoleCount;

    QVector<Deletable *> m_extensionData;

    int m_maxJSStackSize = 4 * 1024 * 1024;
    int m_maxGCStackSize = 2 * 1024 * 1024;
};

#define CHECK_STACK_LIMITS(v4) if ((v4)->checkStackLimits()) return Encode::undefined(); \
    ExecutionEngineCallDepthRecorder _executionEngineCallDepthRecorder(v4);

struct ExecutionEngineCallDepthRecorder
{
    ExecutionEngine *ee;

    ExecutionEngineCallDepthRecorder(ExecutionEngine *e): ee(e) { ++ee->callDepth; }
    ~ExecutionEngineCallDepthRecorder() { --ee->callDepth; }
};

inline bool ExecutionEngine::checkStackLimits()
{
    if (Q_UNLIKELY((jsStackTop > jsStackLimit) || (callDepth >= maxCallDepth))) {
        throwRangeError(QStringLiteral("Maximum call stack size exceeded."));
        return true;
    }

    return false;
}

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ENGINE_H
