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
#ifndef QV4ERROROBJECT_H
#define QV4ERROROBJECT_H

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

#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4string_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct SyntaxErrorObject;

namespace Heap {


#define ErrorObjectMembers(class, Member) \
    Member(class, Pointer, String *, stack)

DECLARE_HEAP_OBJECT(ErrorObject, Object) {
    DECLARE_MARKOBJECTS(ErrorObject);
    enum ErrorType {
        Error,
        EvalError,
        RangeError,
        ReferenceError,
        SyntaxError,
        TypeError,
        URIError
    };
    StackTrace *stackTrace;
    ErrorType errorType;

    void init();
    void init(const Value &message, ErrorType t = Error);
    void init(const Value &message, const QString &fileName, int line, int column, ErrorType t = Error);
    void destroy() {
        delete stackTrace;
        Object::destroy();
    }
};

struct EvalErrorObject : ErrorObject {
    void init(const Value &message);
};

struct RangeErrorObject : ErrorObject {
    void init(const Value &message);
};

struct ReferenceErrorObject : ErrorObject {
    void init(const Value &message);
    void init(const Value &msg, const QString &fileName, int lineNumber, int columnNumber);
};

struct SyntaxErrorObject : ErrorObject {
    void init(const Value &message);
    void init(const Value &msg, const QString &fileName, int lineNumber, int columnNumber);
};

struct TypeErrorObject : ErrorObject {
    void init(const Value &message);
};

struct URIErrorObject : ErrorObject {
    void init(const Value &message);
};

struct ErrorCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
    void init(QV4::ExecutionContext *scope, const QString &name);
};

struct EvalErrorCtor : ErrorCtor {
    void init(QV4::ExecutionContext *scope);
};

struct RangeErrorCtor : ErrorCtor {
    void init(QV4::ExecutionContext *scope);
};

struct ReferenceErrorCtor : ErrorCtor {
    void init(QV4::ExecutionContext *scope);
};

struct SyntaxErrorCtor : ErrorCtor {
    void init(QV4::ExecutionContext *scope);
};

struct TypeErrorCtor : ErrorCtor {
    void init(QV4::ExecutionContext *scope);
};

struct URIErrorCtor : ErrorCtor {
    void init(QV4::ExecutionContext *scope);
};

}

struct ErrorObject: Object {
    enum {
        IsErrorObject = true
    };

    enum {
        Index_Stack = 0, // Accessor Property
        Index_StackSetter = 1, // Accessor Property
        Index_FileName = 2,
        Index_LineNumber = 3,
        Index_Message = 4
    };

    V4_OBJECT2(ErrorObject, Object)
    Q_MANAGED_TYPE(ErrorObject)
    V4_INTERNALCLASS(ErrorObject)
    V4_PROTOTYPE(errorPrototype)
    V4_NEEDS_DESTROY

    template <typename T>
    static Heap::Object *create(ExecutionEngine *e, const Value &message, const Value *newTarget);
    template <typename T>
    static Heap::Object *create(ExecutionEngine *e, const QString &message);
    template <typename T>
    static Heap::Object *create(ExecutionEngine *e, const QString &message, const QString &filename, int line, int column);

    SyntaxErrorObject *asSyntaxError();

    static const char *className(Heap::ErrorObject::ErrorType t);

    static ReturnedValue method_get_stack(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

template<>
inline const ErrorObject *Value::as() const {
    return isManaged() && m()->internalClass->vtable->isErrorObject ? reinterpret_cast<const ErrorObject *>(this) : nullptr;
}

struct EvalErrorObject: ErrorObject {
    typedef Heap::EvalErrorObject Data;
    V4_PROTOTYPE(evalErrorPrototype)
    const Data *d() const { return static_cast<const Data *>(ErrorObject::d()); }
    Data *d() { return static_cast<Data *>(ErrorObject::d()); }
};

struct RangeErrorObject: ErrorObject {
    typedef Heap::RangeErrorObject Data;
    V4_PROTOTYPE(rangeErrorPrototype)
    const Data *d() const { return static_cast<const Data *>(ErrorObject::d()); }
    Data *d() { return static_cast<Data *>(ErrorObject::d()); }
};

struct ReferenceErrorObject: ErrorObject {
    typedef Heap::ReferenceErrorObject Data;
    V4_PROTOTYPE(referenceErrorPrototype)
    const Data *d() const { return static_cast<const Data *>(ErrorObject::d()); }
    Data *d() { return static_cast<Data *>(ErrorObject::d()); }
};

struct SyntaxErrorObject: ErrorObject {
    typedef Heap::SyntaxErrorObject Data;
    V4_PROTOTYPE(syntaxErrorPrototype)
    const Data *d() const { return static_cast<const Data *>(ErrorObject::d()); }
    Data *d() { return static_cast<Data *>(ErrorObject::d()); }
};

struct TypeErrorObject: ErrorObject {
    typedef Heap::TypeErrorObject Data;
    V4_PROTOTYPE(typeErrorPrototype)
    const Data *d() const { return static_cast<const Data *>(ErrorObject::d()); }
    Data *d() { return static_cast<Data *>(ErrorObject::d()); }
};

struct URIErrorObject: ErrorObject {
    typedef Heap::URIErrorObject Data;
    V4_PROTOTYPE(uRIErrorPrototype)
    const Data *d() const { return static_cast<const Data *>(ErrorObject::d()); }
    Data *d() { return static_cast<Data *>(ErrorObject::d()); }
};

struct ErrorCtor: FunctionObject
{
    V4_OBJECT2(ErrorCtor, FunctionObject)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct EvalErrorCtor: ErrorCtor
{
    V4_OBJECT2(EvalErrorCtor, FunctionObject)
    V4_PROTOTYPE(errorCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};

struct RangeErrorCtor: ErrorCtor
{
    V4_OBJECT2(RangeErrorCtor, FunctionObject)
    V4_PROTOTYPE(errorCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};

struct ReferenceErrorCtor: ErrorCtor
{
    V4_OBJECT2(ReferenceErrorCtor, FunctionObject)
    V4_PROTOTYPE(errorCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};

struct SyntaxErrorCtor: ErrorCtor
{
    V4_OBJECT2(SyntaxErrorCtor, FunctionObject)
    V4_PROTOTYPE(errorCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};

struct TypeErrorCtor: ErrorCtor
{
    V4_OBJECT2(TypeErrorCtor, FunctionObject)
    V4_PROTOTYPE(errorCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};

struct URIErrorCtor: ErrorCtor
{
    V4_OBJECT2(URIErrorCtor, FunctionObject)
    V4_PROTOTYPE(errorCtor)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
};


struct ErrorPrototype : Object
{
    enum {
        Index_Constructor = 0,
        Index_Message = 1,
        Index_Name = 2
    };
    void init(ExecutionEngine *engine, Object *ctor) { init(engine, ctor, this, Heap::ErrorObject::Error); }

    static void init(ExecutionEngine *engine, Object *ctor, Object *obj, Heap::ErrorObject::ErrorType t);
    static ReturnedValue method_toString(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
};

struct EvalErrorPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this, Heap::ErrorObject::EvalError); }
};

struct RangeErrorPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this, Heap::ErrorObject::RangeError); }
};

struct ReferenceErrorPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this, Heap::ErrorObject::ReferenceError); }
};

struct SyntaxErrorPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this, Heap::ErrorObject::SyntaxError); }
};

struct TypeErrorPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this, Heap::ErrorObject::TypeError); }
};

struct URIErrorPrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this, Heap::ErrorObject::URIError); }
};


inline SyntaxErrorObject *ErrorObject::asSyntaxError()
{
    return d()->errorType == QV4::Heap::ErrorObject::SyntaxError ? static_cast<SyntaxErrorObject *>(this) : nullptr;
}


template <typename T>
Heap::Object *ErrorObject::create(ExecutionEngine *e, const Value &message, const Value *newTarget) {
    EngineBase::InternalClassType klass = message.isUndefined() ? EngineBase::Class_ErrorObject : EngineBase::Class_ErrorObjectWithMessage;
    Scope scope(e);
    ScopedObject proto(scope, static_cast<const Object *>(newTarget)->get(scope.engine->id_prototype()));
    Scoped<InternalClass> ic(scope, e->internalClasses(klass)->changePrototype(proto->d()));
    return e->memoryManager->allocObject<T>(ic->d(), message);
}
template <typename T>
Heap::Object *ErrorObject::create(ExecutionEngine *e, const QString &message) {
    Scope scope(e);
    ScopedValue v(scope, message.isEmpty() ? Encode::undefined() : e->newString(message)->asReturnedValue());
    EngineBase::InternalClassType klass = v->isUndefined() ? EngineBase::Class_ErrorObject : EngineBase::Class_ErrorObjectWithMessage;
    Scoped<InternalClass> ic(scope, e->internalClasses(klass)->changePrototype(T::defaultPrototype(e)->d()));
    return e->memoryManager->allocObject<T>(ic->d(), v);
}
template <typename T>
Heap::Object *ErrorObject::create(ExecutionEngine *e, const QString &message, const QString &filename, int line, int column) {
    Scope scope(e);
    ScopedValue v(scope, message.isEmpty() ? Encode::undefined() : e->newString(message)->asReturnedValue());
    EngineBase::InternalClassType klass = v->isUndefined() ? EngineBase::Class_ErrorObject : EngineBase::Class_ErrorObjectWithMessage;
    Scoped<InternalClass> ic(scope, e->internalClasses(klass)->changePrototype(T::defaultPrototype(e)->d()));
    return e->memoryManager->allocObject<T>(ic->d(), v, filename, line, column);
}


}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
