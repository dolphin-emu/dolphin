// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/PPCAnalyst.h"

namespace Jit64Reg
{

static const int NUMXREGS = 16;

using namespace Gen;

typedef size_t reg_t;

enum BindMode
{
	// Loads a register with the contents of this register, does not mark it dirty
	Read,
	// Allocates a register without loading the guest register, marks it dirty
	Write,
	// Loads the guest register into a native register, marks it dirty
	ReadWrite,
	// Begins a transaction on this register that will either be rolled back or committed,
	// depending on the method called on Registers. This will implicitly bind the guest
	// register to a native register for the entire scope of the transaction. Attempting to
	// flush, sync or rebind this register before rollback/commit is an error.
	WriteTransaction,
	// Pre-loads a register, does marking it dirty
	Preload,
	// Asserts that the register is already loaded -- doesn't load or mark it as dirty
	Reuse,
};

enum Type
{
	// PPC GPR
	GPR,
	// PPC FPU
	FPU,
};

enum AnyType
{
	PPC,
	X64,
	Imm,
};

enum XLock
{
	// This register is free for use
	Free,
	// This register has been borrowed as a scratch register
	Borrowed,
	// This register is bound to a PPC register
	Bound,
};

enum FlushMode
{
	FlushAll,
	FlushMaintainState
};

static const std::vector<X64Reg> GPR_ALLOCATION_ORDER =
{{
	// R12, when used as base register, for example in a LEA, can generate bad code! Need to look into this.
#ifdef _WIN32
	RSI, RDI, R13, R14, R15, R8, R9, R10, R11, R12, RCX
#else
	R12, R13, R14, R15, RSI, RDI, R8, R9, R10, R11, RCX
#endif
}};

static const std::vector<X64Reg> GPR_SCRATCH_ALLOCATION_ORDER =
{{
	RSCRATCH, RSCRATCH2
}};

static const std::vector<X64Reg> FPU_ALLOCATION_ORDER =
{{
	XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5
}};

static const std::vector<X64Reg> FPU_SCRATCH_ALLOCATION_ORDER =
{{
	XMM0, XMM1
}};

// Forward
template <Type T>
class RegisterClass;
template <Type T>
class RegisterClassBase;
class Registers;
class PPC;
class Tx;
class Imm;
template <Type T>
class Native;
template <Type T>
class Any;

template <Type T>
class Any
{
	friend class Registers;
	friend class Native<T>;
	friend class RegisterClass<T>;
	friend class RegisterClassBase<T>;

protected:
	RegisterClassBase<T>* m_reg;

private:
	bool m_realized = false;
	AnyType m_type;
	u32 m_val;

	Any(RegisterClassBase<T>* reg, AnyType type, u32 val): m_reg(reg), m_type(type), m_val(val) {};

	// A lock is only realized when a caller attempts to cast it to an OpArg or bind it
	void RealizeLock();

public:
	Any(Any<T>&& other) = default;
	~Any() { Unlock(); }
	Any(const Any<T>& other): m_reg(other.m_reg), m_type(other.m_type), m_val(other.m_val) {}
    Any& operator=(const Any<T>& other)
    {
 		_assert_msg_(DYNA_REC, 0, "TODO operator=");
   		return *this;
    }

	bool IsImm()
	{
		RealizeLock();
		return m_type == Imm;
	}

	bool IsZero()
	{
		RealizeLock();
		return m_type == Imm && m_val == 0;
	}

	u32 Imm32()
	{
		RealizeLock();
		_assert_msg_(DYNA_REC, m_type == Imm, "Not an immediate");
		return (u32)m_val;
	}

	s32 SImm32()
	{
		RealizeLock();
		_assert_msg_(DYNA_REC, m_type == Imm, "Not an immediate");
		return (s32)m_val;
	}

	// Returns the guest register represented by this class.
	u32 Register()
	{
		RealizeLock();
		_assert_msg_(DYNA_REC, m_type == PPC, "Not a register");
		return m_val;
	}

	void SetImm32(u32 imm);

	// If this value is an immediate, places it into a register (if there is room) or back
	// into its long-term memory location. If placed in a register, the register is not
	// marked as dirty.
	void RealizeImmediate();

	// If this value is not an immediate, loads it into a register. 
	void LoadIfNotImmediate();

	// Sync the current value to the default location but otherwise doesn't move the
	// register.
	OpArg Sync();

	// True if this register is available in a native register
	bool IsRegBound();

	// Forces this register into a native register. While the binding is in scope, it is an 
	// error to access this register's information.
	Native<T> Bind(BindMode mode);

	// Shortcut for common bind pattern
	Native<T> BindWriteAndReadIf(bool cond) { return Bind(cond ? ReadWrite : Write); }

	// Any register, bound or not, can be used as an OpArg. If not bound, this will return
	// a pointer into PPCSTATE.
	virtual operator Gen::OpArg();

	void Unlock();
};

// A guest register that has been bound to a native.
template <Type T>
class Native final : public Any<T>
{
	friend class Registers;
	friend class Any<T>;
	friend class RegisterClass<T>;
	friend class RegisterClassBase<T>;

private:
	Gen::X64Reg m_xreg;

	Native(RegisterClassBase<T>* reg, bool preload, AnyType type, Gen::X64Reg xreg): Any<T>(reg, type, xreg), m_xreg(xreg)
	{
		if (preload)
			this->RealizeLock();
	}

public:
	Native(const Native& other) = default;
	Native(Native&& other) = default;
	~Native() { }
    Native& operator=(const Native& other)
    {
 		_assert_msg_(DYNA_REC, 0, "TODO operator=");
    	// // Keep the ref count up just in case we are copying a reg over itself 
    	// Native<T> copy(*this);

    	// Unlock();
    	// m_xreg = other.m_xreg;
    	// this->m_type = other.m_type;
    	// this->m_val = other.m_val;
    	// Lock();

    	return *this;
    }
	
	virtual operator Gen::OpArg();
	operator Gen::X64Reg();

	virtual void Unlock();
};

template<Type T>
class RegisterClassBase
{
	friend class Any<T>;
	friend class Native<T>;

	struct PPCCachedReg
	{
		Gen::OpArg location;
		bool away;  // value not in source register

		// A locked register is in use by the currently-generating opcode and cannot
		// be spilled back to memory. This is a reference count.
		int locked;
	};

	struct X64CachedReg
	{
		size_t ppcReg;
		bool dirty;
		XLock lock;
	};

	// One array for GPR, one for FPU
protected:
	Gen::XEmitter* m_emit;
	JitBase* m_jit;
	std::array<PPCCachedReg, 32 + 32> m_regs;
	std::array<X64CachedReg, NUMXREGS> m_xregs;

	virtual const std::vector<X64Reg>& GetAllocationOrder() const = 0;
	virtual const std::vector<X64Reg>& GetScratchAllocationOrder() const = 0;
	virtual void LoadRegister(X64Reg newLoc, const OpArg& location) = 0;
	virtual void StoreRegister(const OpArg& newLoc, const OpArg& location) = 0;
	virtual OpArg GetDefaultLocation(reg_t reg) const = 0;
	virtual BitSet32 GetRegUtilization() const = 0;
	virtual BitSet32 GetRegsIn(int i) const = 0;

	void BindToRegister(reg_t preg, bool doLoad, bool makeDirty);
	void CheckUnlocked();
	BitSet32 CountRegsIn(reg_t preg, u32 lookahead);
	void Flush();
	X64Reg GetFreeXReg(bool scratch);
	void Init();
	int NumFreeRegisters();
	float ScoreRegister(X64Reg xr);
	void StoreFromRegister(size_t preg, FlushMode mode);

	RegisterClassBase(Gen::XEmitter* emit, JitBase* jit): m_emit(emit), m_jit(jit) {}
	void CopyFrom(RegisterClassBase<T>& other)
	{
		other.CheckUnlocked();
		m_regs = other.m_regs;
		m_xregs = other.m_xregs;
	}

public:
	// Borrows a host register. If 'which' is omitted, an appropriate one is chosen.
	// Note that the register that is actually borrowed is indeterminate up until
	// the actual point where a method is called that would require it to be
	// determined. It is an error to borrow a register and never access it.
	Native<T> Borrow(Gen::X64Reg which = Gen::INVALID_REG);

	// Locks a target register for the duration of this scope. This register will
	// no longer be implicitly moved until the end of the scope. Note that the lock
	// is advisory until a method that requires current state of the register to be
	// accessed is called and a register may be moved up until that point.
	// It is an error to lock a register and never access it.
	Any<T> Lock(size_t register);

	void BindBatch(BitSet32 regs);
	void FlushBatch(BitSet32 regs);

	BitSet32 InUse() const;
};

template<Type T>
class RegisterClass
{
};

template<>
class RegisterClass<FPU>: public RegisterClassBase<FPU>
{
	friend class Registers;

protected:
	virtual const std::vector<X64Reg>& GetAllocationOrder() const { return FPU_ALLOCATION_ORDER; }
	virtual const std::vector<X64Reg>& GetScratchAllocationOrder() const { return FPU_SCRATCH_ALLOCATION_ORDER; }
	virtual void LoadRegister(X64Reg newLoc, const OpArg& location)
	{
		m_emit->MOVAPD(newLoc, location);
	}
	virtual void StoreRegister(const OpArg& newLoc, const OpArg& location)
	{
		m_emit->MOVAPD(newLoc, location.GetSimpleReg());
	}
	virtual OpArg GetDefaultLocation(reg_t reg) const { return PPCSTATE(ps[reg][0]); }
	virtual BitSet32 GetRegUtilization() const { return m_jit->js.op->fprInXmm; }
	virtual BitSet32 GetRegsIn(int i) const { return m_jit->js.op[i].fregsIn; }

	RegisterClass(Gen::XEmitter* emit, JitBase* jit): RegisterClassBase(emit, jit) {}
};

typedef RegisterClass<FPU> FPURegisters;

template<>
class RegisterClass<GPR>: public RegisterClassBase<GPR>
{
	friend class Registers;

protected:
	virtual const std::vector<X64Reg>& GetAllocationOrder() const { return GPR_ALLOCATION_ORDER; }
	virtual const std::vector<X64Reg>& GetScratchAllocationOrder() const { return GPR_SCRATCH_ALLOCATION_ORDER; }
	virtual void LoadRegister(X64Reg newLoc, const OpArg& location)
	{
		m_emit->MOV(32, R(newLoc), location);
	}
	virtual void StoreRegister(const OpArg& newLoc, const OpArg& location)
	{
		m_emit->MOV(32, newLoc, location);
	}
	virtual OpArg GetDefaultLocation(reg_t reg) const { return PPCSTATE(gpr[reg]); }
	virtual BitSet32 GetRegUtilization() const { return m_jit->js.op->gprInReg; }
	virtual BitSet32 GetRegsIn(int i) const { return m_jit->js.op[i].regsIn; };

	RegisterClass(Gen::XEmitter* emit, JitBase* jit): RegisterClassBase(emit, jit) {}

public:
	// A virtual register that contains zero and cannot be updated
	Any<GPR> Zero() { return Imm32(0); }

	// A virtual register that contains an immediate and cannot be updated
	Any<GPR> Imm32(u32 immediate);
};

typedef RegisterClass<GPR> GPRRegisters;


class Registers
{
	template<Type T>
	friend class Any;
	template<Type T>
	friend class RegisterClass;
	template<Type T>
	friend class RegisterClassBase;

private:
	Gen::XEmitter* m_emit;
	JitBase* m_jit;

	void BindToRegister(Type type, reg_t preg, bool doLoad, bool makeDirty);
	float ScoreRegister(Type type, X64Reg xr);

public:
	FPURegisters fpu{m_emit, m_jit};
	GPRRegisters gpr{m_emit, m_jit};

	void Init();

	Registers(Gen::XEmitter* emit, JitBase* jit): m_emit(emit), m_jit(jit) {}

	// Create an independent copy of the register cache state for a branch
	Registers Branch();

	int SanityCheck() const;

	void Flush() { gpr.Flush(); fpu.Flush(); }

	void Commit();
	void Rollback();
};

};
