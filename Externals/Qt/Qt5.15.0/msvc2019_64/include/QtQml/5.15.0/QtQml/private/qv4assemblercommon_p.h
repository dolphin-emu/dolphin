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

#ifndef QV4PLATFORMASSEMBLER_P_H
#define QV4PLATFORMASSEMBLER_P_H

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

#include <private/qv4engine_p.h>
#include <private/qv4global_p.h>
#include <private/qv4function_p.h>
#include <QHash>
#include <wtf/Vector.h>
#include <assembler/MacroAssembler.h>

QT_REQUIRE_CONFIG(qml_jit);

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace JIT {

#if defined(Q_PROCESSOR_X86_64) || defined(ENABLE_ALL_ASSEMBLERS_FOR_REFACTORING_PURPOSES)
#if defined(Q_OS_LINUX) || defined(Q_OS_QNX) || defined(Q_OS_FREEBSD) || defined(Q_OS_DARWIN)

class PlatformAssembler_X86_64_SysV : public JSC::MacroAssembler<JSC::MacroAssemblerX86_64>
{
public:
    static constexpr int NativeStackAlignment = 16;

    static const RegisterID NoRegister = RegisterID(-1);

    static const RegisterID ReturnValueRegister   = RegisterID::eax;
    static const RegisterID ReturnValueRegisterValue = ReturnValueRegister;
    static const RegisterID AccumulatorRegister   = RegisterID::eax;
    static const RegisterID AccumulatorRegisterValue = AccumulatorRegister;
    static const RegisterID ScratchRegister       = RegisterID::r10;
    static const RegisterID ScratchRegister2      = RegisterID::r9; // Note: overlaps with Arg5Reg, so do not use while setting up a call!
    static const RegisterID JSStackFrameRegister  = RegisterID::r12;
    static const RegisterID CppStackFrameRegister = RegisterID::r13;
    static const RegisterID EngineRegister        = RegisterID::r14;
    static const RegisterID StackPointerRegister  = RegisterID::esp;
    static const RegisterID FramePointerRegister  = RegisterID::ebp;
    static const FPRegisterID FPScratchRegister   = FPRegisterID::xmm1;
    static const FPRegisterID FPScratchRegister2  = FPRegisterID::xmm2;

    static const RegisterID Arg0Reg = RegisterID::edi;
    static const RegisterID Arg1Reg = RegisterID::esi;
    static const RegisterID Arg2Reg = RegisterID::edx;
    static const RegisterID Arg3Reg = RegisterID::ecx;
    static const RegisterID Arg4Reg = RegisterID::r8;
    static const RegisterID Arg5Reg = RegisterID::r9;
    static const RegisterID Arg6Reg = NoRegister;
    static const RegisterID Arg7Reg = NoRegister;
    static const int ArgInRegCount = 6;

    void popValue()
    {
        addPtr(TrustedImmPtr(sizeof(ReturnedValue)), StackPointerRegister);
    }

    void generatePlatformFunctionEntry()
    {
        push(FramePointerRegister);
        move(StackPointerRegister, FramePointerRegister);
        move(TrustedImmPtr(nullptr), AccumulatorRegister); push(AccumulatorRegister); // exceptionHandler
        push(JSStackFrameRegister);
        push(CppStackFrameRegister);
        push(EngineRegister);
        move(Arg0Reg, CppStackFrameRegister);
        move(Arg1Reg, EngineRegister);
    }

    void generatePlatformFunctionExit(bool tailCall = false)
    {
        pop(EngineRegister);
        pop(CppStackFrameRegister);
        pop(JSStackFrameRegister);
        pop(); // exceptionHandler
        pop(FramePointerRegister);
        if (!tailCall)
            ret();
    }

    void callAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        call(ScratchRegister);
    }

    void jumpAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        jump(ScratchRegister);
    }

    void pushAligned(RegisterID reg)
    {
        subPtr(TrustedImm32(PointerSize), StackPointerRegister);
        push(reg);
    }

    void popAligned(RegisterID reg)
    {
        pop(reg);
        addPtr(TrustedImm32(PointerSize), StackPointerRegister);
    }
};

typedef PlatformAssembler_X86_64_SysV PlatformAssemblerBase;

#endif
#if defined(Q_OS_WIN)

class PlatformAssembler_Win64 : public JSC::MacroAssembler<JSC::MacroAssemblerX86_64>
{
public:
    static const RegisterID NoRegister = RegisterID(-1);

    static const RegisterID ReturnValueRegister   = RegisterID::eax;
    static const RegisterID ReturnValueRegisterValue = ReturnValueRegister;
    static const RegisterID AccumulatorRegister   = RegisterID::eax;
    static const RegisterID AccumulatorRegisterValue = AccumulatorRegister;
    static const RegisterID ScratchRegister       = RegisterID::r10;
    static const RegisterID ScratchRegister2      = RegisterID::r9; // Note: overlaps with Arg3Reg, so do not use while setting up a call!
    static const RegisterID JSStackFrameRegister  = RegisterID::r12;
    static const RegisterID CppStackFrameRegister = RegisterID::r13;
    static const RegisterID EngineRegister        = RegisterID::r14;
    static const RegisterID StackPointerRegister  = RegisterID::esp;
    static const RegisterID FramePointerRegister  = RegisterID::ebp;
    static const FPRegisterID FPScratchRegister   = FPRegisterID::xmm1;

    static const RegisterID Arg0Reg = RegisterID::ecx;
    static const RegisterID Arg1Reg = RegisterID::edx;
    static const RegisterID Arg2Reg = RegisterID::r8;
    static const RegisterID Arg3Reg = RegisterID::r9;
    static const RegisterID Arg4Reg = NoRegister;
    static const RegisterID Arg5Reg = NoRegister;
    static const RegisterID Arg6Reg = NoRegister;
    static const RegisterID Arg7Reg = NoRegister;
    static const int ArgInRegCount = 4;

    void popValue()
    {
        addPtr(TrustedImmPtr(sizeof(ReturnedValue)), StackPointerRegister);
    }

    void generatePlatformFunctionEntry()
    {
        push(FramePointerRegister);
        move(StackPointerRegister, FramePointerRegister);
        move(TrustedImmPtr(nullptr), AccumulatorRegister); push(AccumulatorRegister); // exceptionHandler
        push(JSStackFrameRegister);
        push(CppStackFrameRegister);
        push(EngineRegister);
        move(Arg0Reg, CppStackFrameRegister);
        move(Arg1Reg, EngineRegister);
    }

    void generatePlatformFunctionExit(bool tailCall = false)
    {
        pop(EngineRegister);
        pop(CppStackFrameRegister);
        pop(JSStackFrameRegister);
        pop(); // exceptionHandler
        pop(FramePointerRegister);
        if (!tailCall)
            ret();
    }

    void callAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        subPtr(TrustedImm32(4 * PointerSize), StackPointerRegister);
        call(ScratchRegister);
        addPtr(TrustedImm32(4 * PointerSize), StackPointerRegister);
    }

    void jumpAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        jump(ScratchRegister);
    }

    void pushAligned(RegisterID reg)
    {
        subPtr(TrustedImm32(PointerSize), StackPointerRegister);
        push(reg);
    }

    void popAligned(RegisterID reg)
    {
        pop(reg);
        addPtr(TrustedImm32(PointerSize), StackPointerRegister);
    }
};

typedef PlatformAssembler_Win64 PlatformAssemblerBase;

#endif
#endif

#if (defined(Q_PROCESSOR_X86) && !defined(Q_PROCESSOR_X86_64)) || defined(ENABLE_ALL_ASSEMBLERS_FOR_REFACTORING_PURPOSES)

class PlatformAssembler_X86_All : public JSC::MacroAssembler<JSC::MacroAssemblerX86>
{
public:
    static const RegisterID NoRegister = RegisterID(-1);

    static const RegisterID ReturnValueRegisterValue = RegisterID::eax;
    static const RegisterID ReturnValueRegisterTag   = RegisterID::edx;
    static const RegisterID ScratchRegister          = RegisterID::ecx;
    static const RegisterID AccumulatorRegisterValue = ReturnValueRegisterValue;
    static const RegisterID AccumulatorRegisterTag   = ReturnValueRegisterTag;
    static const RegisterID JSStackFrameRegister  = RegisterID::ebx;
    static const RegisterID CppStackFrameRegister = RegisterID::esi;
    static const RegisterID EngineRegister        = RegisterID::edi;
    static const RegisterID StackPointerRegister  = RegisterID::esp;
    static const RegisterID FramePointerRegister  = RegisterID::ebp;
    static const FPRegisterID FPScratchRegister   = FPRegisterID::xmm1;

    static const RegisterID Arg0Reg = NoRegister;
    static const RegisterID Arg1Reg = NoRegister;
    static const RegisterID Arg2Reg = NoRegister;
    static const RegisterID Arg3Reg = NoRegister;
    static const RegisterID Arg4Reg = NoRegister;
    static const RegisterID Arg5Reg = NoRegister;
    static const RegisterID Arg6Reg = NoRegister;
    static const RegisterID Arg7Reg = NoRegister;
    static const int ArgInRegCount = 0;

    void popValue()
    {
        addPtr(TrustedImmPtr(sizeof(ReturnedValue)), StackPointerRegister);
    }

    void generatePlatformFunctionEntry()
    {
        push(RegisterID::ebp);
        move(RegisterID::esp, RegisterID::ebp);
        move(TrustedImmPtr(nullptr), AccumulatorRegisterValue); push(AccumulatorRegisterValue); // exceptionHandler
        push(JSStackFrameRegister);
        push(CppStackFrameRegister);
        push(EngineRegister);
        // Ensure the stack is 16-byte aligned in order for compiler generated aligned SSE2
        // instructions to be able to target the stack.
        subPtr(TrustedImm32(8), StackPointerRegister);
        loadPtr(Address(FramePointerRegister, 2 * PointerSize), CppStackFrameRegister);
        loadPtr(Address(FramePointerRegister, 3 * PointerSize), EngineRegister);
    }

    void generatePlatformFunctionExit(bool tailCall = false)
    {
        addPtr(TrustedImm32(8), StackPointerRegister);
        pop(EngineRegister);
        pop(CppStackFrameRegister);
        pop(JSStackFrameRegister);
        pop(); // exceptionHandler
        pop(RegisterID::ebp);
        if (!tailCall)
            ret();
    }

    void callAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        call(ScratchRegister);
    }

    void jumpAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        jump(ScratchRegister);
    }

    void pushAligned(RegisterID reg)
    {
        subPtr(TrustedImm32(3 * PointerSize), StackPointerRegister);
        push(reg);
    }

    void popAligned(RegisterID reg)
    {
        pop(reg);
        addPtr(TrustedImm32(3 * PointerSize), StackPointerRegister);
    }
};

typedef PlatformAssembler_X86_All PlatformAssemblerBase;

#endif

#if defined(Q_PROCESSOR_ARM_64) || defined(ENABLE_ALL_ASSEMBLERS_FOR_REFACTORING_PURPOSES)

class PlatformAssembler_ARM64 : public JSC::MacroAssembler<JSC::MacroAssemblerARM64>
{
public:
    static const RegisterID NoRegister = RegisterID(-1);

    static const RegisterID ReturnValueRegister   = JSC::ARM64Registers::x0;
    static const RegisterID ReturnValueRegisterValue = ReturnValueRegister;
    static const RegisterID AccumulatorRegister   = JSC::ARM64Registers::x9;
    static const RegisterID AccumulatorRegisterValue = AccumulatorRegister;
    static const RegisterID ScratchRegister       = JSC::ARM64Registers::x10;
    static const RegisterID ScratchRegister2      = JSC::ARM64Registers::x7; // Note: overlaps with Arg7Reg, so do not use while setting up a call!
    static const RegisterID JSStackFrameRegister  = JSC::ARM64Registers::x19;
    static const RegisterID CppStackFrameRegister = JSC::ARM64Registers::x20;
    static const RegisterID EngineRegister        = JSC::ARM64Registers::x21;
    static const RegisterID StackPointerRegister  = JSC::ARM64Registers::sp;
    static const RegisterID FramePointerRegister  = JSC::ARM64Registers::fp;
    static const FPRegisterID FPScratchRegister   = JSC::ARM64Registers::q1;

    static const RegisterID Arg0Reg = JSC::ARM64Registers::x0;
    static const RegisterID Arg1Reg = JSC::ARM64Registers::x1;
    static const RegisterID Arg2Reg = JSC::ARM64Registers::x2;
    static const RegisterID Arg3Reg = JSC::ARM64Registers::x3;
    static const RegisterID Arg4Reg = JSC::ARM64Registers::x4;
    static const RegisterID Arg5Reg = JSC::ARM64Registers::x5;
    static const RegisterID Arg6Reg = JSC::ARM64Registers::x6;
    static const RegisterID Arg7Reg = JSC::ARM64Registers::x7;
    static const int ArgInRegCount = 8;

    void push(RegisterID src)
    {
        pushToSave(src);
    }

    void pop(RegisterID dest)
    {
        popToRestore(dest);
    }

    void pop()
    {
        add64(TrustedImm32(16), stackPointerRegister);
    }

    void popValue()
    {
        pop();
    }

    void generatePlatformFunctionEntry()
    {
        pushPair(JSC::ARM64Registers::fp, JSC::ARM64Registers::lr);
        move(RegisterID::sp, RegisterID::fp);
        move(TrustedImmPtr(nullptr), AccumulatorRegister); // exceptionHandler
        pushPair(JSStackFrameRegister, AccumulatorRegister);
        pushPair(EngineRegister, CppStackFrameRegister);
        move(Arg0Reg, CppStackFrameRegister);
        move(Arg1Reg, EngineRegister);
    }

    void generatePlatformFunctionExit(bool tailCall = false)
    {
        if (!tailCall) // do not overwrite arg0 (used in the tail call)
            move(AccumulatorRegister, ReturnValueRegister);
        popPair(EngineRegister, CppStackFrameRegister);
        popPair(JSStackFrameRegister, AccumulatorRegister);
        popPair(JSC::ARM64Registers::fp, JSC::ARM64Registers::lr);
        if (!tailCall)
            ret();
    }

    void callAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        call(ScratchRegister);
    }

    void jumpAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), ScratchRegister);
        jump(ScratchRegister);
    }

    void pushAligned(RegisterID reg)
    {
        pushToSave(reg);
    }

    void popAligned(RegisterID reg)
    {
        popToRestore(reg);
    }
};

typedef PlatformAssembler_ARM64 PlatformAssemblerBase;

#endif

#if defined(Q_PROCESSOR_ARM_32) || defined(ENABLE_ALL_ASSEMBLERS_FOR_REFACTORING_PURPOSES)

class PlatformAssembler_ARM32 : public JSC::MacroAssembler<JSC::MacroAssemblerARMv7>
{
public:
    static const RegisterID NoRegister = RegisterID(-1);

    static const RegisterID ReturnValueRegisterValue = JSC::ARMRegisters::r0;
    static const RegisterID ReturnValueRegisterTag   = JSC::ARMRegisters::r1;
    static const RegisterID ScratchRegister          = JSC::ARMRegisters::r2;
    static const RegisterID AccumulatorRegisterValue = JSC::ARMRegisters::r4;
    static const RegisterID AccumulatorRegisterTag   = JSC::ARMRegisters::r5;
    // r6 is used by MacroAssemblerARMv7
    static const RegisterID JSStackFrameRegister     = JSC::ARMRegisters::r8;
    static const RegisterID CppStackFrameRegister    = JSC::ARMRegisters::r10;
#if CPU(ARM_THUMB2)
    static const RegisterID FramePointerRegister     = JSC::ARMRegisters::r7;
    static const RegisterID EngineRegister           = JSC::ARMRegisters::r11;
#else // Thumbs down
    static const RegisterID FramePointerRegister     = JSC::ARMRegisters::r11;
    static const RegisterID EngineRegister           = JSC::ARMRegisters::r7;
#endif
    static const RegisterID StackPointerRegister     = JSC::ARMRegisters::r13;
    static const FPRegisterID FPScratchRegister      = JSC::ARMRegisters::d1;

    static const RegisterID Arg0Reg = JSC::ARMRegisters::r0;
    static const RegisterID Arg1Reg = JSC::ARMRegisters::r1;
    static const RegisterID Arg2Reg = JSC::ARMRegisters::r2;
    static const RegisterID Arg3Reg = JSC::ARMRegisters::r3;
    static const RegisterID Arg4Reg = NoRegister;
    static const RegisterID Arg5Reg = NoRegister;
    static const RegisterID Arg6Reg = NoRegister;
    static const RegisterID Arg7Reg = NoRegister;
    static const int ArgInRegCount = 4;

    void popValue()
    {
        addPtr(TrustedImm32(sizeof(ReturnedValue)), StackPointerRegister);
    }

    void generatePlatformFunctionEntry()
    {
        push(JSC::ARMRegisters::lr);
        push(FramePointerRegister);
        move(StackPointerRegister, FramePointerRegister);
        push(TrustedImm32(0)); // exceptionHandler
        push(AccumulatorRegisterValue);
        push(AccumulatorRegisterTag);
        push(addressTempRegister);
        push(JSStackFrameRegister);
        push(CppStackFrameRegister);
        push(EngineRegister);
        subPtr(TrustedImm32(4), StackPointerRegister); // stack alignment
        move(Arg0Reg, CppStackFrameRegister);
        move(Arg1Reg, EngineRegister);
    }

    void generatePlatformFunctionExit(bool tailCall = false)
    {
        if (!tailCall) { // do not overwrite arg0 and arg1 (used in the tail call)
            move(AccumulatorRegisterValue, ReturnValueRegisterValue);
            move(AccumulatorRegisterTag, ReturnValueRegisterTag);
        }
        addPtr(TrustedImm32(4), StackPointerRegister); // stack alignment
        pop(EngineRegister);
        pop(CppStackFrameRegister);
        pop(JSStackFrameRegister);
        pop(addressTempRegister);
        pop(AccumulatorRegisterTag);
        pop(AccumulatorRegisterValue);
        pop(); // exceptionHandler
        pop(FramePointerRegister);
        pop(JSC::ARMRegisters::lr);
        if (!tailCall)
            ret();
    }

    void callAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), dataTempRegister);
        call(dataTempRegister);
    }

    void jumpAbsolute(const void *funcPtr)
    {
        move(TrustedImmPtr(funcPtr), dataTempRegister);
        jump(dataTempRegister);
    }

    void pushAligned(RegisterID reg)
    {
        subPtr(TrustedImm32(PointerSize), StackPointerRegister);
        push(reg);
    }

    void popAligned(RegisterID reg)
    {
        pop(reg);
        addPtr(TrustedImm32(PointerSize), StackPointerRegister);
    }
};

typedef PlatformAssembler_ARM32 PlatformAssemblerBase;
#endif

class PlatformAssemblerCommon : public JIT::PlatformAssemblerBase
{
public:
    PlatformAssemblerCommon(const Value *constantTable)
        : constantTable(constantTable)
    {}

    virtual ~PlatformAssemblerCommon();

    Address exceptionHandlerAddress() const
    {
        return Address(FramePointerRegister, -1 * PointerSize);
    }

    Address contextAddress() const
    {
        return Address(JSStackFrameRegister, offsetof(CallData, context));
    }

    RegisterID registerForArg(int arg) const
    {
        Q_ASSERT(arg >= 0);
        Q_ASSERT(arg < ArgInRegCount);
        switch (arg) {
        case 0: return Arg0Reg;
        case 1: return Arg1Reg;
        case 2: return Arg2Reg;
        case 3: return Arg3Reg;
        case 4: return Arg4Reg;
        case 5: return Arg5Reg;
        case 6: return Arg6Reg;
        case 7: return Arg7Reg;
        default:
            Q_UNIMPLEMENTED();
            Q_UNREACHABLE();
        }
    }

    Address loadFunctionPtr(RegisterID target)
    {
        Address addr(CppStackFrameRegister, offsetof(CppStackFrame, v4Function));
        loadPtr(addr, target);
        return Address(target);
    }

    Address loadCompilationUnitPtr(RegisterID target)
    {
        Address addr = loadFunctionPtr(target);
        addr.offset = offsetof(QV4::FunctionData, compilationUnit);
        loadPtr(addr, target);
        return Address(target);
    }

    Address loadConstAddress(int constIndex, RegisterID baseReg = ScratchRegister)
    {
        Address addr = loadCompilationUnitPtr(baseReg);
        addr.offset = offsetof(QV4::CompiledData::CompilationUnitBase, constants);
        loadPtr(addr, baseReg);
        addr.offset = constIndex * int(sizeof(QV4::Value));
        return addr;
    }

    Address loadStringAddress(int stringId)
    {
        Address addr = loadCompilationUnitPtr(ScratchRegister);
        addr.offset = offsetof(QV4::CompiledData::CompilationUnitBase, runtimeStrings);
        loadPtr(addr, ScratchRegister);
        return Address(ScratchRegister, stringId * PointerSize);
    }

    void passAsArg(RegisterID src, int arg)
    {
        move(src, registerForArg(arg));
    }

    void generateCatchTrampoline(std::function<void()> loadUndefined)
    {
        for (Jump j : catchyJumps)
            j.link(this);

        // We don't need to check for isInterrupted here because if that is set,
        // then the first checkException() in any exception handler will find another "exception"
        // and jump out of the exception handler.
        loadPtr(exceptionHandlerAddress(), ScratchRegister);
        Jump exitFunction = branchPtr(Equal, ScratchRegister, TrustedImmPtr(0));
        loadUndefined();
        jump(ScratchRegister);
        exitFunction.link(this);

        if (functionExit.isSet())
            jump(functionExit);
        else
            generateFunctionExit();
    }

    void checkException()
    {
        // This actually reads 4 bytes, starting at hasException.
        // Therefore, it also reads the isInterrupted flag, and triggers an exception on that.
        addCatchyJump(
                    branch32(NotEqual,
                             Address(EngineRegister, offsetof(EngineBase, hasException)),
                             TrustedImm32(0)));
    }

    void addCatchyJump(Jump j)
    {
        Q_ASSERT(j.isSet());
        catchyJumps.push_back(j);
    }

    void generateFunctionEntry()
    {
        generatePlatformFunctionEntry();
        loadPtr(Address(CppStackFrameRegister, offsetof(CppStackFrame, jsFrame)), JSStackFrameRegister);
        allocateStackSpace();
    }

    virtual void allocateStackSpace() {}

    void generateFunctionExit()
    {
        if (functionExit.isSet()) {
            jump(functionExit);
            return;
        }

        functionExit = label();
        freeStackSpace();
        generatePlatformFunctionExit();
    }

    virtual void freeStackSpace() {}

    void addLabelForOffset(int offset)
    {
        if (!labelForOffset.contains(offset))
            labelForOffset.insert(offset, label());
    }

    void addJumpToOffset(const Jump &jump, int offset)
    {
        jumpsToLink.push_back({ jump, offset });
    }

    void addEHTarget(const DataLabelPtr &label, int offset)
    {
        ehTargets.push_back({ label, offset });
    }

    void link(Function *function, const char *jitKind);

    Value constant(int idx) const
    { return constantTable[idx]; }

    // stuff for runtime calls
    void prepareCallWithArgCount(int argc);
    void storeInstructionPointer(int instructionOffset);
    void passAccumulatorAsArg(int arg);
    void pushAccumulatorAsArg(int arg);
    void passFunctionAsArg(int arg);
    void passEngineAsArg(int arg);
    void passJSSlotAsArg(int reg, int arg);
    void passAddressAsArg(Address addr, int arg);
    void passCppFrameAsArg(int arg);
    void passInt32AsArg(int value, int arg);
    void passPointerAsArg(void *ptr, int arg);
    void callRuntime(const void *funcPtr, const char *functionName = nullptr);
    void callRuntimeUnchecked(const void *funcPtr, const char *functionName = nullptr);
    void tailCallRuntime(const void *funcPtr, const char *functionName = nullptr);
    void setTailCallArg(RegisterID src, int arg);
    Address jsAlloca(int slotCount);
    void storeInt32AsValue(int srcInt, Address destAddr);

private:
    void passAccumulatorAsArg_internal(int arg, bool doPush);
    static Address argStackAddress(int arg);

private:
    const Value* constantTable;
    struct JumpTarget { JSC::MacroAssemblerBase::Jump jump; int offset; };
    std::vector<JumpTarget> jumpsToLink;
    struct ExceptionHanlderTarget { JSC::MacroAssemblerBase::DataLabelPtr label; int offset; };
    std::vector<ExceptionHanlderTarget> ehTargets;
    QHash<int, JSC::MacroAssemblerBase::Label> labelForOffset;
    QHash<const void *, const char *> functions;
    std::vector<Jump> catchyJumps;
    Label functionExit;

#ifndef QT_NO_DEBUG
    enum { NoCall = -1 };
    int remainingArgcForCall = NoCall;
#endif
    int argcOnStackForCall = 0;
};

} // JIT namespace
} // QV4 namespace

QT_END_NAMESPACE

#endif // QV4PLATFORMASSEMBLER_P_H
