/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#ifndef QV4JSCALL_H
#define QV4JSCALL_H

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
#include "qv4function_p.h"
#include "qv4functionobject_p.h"
#include "qv4context_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4stackframe_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct JSCallData {
    JSCallData(const Scope &scope, int argc = 0, const Value *argv = nullptr, const Value *thisObject = nullptr)
        : scope(scope), argc(argc)
    {
        if (thisObject)
            this->thisObject = const_cast<Value *>(thisObject);
        else
            this->thisObject = scope.alloc();
        if (argv)
            this->args = const_cast<Value *>(argv);
        else
            this->args = scope.alloc(argc);
    }

    JSCallData *operator->() {
        return this;
    }

    CallData *callData(const FunctionObject *f = nullptr) const {
        int size = int(offsetof(QV4::CallData, args)/sizeof(QV4::Value)) + argc;
        CallData *ptr = reinterpret_cast<CallData *>(scope.alloc<Scope::Uninitialized>(size));
        ptr->function = Encode::undefined();
        ptr->context = Encode::undefined();
        ptr->accumulator = Encode::undefined();
        ptr->thisObject = thisObject->asReturnedValue();
        ptr->newTarget = Encode::undefined();
        ptr->setArgc(argc);
        if (argc)
            memcpy(ptr->args, args, argc*sizeof(Value));
        if (f)
            ptr->function = f->asReturnedValue();
        return ptr;
    }
    const Scope &scope;
    int argc;
    Value *args;
    Value *thisObject;
};

inline
ReturnedValue FunctionObject::callAsConstructor(const JSCallData &data) const
{
    return callAsConstructor(data.args, data.argc, this);
}

inline
ReturnedValue FunctionObject::call(const JSCallData &data) const
{
    return call(data.thisObject, data.args, data.argc);
}


struct ScopedStackFrame {
    Scope &scope;
    CppStackFrame frame;

    ScopedStackFrame(Scope &scope, Heap::ExecutionContext *context)
        : scope(scope)
    {
        frame.parent = scope.engine->currentStackFrame;
        if (!context)
            return;
        frame.jsFrame = reinterpret_cast<CallData *>(scope.alloc(sizeof(CallData)/sizeof(Value)));
        frame.jsFrame->context = context;
        frame.v4Function = frame.parent ? frame.parent->v4Function : nullptr;
        scope.engine->currentStackFrame = &frame;
    }
    ~ScopedStackFrame() {
        scope.engine->currentStackFrame = frame.parent;
    }
};

}

QT_END_NAMESPACE

#endif // QV4JSCALL_H
