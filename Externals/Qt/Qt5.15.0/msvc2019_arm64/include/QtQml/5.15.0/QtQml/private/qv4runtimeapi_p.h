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
#ifndef QV4RUNTIMEAPI_P_H
#define QV4RUNTIMEAPI_P_H

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
#include <private/qv4staticvalue_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

typedef uint Bool;

struct Q_QML_PRIVATE_EXPORT Runtime {
    typedef ReturnedValue (*UnaryOperation)(const Value &value);
    typedef ReturnedValue (*BinaryOperation)(const Value &left, const Value &right);
    typedef ReturnedValue (*BinaryOperationContext)(ExecutionEngine *, const Value &left, const Value &right);

    enum class Throws { No, Yes };
    enum class ChangesContext { No, Yes };
    enum class Pure { No, Yes };
    enum class LastArgumentIsOutputValue { No, Yes };

    template<Throws t, ChangesContext c = ChangesContext::No, Pure p = Pure::No,
             LastArgumentIsOutputValue out = LastArgumentIsOutputValue::No>
    struct Method
    {
        static constexpr bool throws = t == Throws::Yes;
        static constexpr bool changesContext = c == ChangesContext::Yes;
        static constexpr bool pure = p == Pure::Yes;
        static constexpr bool lastArgumentIsOutputValue = out == LastArgumentIsOutputValue::Yes;
    };
    using PureMethod = Method<Throws::No, ChangesContext::No, Pure::Yes>;
    using IteratorMethod = Method<Throws::Yes, ChangesContext::No, Pure::No,
                                  LastArgumentIsOutputValue::Yes>;

    /* call */
    struct Q_QML_PRIVATE_EXPORT CallGlobalLookup : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, uint, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallQmlContextPropertyLookup : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, uint, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallName : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, int, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallProperty : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, int, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallPropertyLookup : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, uint, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallElement : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallValue : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallWithReceiver : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallPossiblyDirectEval : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CallWithSpread : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT TailCall : Method<Throws::Yes>
    {
        static ReturnedValue call(CppStackFrame *, ExecutionEngine *);
    };

    /* construct */
    struct Q_QML_PRIVATE_EXPORT Construct : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT ConstructWithSpread : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &, Value[], int);
    };

    /* load & store */
    struct Q_QML_PRIVATE_EXPORT StoreNameStrict : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, int, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT StoreNameSloppy : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, int, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT StoreProperty : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, const Value &, int, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT StoreElement : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, const Value &, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT LoadProperty : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, int);
    };
    struct Q_QML_PRIVATE_EXPORT LoadName : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, int);
    };
    struct Q_QML_PRIVATE_EXPORT LoadElement : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT LoadSuperProperty : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT StoreSuperProperty : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT LoadSuperConstructor : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT LoadGlobalLookup : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, Function *, int);
    };
    struct Q_QML_PRIVATE_EXPORT LoadQmlContextPropertyLookup : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, uint);
    };
    struct Q_QML_PRIVATE_EXPORT GetLookup : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, Function *, const Value &, int);
    };
    struct Q_QML_PRIVATE_EXPORT SetLookupStrict : Method<Throws::Yes>
    {
        static void call(Function *, const Value &, int, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT SetLookupSloppy : Method<Throws::Yes>
    {
        static void call(Function *, const Value &, int, const Value &);
    };

    /* typeof */
    struct Q_QML_PRIVATE_EXPORT TypeofValue : PureMethod
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT TypeofName : Method<Throws::No>
    {
        static ReturnedValue call(ExecutionEngine *, int);
    };

    /* delete */
    struct Q_QML_PRIVATE_EXPORT DeleteProperty_NoThrow : Method<Throws::No>
    {
        static Bool call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT DeleteProperty : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, Function *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT DeleteName_NoThrow : Method<Throws::No>
    {
        static Bool call(ExecutionEngine *, int);
    };
    struct Q_QML_PRIVATE_EXPORT DeleteName : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, Function *, int);
    };

    /* exceptions & scopes */
    struct Q_QML_PRIVATE_EXPORT ThrowException : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT PushCallContext : Method<Throws::No, ChangesContext::Yes>
    {
        static void call(CppStackFrame *);
    };
    struct Q_QML_PRIVATE_EXPORT PushWithContext : Method<Throws::Yes, ChangesContext::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT PushCatchContext : Method<Throws::No, ChangesContext::Yes>
    {
        static void call(ExecutionEngine *, int, int);
    };
    struct Q_QML_PRIVATE_EXPORT PushBlockContext : Method<Throws::No, ChangesContext::Yes>
    {
        static void call(ExecutionEngine *, int);
    };
    struct Q_QML_PRIVATE_EXPORT CloneBlockContext : Method<Throws::No, ChangesContext::Yes>
    {
        static void call(ExecutionEngine *);
    };
    struct Q_QML_PRIVATE_EXPORT PushScriptContext : Method<Throws::No, ChangesContext::Yes>
    {
        static void call(ExecutionEngine *, int);
    };
    struct Q_QML_PRIVATE_EXPORT PopScriptContext : Method<Throws::No, ChangesContext::Yes>
    {
        static void call(ExecutionEngine *);
    };
    struct Q_QML_PRIVATE_EXPORT ThrowReferenceError : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, int);
    };
    struct Q_QML_PRIVATE_EXPORT ThrowOnNullOrUndefined : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, const Value &);
    };

    /* closures */
    struct Q_QML_PRIVATE_EXPORT Closure : Method<Throws::No>
    {
        static ReturnedValue call(ExecutionEngine *, int);
    };

    /* Function header */
    struct Q_QML_PRIVATE_EXPORT ConvertThisToObject : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT DeclareVar : Method<Throws::Yes>
    {
        static void call(ExecutionEngine *, Bool, int);
    };
    struct Q_QML_PRIVATE_EXPORT CreateMappedArgumentsObject : Method<Throws::No>
    {
        static ReturnedValue call(ExecutionEngine *);
    };
    struct Q_QML_PRIVATE_EXPORT CreateUnmappedArgumentsObject : Method<Throws::No>
    {
        static ReturnedValue call(ExecutionEngine *);
    };
    struct Q_QML_PRIVATE_EXPORT CreateRestParameter : PureMethod
    {
        static ReturnedValue call(ExecutionEngine *, int);
    };

    /* literals */
    struct Q_QML_PRIVATE_EXPORT ArrayLiteral : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, Value[], uint);
    };
    struct Q_QML_PRIVATE_EXPORT ObjectLiteral : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, int, Value[], int);
    };
    struct Q_QML_PRIVATE_EXPORT CreateClass : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, int, const Value &, Value[]);
    };

    /* for-in, for-of and array destructuring */
    struct Q_QML_PRIVATE_EXPORT GetIterator : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, int);
    };
    struct Q_QML_PRIVATE_EXPORT IteratorNext : IteratorMethod
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, Value *);
    };
    struct Q_QML_PRIVATE_EXPORT IteratorNextForYieldStar : IteratorMethod
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &, Value *);
    };
    struct Q_QML_PRIVATE_EXPORT IteratorClose : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT DestructureRestElement : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };

    /* conversions */
    struct Q_QML_PRIVATE_EXPORT ToObject : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT ToBoolean : PureMethod
    {
        static Bool call(const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT ToNumber : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &);
    };
    /* unary operators */
    struct Q_QML_PRIVATE_EXPORT UMinus : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &);
    };

    /* binary operators */
    struct Q_QML_PRIVATE_EXPORT Instanceof : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT In : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Add : Method<Throws::Yes>
    {
        static ReturnedValue call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Sub : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Mul : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Div : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Mod : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Exp : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT BitAnd : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT BitOr : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT BitXor : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Shl : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Shr : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT UShr : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT GreaterThan : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT LessThan : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT GreaterEqual : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT LessEqual : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT Equal : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT NotEqual : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT StrictEqual : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT StrictNotEqual : Method<Throws::Yes>
    {
        static ReturnedValue call(const Value &, const Value &);
    };

    /* comparisons */
    struct Q_QML_PRIVATE_EXPORT CompareGreaterThan : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareLessThan : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareGreaterEqual : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareLessEqual : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareEqual : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareNotEqual : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareStrictEqual : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareStrictNotEqual : Method<Throws::Yes>
    {
        static Bool call(const Value &, const Value &);
    };

    struct Q_QML_PRIVATE_EXPORT CompareInstanceof : Method<Throws::Yes>
    {
        static Bool call(ExecutionEngine *, const Value &, const Value &);
    };
    struct Q_QML_PRIVATE_EXPORT CompareIn : Method<Throws::Yes>
    {
        static Bool call(ExecutionEngine *, const Value &, const Value &);
    };

    struct Q_QML_PRIVATE_EXPORT RegexpLiteral : PureMethod
    {
        static ReturnedValue call(ExecutionEngine *, int);
    };
    struct Q_QML_PRIVATE_EXPORT GetTemplateObject : PureMethod
    {
        static ReturnedValue call(Function *, int);
    };

    struct StackOffsets {
        static const int tailCall_function   = -1;
        static const int tailCall_thisObject = -2;
        static const int tailCall_argv       = -3;
        static const int tailCall_argc       = -4;
    };

    static QHash<const void *, const char *> symbolTable();
};

static_assert(std::is_standard_layout<Runtime>::value, "Runtime needs to be standard layout in order for us to be able to use offsetof");
static_assert(sizeof(Runtime::BinaryOperation) == sizeof(void*), "JIT expects a function pointer to fit into a regular pointer, for cross-compilation offset translation");

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4RUNTIMEAPI_P_H
