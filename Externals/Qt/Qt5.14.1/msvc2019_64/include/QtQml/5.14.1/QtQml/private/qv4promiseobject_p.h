/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#ifndef QV4PROMISEOBJECT_H
#define QV4PROMISEOBJECT_H

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

QT_BEGIN_NAMESPACE

namespace QV4 {

struct PromiseCapability;

namespace Promise {

struct ReactionEvent;
struct ResolveThenableEvent;

class ReactionHandler : public QObject
{
    Q_OBJECT

public:
    ReactionHandler(QObject *parent = nullptr);
    virtual ~ReactionHandler() override;

    void addReaction(ExecutionEngine *e, const Value *reaction, const Value *value);
    void addResolveThenable(ExecutionEngine *e, const PromiseObject *promise, const Object *thenable, const FunctionObject *then);

protected:
    void customEvent(QEvent *event) override;
    void executeReaction(ReactionEvent *event);
    void executeResolveThenable(ResolveThenableEvent *event);
};

} // Promise

namespace Heap {

struct PromiseCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

#define PromiseObjectMembers(class, Member) \
    Member(class, HeapValue, HeapValue, resolution) \
    Member(class, HeapValue, HeapValue, fulfillReactions) \
    Member(class, HeapValue, HeapValue, rejectReactions)

DECLARE_HEAP_OBJECT(PromiseObject, Object) {
    DECLARE_MARKOBJECTS(PromiseObject)
    void init(ExecutionEngine *e);

    enum State {
        Pending,
        Fulfilled,
        Rejected
    };

    void setState(State);
    bool isSettled() const;
    bool isPending() const;
    bool isFulfilled() const;
    bool isRejected() const;

    State state;

    void triggerFullfillReactions(ExecutionEngine *e);
    void triggerRejectReactions(ExecutionEngine *e);
};

#define PromiseCapabilityMembers(class, Member) \
    Member(class, HeapValue, HeapValue, promise) \
    Member(class, HeapValue, HeapValue, resolve) \
    Member(class, HeapValue, HeapValue, reject)

DECLARE_HEAP_OBJECT(PromiseCapability, Object) {
    DECLARE_MARKOBJECTS(PromiseCapability)
};

#define PromiseReactionMembers(class, Member) \
    Member(class, HeapValue, HeapValue, handler) \
    Member(class, Pointer, PromiseCapability*, capability)

DECLARE_HEAP_OBJECT(PromiseReaction, Object) {
    DECLARE_MARKOBJECTS(PromiseReaction)

    static Heap::PromiseReaction *createFulfillReaction(ExecutionEngine* e, const QV4::PromiseCapability *capability, const QV4::FunctionObject *onFulfilled);
    static Heap::PromiseReaction *createRejectReaction(ExecutionEngine* e, const QV4::PromiseCapability *capability, const QV4::FunctionObject *onRejected);

    void triggerWithValue(ExecutionEngine *e, const Value *value);

    enum Type {
        Function,
        Identity,
        Thrower
    };

    Type type;

    friend class ReactionHandler;
};

#define CapabilitiesExecutorWrapperMembers(class, Member) \
    Member(class, Pointer, PromiseCapability*, capabilities)

DECLARE_HEAP_OBJECT(CapabilitiesExecutorWrapper, FunctionObject) {
    DECLARE_MARKOBJECTS(CapabilitiesExecutorWrapper)
    void init();
    void destroy();
};

#define PromiseExecutionStateMembers(class, Member) \
    Member(class, HeapValue, HeapValue, values) \
    Member(class, HeapValue, HeapValue, capability)

DECLARE_HEAP_OBJECT(PromiseExecutionState, FunctionObject) {
    DECLARE_MARKOBJECTS(PromiseExecutionState)
    void init();

    uint index;
    uint remainingElementCount;
};

#define ResolveElementWrapperMembers(class, Member) \
    Member(class, HeapValue, HeapValue, state)

DECLARE_HEAP_OBJECT(ResolveElementWrapper, FunctionObject) {
    DECLARE_MARKOBJECTS(ResolveElementWrapper)
    void init();

    uint index;
    bool alreadyResolved;
};

#define ResolveWrapperMembers(class, Member) \
    Member(class, Pointer, PromiseObject*, promise)

DECLARE_HEAP_OBJECT(ResolveWrapper, FunctionObject) {
    DECLARE_MARKOBJECTS(ResolveWrapper)
    void init();

    bool alreadyResolved;
};

#define RejectWrapperMembers(class, Member) \
    Member(class, Pointer, PromiseObject*, promise)

DECLARE_HEAP_OBJECT(RejectWrapper, FunctionObject) {
    DECLARE_MARKOBJECTS(RejectWrapper)
    void init();

    bool alreadyResolved;
};

} // Heap

struct PromiseReaction : Object
{
    V4_OBJECT2(PromiseReaction, Object)
};

struct PromiseCapability : Object
{
    V4_OBJECT2(PromiseCapability, Object)
};

struct PromiseExecutionState : Object
{
    V4_OBJECT2(PromiseExecutionState, Object)
};

struct Q_QML_PRIVATE_EXPORT PromiseObject : Object
{
    V4_OBJECT2(PromiseObject, Object)
    V4_NEEDS_DESTROY
    V4_PROTOTYPE(promisePrototype)
};

struct PromiseCtor: FunctionObject
{
    V4_OBJECT2(PromiseCtor, FunctionObject)

    static ReturnedValue virtualCallAsConstructor(const FunctionObject *f, const Value *argv, int argc, const Value *);
    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_resolve(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_reject(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_all(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_race(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct PromisePrototype : Object
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_then(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_catch(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct CapabilitiesExecutorWrapper: FunctionObject {
    V4_OBJECT2(CapabilitiesExecutorWrapper, FunctionObject)

    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct ResolveElementWrapper : FunctionObject {
    V4_OBJECT2(ResolveElementWrapper, FunctionObject)

    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct ResolveWrapper : FunctionObject {
    V4_OBJECT2(ResolveWrapper, FunctionObject)

    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

struct RejectWrapper : FunctionObject {
    V4_OBJECT2(RejectWrapper, FunctionObject)

    static ReturnedValue virtualCall(const FunctionObject *f, const Value *thisObject, const Value *argv, int argc);
};

} // QV4

QT_END_NAMESPACE

#endif // QV4PROMISEOBJECT_H
