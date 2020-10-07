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
#ifndef QV4ENGINEBASE_P_H
#define QV4ENGINEBASE_P_H

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

#include <private/qv4global_p.h>
#include <private/qv4runtimeapi_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct CppStackFrame;

// Base class for the execution engine

#if defined(Q_CC_MSVC) || defined(Q_CC_GNU)
#pragma pack(push, 1)
#endif
struct Q_QML_EXPORT EngineBase {

    CppStackFrame *currentStackFrame = nullptr;

    Value *jsStackTop = nullptr;

    // The JIT expects hasException and isInterrupted to be in the same 32bit word in memory.
    quint8 hasException = false;
    // isInterrupted is expected to be set from a different thread
#if defined(Q_ATOMIC_INT8_IS_SUPPORTED)
    QAtomicInteger<quint8> isInterrupted = false;
    quint16 unused = 0;
#elif defined(Q_ATOMIC_INT16_IS_SUPPORTED)
    quint8 unused = 0;
    QAtomicInteger<quint16> isInterrupted = false;
#else
#   error V4 needs either 8bit or 16bit atomics.
#endif

    quint8 isExecutingInRegExpJIT = false;
    quint8 padding[3];
    MemoryManager *memoryManager = nullptr;

    qint32 callDepth = 0;
    Value *jsStackLimit = nullptr;
    Value *jsStackBase = nullptr;

    IdentifierTable *identifierTable = nullptr;
    Object *globalObject = nullptr;

    // Exception handling
    Value *exceptionValue = nullptr;

    enum InternalClassType {
        Class_Empty,
        Class_String,
        Class_MemberData,
        Class_SimpleArrayData,
        Class_SparseArrayData,
        Class_ExecutionContext,
        Class_CallContext,
        Class_QmlContext,
        Class_Object,
        Class_ArrayObject,
        Class_FunctionObject,
        Class_ArrowFunction,
        Class_GeneratorFunction,
        Class_GeneratorObject,
        Class_StringObject,
        Class_SymbolObject,
        Class_ScriptFunction,
        Class_ConstructorFunction,
        Class_MemberFunction,
        Class_MemberGeneratorFunction,
        Class_ObjectProto,
        Class_RegExp,
        Class_RegExpObject,
        Class_RegExpExecArray,
        Class_ArgumentsObject,
        Class_StrictArgumentsObject,
        Class_ErrorObject,
        Class_ErrorObjectWithMessage,
        Class_ErrorProto,
        Class_QmlContextWrapper,
        Class_ProxyObject,
        Class_ProxyFunctionObject,
        Class_Symbol,
        NClasses
    };
    Heap::InternalClass *classes[NClasses];
    Heap::InternalClass *internalClasses(InternalClassType icType) { return classes[icType]; }
};
#if defined(Q_CC_MSVC) || defined(Q_CC_GNU)
#pragma pack(pop)
#endif

Q_STATIC_ASSERT(std::is_standard_layout<EngineBase>::value);
Q_STATIC_ASSERT(offsetof(EngineBase, currentStackFrame) == 0);
Q_STATIC_ASSERT(offsetof(EngineBase, jsStackTop) == offsetof(EngineBase, currentStackFrame) + QT_POINTER_SIZE);
Q_STATIC_ASSERT(offsetof(EngineBase, hasException) == offsetof(EngineBase, jsStackTop) + QT_POINTER_SIZE);
Q_STATIC_ASSERT(offsetof(EngineBase, memoryManager) == offsetof(EngineBase, hasException) + 8);
Q_STATIC_ASSERT(offsetof(EngineBase, isInterrupted) + sizeof(EngineBase::isInterrupted) <= offsetof(EngineBase, hasException) + 4);

}

QT_END_NAMESPACE

#endif
