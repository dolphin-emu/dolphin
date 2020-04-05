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

#ifndef QV4BYTECODEHANDLER_P_H
#define QV4BYTECODEHANDLER_P_H

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
#include <private/qv4instr_moth_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace Moth {

#define BYTECODE_HANDLER_DEFINE_ARGS(nargs, ...) \
    MOTH_EXPAND_FOR_MSVC(BYTECODE_HANDLER_DEFINE_ARGS##nargs(__VA_ARGS__))

#define BYTECODE_HANDLER_DEFINE_ARGS0()
#define BYTECODE_HANDLER_DEFINE_ARGS1(arg) \
    int arg
#define BYTECODE_HANDLER_DEFINE_ARGS2(arg1, arg2) \
    int arg1, \
    int arg2
#define BYTECODE_HANDLER_DEFINE_ARGS3(arg1, arg2, arg3) \
    int arg1, \
    int arg2, \
    int arg3
#define BYTECODE_HANDLER_DEFINE_ARGS4(arg1, arg2, arg3, arg4) \
    int arg1, \
    int arg2, \
    int arg3, \
    int arg4
#define BYTECODE_HANDLER_DEFINE_ARGS5(arg1, arg2, arg3, arg4, arg5) \
    int arg1, \
    int arg2, \
    int arg3, \
    int arg4, \
    int arg5

#define BYTECODE_HANDLER_DEFINE_VIRTUAL_BYTECODE_HANDLER_INSTRUCTION(name, nargs, ...) \
    virtual void generate_##name( \
    BYTECODE_HANDLER_DEFINE_ARGS(nargs, __VA_ARGS__) \
    ) = 0;

#define BYTECODE_HANDLER_DEFINE_VIRTUAL_BYTECODE_HANDLER(instr) \
    INSTR_##instr(BYTECODE_HANDLER_DEFINE_VIRTUAL_BYTECODE_HANDLER)

class ByteCodeHandler
{
public:
    virtual ~ByteCodeHandler();

    void decode(const char *code, uint len);

    int currentInstructionOffset() const { return _currentOffset; }
    int nextInstructionOffset() const { return _nextOffset; }
    int absoluteOffset(int relativeOffset) const
    { return nextInstructionOffset() + relativeOffset; }

protected:
    FOR_EACH_MOTH_INSTR(BYTECODE_HANDLER_DEFINE_VIRTUAL_BYTECODE_HANDLER)

    enum Verdict { ProcessInstruction, SkipInstruction };
    virtual Verdict startInstruction(Moth::Instr::Type instr) = 0;
    virtual void endInstruction(Moth::Instr::Type instr) = 0;

private:
    int _currentOffset = 0;
    int _nextOffset = 0;
};

} // Moth namespace
} // QV4 namespace

QT_END_NAMESPACE

#endif // QV4BYTECODEHANDLER_P_H
