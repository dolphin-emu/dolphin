// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <map>

#include "Common.h"
#include "ChunkFile.h"

#include "CPU.h"
#include "Memmap.h"
#include "ProcessorInterface.h"

#include "../IPC_HLE/WII_IPC_HLE.h"
#include "WII_IPC.h"


// This is the intercommunication between ARM and PPC. Currently only PPC actually uses it, because of the IOS HLE
// How IOS uses IPC:
// X1 Execute command: a new pointer is available in HW_IPC_PPCCTRL
// X2 Reload (a new IOS is being loaded, old one doesn't need to reply anymore)
// Y1 Command executed and reply available in HW_IPC_ARMMSG
// Y2 Command acknowledge
// ppc_msg is a pointer to 0x40byte command structure
// arm_msg is, similarly, starlet's response buffer*

namespace WII_IPCInterface
{

enum
{
	IPC_PPCMSG	= 0x00,
	IPC_PPCCTRL	= 0x04,
	IPC_ARMMSG	= 0x08,
	IPC_ARMCTRL	= 0x0c,

	PPC_IRQFLAG	= 0x30,
	PPC_IRQMASK	= 0x34,
	ARM_IRQFLAG	= 0x38,
	ARM_IRQMASK	= 0x3c,

	GPIOB_OUT	= 0xc0 // sensor bar power flag??
};

struct CtrlRegister
{
	u8 X1	: 1;
	u8 X2	: 1;
	u8 Y1	: 1;
	u8 Y2	: 1;
	u8 IX1	: 1;
	u8 IX2	: 1;
	u8 IY1	: 1;
	u8 IY2	: 1;

	CtrlRegister() { X1 = X2 = Y1 = Y2 = IX1 = IX2 = IY1 = IY2 = 0; }

	inline u8 ppc() { return (IY2<<5)|(IY1<<4)|(X2<<3)|(Y1<<2)|(Y2<<1)|X1; }
	inline u8 arm() { return (IX2<<5)|(IX1<<4)|(Y2<<3)|(X1<<2)|(X2<<1)|Y1; }

	inline void ppc(u32 v) {
		X1 = v & 1;
		X2 = (v >> 3) & 1;
		if ((v >> 2) & 1) Y1 = 0;
		if ((v >> 1) & 1) Y2 = 0;
		IY1 = (v >> 4) & 1;
		IY2 = (v >> 5) & 1;
	}

	inline void arm(u32 v) {
		Y1 = v & 1;
		Y2 = (v >> 3) & 1;
		if ((v >> 2) & 1) X1 = 0;
		if ((v >> 1) & 1) X2 = 0;
		IX1 = (v >> 4) & 1;
		IX2 = (v >> 5) & 1;
	}
};

// STATE_TO_SAVE
u32 ppc_msg;
u32 arm_msg;
CtrlRegister ctrl;

u32 ppc_irq_flags;
u32 ppc_irq_masks;
u32 arm_irq_flags;
u32 arm_irq_masks;

u32 sensorbar_power; // do we need to care about this?

// not actual registers:
bool cmd_active;
u32 g_ReplyHead;
u32 g_ReplyTail;
u32 g_ReplyNum;
u32 g_ReplyFifo[REPLY_FIFO_DEPTH];

void DoState(PointerWrap &p)
{
	p.Do(ppc_msg);
	p.Do(arm_msg);
	p.Do(ctrl);
	p.Do(ppc_irq_flags);
	p.Do(ppc_irq_masks);
	p.Do(arm_irq_flags);
	p.Do(arm_irq_masks);
	p.Do(sensorbar_power);
	p.Do(cmd_active);
	p.Do(g_ReplyHead);
	p.Do(g_ReplyTail);
	p.Do(g_ReplyNum);
	p.DoArray(g_ReplyFifo, REPLY_FIFO_DEPTH);
}

// Init
void Init()
{
	cmd_active = false;
	ctrl = CtrlRegister();
	ppc_msg =
	arm_msg =

	ppc_irq_flags =
	ppc_irq_masks =
	arm_irq_flags =
	arm_irq_masks =

	sensorbar_power =

	g_ReplyHead =
	g_ReplyTail =
	g_ReplyNum = 0;
	memset(g_ReplyFifo, 0, REPLY_FIFO_DEPTH);

	ppc_irq_masks |= INT_CAUSE_IPC_BROADWAY;
}

void Reset()
{
	INFO_LOG(WII_IPC, "Resetting ...");
	Init();
	WII_IPC_HLE_Interface::Reset();
}

void Shutdown()
{
}

void Read32(u32& _rReturnValue, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
	case IPC_PPCCTRL:	
		_rReturnValue = ctrl.ppc();
		INFO_LOG(WII_IPC, "r32 IPC_PPCCTRL %03x [R:%i A:%i E:%i]",
			ctrl.ppc(), ctrl.Y1, ctrl.Y2, ctrl.X1);

		// if ((REASON_REG & 0x14) == 0x14) CALL IPCReplayHandler
		// if ((REASON_REG & 0x22) != 0x22) Jumps to the end
		break;

	case IPC_ARMMSG:	// looks a little bit like a callback function
		_rReturnValue = arm_msg;
		INFO_LOG(WII_IPC, "r32 IPC_ARMMSG %08x ", _rReturnValue);
		break;

	case GPIOB_OUT:
		_rReturnValue = sensorbar_power;
		break;

	default:
		_dbg_assert_msg_(WII_IPC, 0, "r32 from %08x", _Address);
		break;
	}
}

void Write32(const u32 _Value, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
	case IPC_PPCMSG:	// __ios_Ipc2 ... a value from __responses is loaded
		{
			ppc_msg = _Value;
			INFO_LOG(WII_IPC, "IPC_PPCMSG = %08x", ppc_msg);
		}
		break;

	case IPC_PPCCTRL:
		{
			ctrl.ppc(_Value);
			INFO_LOG(WII_IPC, "w32 %08x IPC_PPCCTRL = %03x [R:%i A:%i E:%i]",
				_Value, ctrl.ppc(), ctrl.Y1, ctrl.Y2, ctrl.X1);
			if (ctrl.X1) // seems like we should just be able to use X1 directly, but it doesn't work...why?!
				cmd_active = true;
		}
		break;  

	case PPC_IRQFLAG:	// ACR REGISTER IT IS CALLED IN DEBUG
		{
			ppc_irq_flags &= ~_Value;
			INFO_LOG(WII_IPC,  "w32 PPC_IRQFLAG %08x (%08x)", _Value, ppc_irq_flags);
		}
		break;

	case PPC_IRQMASK:	// __OSInterruptInit (0x40000000)
		{
			ppc_irq_masks = _Value;
			if (ppc_irq_masks & INT_CAUSE_IPC_BROADWAY) // wtf?
				Reset();
			INFO_LOG(WII_IPC, "w32 PPC_IRQMASK %08x", ppc_irq_masks);
		}
		break;

	case GPIOB_OUT:
		sensorbar_power = _Value;
		break;

	default:
		_dbg_assert_msg_(WII_IPC, 0, "w32 %08x @ %08x", _Value, _Address);
		break;
	}	

	UpdateInterrupts();
}

void UpdateInterrupts()
{
	if ((ctrl.Y1 & ctrl.IY1) || (ctrl.Y2 & ctrl.IY2))
	{
		ppc_irq_flags |= INT_CAUSE_IPC_BROADWAY;
	}

	if ((ctrl.X1 & ctrl.IX1) || (ctrl.X2 & ctrl.IX2))
	{
		ppc_irq_flags |= INT_CAUSE_IPC_STARLET;
	}

	// Generate interrupt on PI if any of the devices behind starlet have an interrupt and mask is set
	ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_WII_IPC, !!(ppc_irq_flags & ppc_irq_masks));
}

// The rest is for IOS HLE
bool IsReady()
{
	return ((ctrl.Y1 == 0) && (ctrl.Y2 == 0) && ((ppc_irq_flags & INT_CAUSE_IPC_BROADWAY) == 0));
}

u32 GetAddress()
{
	return (cmd_active ? ppc_msg : 0);
}

void GenerateAck()
{
	cmd_active = false;
	ctrl.Y2 = 1;
	UpdateInterrupts();
}

void GenerateReply(u32 _Address)
{
	arm_msg = _Address;
	ctrl.Y1 = 1;
	UpdateInterrupts();
}

void EnqReply(u32 _Address)
{
	if (g_ReplyNum < REPLY_FIFO_DEPTH)
	{
		g_ReplyFifo[g_ReplyTail++] = _Address;
		g_ReplyTail &= REPLY_FIFO_MASK;
		g_ReplyNum++;
	}
	else
	{
		ERROR_LOG(WII_IPC, "Reply FIFO is full, something must be wrong!");
		PanicAlert("WII_IPC: Reply FIFO is full, something must be wrong!");
	}
}

u32 DeqReply()
{
	u32 _Address;

	if (g_ReplyNum)
	{
		_Address = g_ReplyFifo[g_ReplyHead++];
		g_ReplyHead &= REPLY_FIFO_MASK;
		g_ReplyNum--;
	}
	else
	{
		_Address = NULL;
	}

	return _Address;
}

}
