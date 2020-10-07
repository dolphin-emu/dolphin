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

#ifndef QV4BASELINEASSEMBLER_P_H
#define QV4BASELINEASSEMBLER_P_H

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
#include <private/qv4function_p.h>
#include <QHash>

QT_REQUIRE_CONFIG(qml_jit);

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace JIT {

#define GENERATE_RUNTIME_CALL(function, destination) \
    callRuntime(reinterpret_cast<void *>(&Runtime::function::call), \
                destination)
#define GENERATE_TAIL_CALL(function) \
    tailCallRuntime(reinterpret_cast<void *>(&function))

class BaselineAssembler {
public:
    BaselineAssembler(const Value* constantTable);
    ~BaselineAssembler();

    // codegen infrastructure
    void generatePrologue();
    void generateEpilogue();
    void link(Function *function);
    void addLabel(int offset);

    // loads/stores/moves
    void loadConst(int constIndex);
    void copyConst(int constIndex, int destReg);
    void loadReg(int reg);
    void moveReg(int sourceReg, int destReg);
    void storeReg(int reg);
    void loadLocal(int index, int level = 0);
    void storeLocal(int index, int level = 0);
    void loadString(int stringId);
    void loadValue(ReturnedValue value);
    void storeHeapObject(int reg);
    void loadImport(int index);

    // numeric ops
    void unot();
    void toNumber();
    void uminus();
    void ucompl();
    void inc();
    void dec();
    void add(int lhs);
    void bitAnd(int lhs);
    void bitOr(int lhs);
    void bitXor(int lhs);
    void ushr(int lhs);
    void shr(int lhs);
    void shl(int lhs);
    void bitAndConst(int rhs);
    void bitOrConst(int rhs);
    void bitXorConst(int rhs);
    void ushrConst(int rhs);
    void shrConst(int rhs);
    void shlConst(int rhs);
    void mul(int lhs);
    void div(int lhs);
    void mod(int lhs);
    void sub(int lhs);

    // comparissons
    void cmpeqNull();
    void cmpneNull();
    void cmpeqInt(int lhs);
    void cmpneInt(int lhs);
    void cmpeq(int lhs);
    void cmpne(int lhs);
    void cmpgt(int lhs);
    void cmpge(int lhs);
    void cmplt(int lhs);
    void cmple(int lhs);
    void cmpStrictEqual(int lhs);
    void cmpStrictNotEqual(int lhs);

    // jumps
    Q_REQUIRED_RESULT int jump(int offset);
    Q_REQUIRED_RESULT int jumpTrue(int offset);
    Q_REQUIRED_RESULT int jumpFalse(int offset);
    Q_REQUIRED_RESULT int jumpNoException(int offset);
    Q_REQUIRED_RESULT int jumpNotUndefined(int offset);

    // stuff for runtime calls
    void prepareCallWithArgCount(int argc);
    void storeInstructionPointer(int instructionOffset);
    void passAccumulatorAsArg(int arg);
    void passFunctionAsArg(int arg);
    void passEngineAsArg(int arg);
    void passJSSlotAsArg(int reg, int arg);
    void passCppFrameAsArg(int arg);
    void passInt32AsArg(int value, int arg);
    void passPointerAsArg(void *ptr, int arg);
    void callRuntime(const void *funcPtr, CallResultDestination dest);
    void saveAccumulatorInFrame();
    void loadAccumulatorFromFrame();
    void jsTailCall(int func, int thisObject, int argc, int argv);

    // exception/context stuff
    void checkException();
    void gotoCatchException();
    void getException();
    void setException();
    Q_REQUIRED_RESULT int setUnwindHandler(int offset);
    void clearUnwindHandler();
    void unwindDispatch();
    Q_REQUIRED_RESULT int unwindToLabel(int level, int offset);
    void pushCatchContext(int index, int name);
    void popContext();
    void deadTemporalZoneCheck(int offsetForSavedIP, int variableName);

    // other stuff
    void ret();

protected:
    void *d;

private:
    typedef unsigned(*CmpFunc)(const Value&,const Value&);
    void cmp(int cond, CmpFunc function, int lhs);
};

} // namespace JIT
} // namespace QV4

QT_END_NAMESPACE

#endif // QV4BASELINEASSEMBLER_P_H
