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
#ifndef QV4STACKFRAME_H
#define QV4STACKFRAME_H

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

#include <private/qv4context_p.h>
#include <private/qv4enginebase_p.h>
#include <private/qv4calldata_p.h>
#include <private/qv4function_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_EXPORT CppStackFrame {
    EngineBase *engine;
    Value *savedStackTop;
    CppStackFrame *parent;
    Function *v4Function;
    CallData *jsFrame;
    const Value *originalArguments;
    int originalArgumentsCount;
    int instructionPointer;
    const char *yield;
    const char *unwindHandler;
    const char *unwindLabel;
    int unwindLevel;
    bool yieldIsIterator;
    bool callerCanHandleTailCall;
    bool pendingTailCall;
    bool isTailCalling;

    void init(EngineBase *engine, Function *v4Function, const Value *argv, int argc, bool callerCanHandleTailCall = false) {
        this->engine = engine;

        this->v4Function = v4Function;
        originalArguments = argv;
        originalArgumentsCount = argc;
        instructionPointer = 0;
        yield = nullptr;
        unwindHandler = nullptr;
        unwindLabel = nullptr;
        unwindLevel = 0;
        yieldIsIterator = false;
        this->callerCanHandleTailCall = callerCanHandleTailCall;
        pendingTailCall = false;
        isTailCalling = false;
    }

    void push() {
        parent = engine->currentStackFrame;
        engine->currentStackFrame = this;
        savedStackTop = engine->jsStackTop;
    }

    void pop() {
        engine->currentStackFrame = parent;
        engine->jsStackTop = savedStackTop;
    }

    static uint requiredJSStackFrameSize(uint nRegisters) {
        return CallData::HeaderSize() + nRegisters;
    }
    static uint requiredJSStackFrameSize(Function *v4Function) {
        return CallData::HeaderSize() + v4Function->compiledFunction->nRegisters;
    }
    uint requiredJSStackFrameSize() const {
        return requiredJSStackFrameSize(v4Function);
    }
    void setupJSFrame(Value *stackSpace, const Value &function, const Heap::ExecutionContext *scope,
                      const Value &thisObject, const Value &newTarget = Value::undefinedValue()) {
        setupJSFrame(stackSpace, function, scope, thisObject, newTarget,
                     v4Function->compiledFunction->nFormals, v4Function->compiledFunction->nRegisters);
    }
    void setupJSFrame(Value *stackSpace, const Value &function, const Heap::ExecutionContext *scope,
                      const Value &thisObject, const Value &newTarget, uint nFormals, uint nRegisters)
    {
        jsFrame = reinterpret_cast<CallData *>(stackSpace);
        jsFrame->function = function;
        jsFrame->context = scope->asReturnedValue();
        jsFrame->accumulator = Encode::undefined();
        jsFrame->thisObject = thisObject;
        jsFrame->newTarget = newTarget;

        uint argc = uint(originalArgumentsCount);
        if (argc > nFormals)
            argc = nFormals;
        jsFrame->setArgc(argc);

        memcpy(jsFrame->args, originalArguments, argc*sizeof(Value));
        Q_STATIC_ASSERT(Encode::undefined() == 0);
        memset(jsFrame->args + argc, 0, (nRegisters - argc)*sizeof(Value));

        if (v4Function && v4Function->compiledFunction) {
            const int firstDeadZoneRegister = v4Function->compiledFunction->firstTemporalDeadZoneRegister;
            const int registerDeadZoneSize = v4Function->compiledFunction->sizeOfRegisterTemporalDeadZone;

            const Value * tdzEnd = stackSpace + firstDeadZoneRegister + registerDeadZoneSize;
            for (Value *v = stackSpace + firstDeadZoneRegister; v < tdzEnd; ++v)
                *v = Value::emptyValue().asReturnedValue();
        }
    }

    QString source() const;
    QString function() const;
    inline QV4::ExecutionContext *context() const {
        return static_cast<ExecutionContext *>(&jsFrame->context);
    }
    int lineNumber() const;

    inline QV4::Heap::CallContext *callContext() const {
        Heap::ExecutionContext *ctx = static_cast<ExecutionContext &>(jsFrame->context).d();\
        while (ctx->type != Heap::ExecutionContext::Type_CallContext)
            ctx = ctx->outer;
        return static_cast<Heap::CallContext *>(ctx);
    }
    ReturnedValue thisObject() const;
};

}

QT_END_NAMESPACE

#endif
