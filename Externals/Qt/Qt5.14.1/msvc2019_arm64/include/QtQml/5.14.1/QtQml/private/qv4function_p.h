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
#ifndef QV4FUNCTION_H
#define QV4FUNCTION_H

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
#include <private/qv4executablecompilationunit_p.h>
#include <private/qv4context_p.h>
#include <private/qv4string_p.h>

namespace JSC {
class MacroAssemblerCodeRef;
}

QT_BEGIN_NAMESPACE

struct QQmlSourceLocation;

namespace QV4 {

struct Q_QML_EXPORT FunctionData {
    CompiledData::CompilationUnitBase *compilationUnit;

    // Intentionally require an ExecutableCompilationUnit but save only a pointer to
    // CompilationUnitBase. This is so that we can take advantage of the standard layout
    // of CompilationUnitBase in the JIT. Furthermore we can safely static_cast to
    // ExecutableCompilationUnit where we need it.
    FunctionData(ExecutableCompilationUnit *compilationUnit)
        : compilationUnit(compilationUnit)
    {}
};
// Make sure this class can be accessed through offsetof (done by the assemblers):
Q_STATIC_ASSERT(std::is_standard_layout< FunctionData >::value);

struct Q_QML_EXPORT Function : public FunctionData {
private:
    Function(ExecutionEngine *engine, ExecutableCompilationUnit *unit,
             const CompiledData::Function *function);
    ~Function();

public:
    const CompiledData::Function *compiledFunction;

    QV4::ExecutableCompilationUnit *executableCompilationUnit() const
    {
        // This is safe: We require an ExecutableCompilationUnit in the ctor.
        return static_cast<QV4::ExecutableCompilationUnit *>(compilationUnit);
    }

    QV4::Heap::String *runtimeString(uint i) const
    {
        return compilationUnit->runtimeStrings[i];
    }

    ReturnedValue call(const Value *thisObject, const Value *argv, int argc, const ExecutionContext *context);

    const char *codeData;

    typedef ReturnedValue (*JittedCode)(CppStackFrame *, ExecutionEngine *);
    JittedCode jittedCode;
    JSC::MacroAssemblerCodeRef *codeRef;

    // first nArguments names in internalClass are the actual arguments
    Heap::InternalClass *internalClass;
    uint nFormals;
    int interpreterCallCount = 0;
    bool isEval = false;

    static Function *create(ExecutionEngine *engine, ExecutableCompilationUnit *unit,
                            const CompiledData::Function *function);
    void destroy();

    // used when dynamically assigning signal handlers (QQmlConnection)
    void updateInternalClass(ExecutionEngine *engine, const QList<QByteArray> &parameters);

    inline Heap::String *name() const {
        return runtimeString(compiledFunction->nameIndex);
    }

    static QString prettyName(const Function *function, const void *address);

    inline QString sourceFile() const { return executableCompilationUnit()->fileName(); }
    inline QUrl finalUrl() const { return executableCompilationUnit()->finalUrl(); }

    inline bool isStrict() const { return compiledFunction->flags & CompiledData::Function::IsStrict; }
    inline bool isArrowFunction() const { return compiledFunction->flags & CompiledData::Function::IsArrowFunction; }
    inline bool isGenerator() const { return compiledFunction->flags & CompiledData::Function::IsGenerator; }

    QQmlSourceLocation sourceLocation() const;

    Function *nestedFunction() const
    {
        if (compiledFunction->nestedFunctionIndex == std::numeric_limits<uint32_t>::max())
            return nullptr;
        return executableCompilationUnit()->runtimeFunctions[compiledFunction->nestedFunctionIndex];
    }
};

}

QT_END_NAMESPACE

#endif
