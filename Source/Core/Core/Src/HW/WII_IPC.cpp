// Copyright (C) 2003-2008 Dolphin Project.

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
#include "PeripheralInterface.h"

#include "../IPC_HLE/WII_IPC_HLE.h"
#include "WII_IPC.h"

namespace WII_IPCInterface
{

enum
{
    IPC_COMMAND_REGISTER    = 0x00,
    IPC_CONTROL_REGISTER    = 0x04,
    IPC_REPLY_REGISTER      = 0x08,
    IPC_STATUS_REGISTER     = 0x30,
    IPC_CONFIG_REGISTER     = 0x34,    
	IPC_SENSOR_BAR_POWER_REGISTER = 0xC0
};

union UIPC_Control
{
    u32 Hex;
    struct  
    {		
        unsigned ExecuteCmd     : 1;
        unsigned AckReady       : 1;
        unsigned ReplyReady     : 1;
        unsigned Relaunch       : 1;
        unsigned unk5           : 1;
        unsigned unk6           : 1;
        unsigned pad            : 26;
    };
    UIPC_Control(u32 _Hex = 0) {Hex = _Hex;}
};

union UIPC_Status
{
    u32 Hex;
    struct  
    {		
        unsigned 				:  30;
        unsigned INTERRUPT		:	1;   // 0x40000000
        unsigned				:	1;
    };
    UIPC_Status(u32 _Hex = 0) {Hex = _Hex;}
};

union UIPC_Config
{
	u32 Hex;
	struct  
	{		
		unsigned 				:  30;
		unsigned INT_MASK		:	1;   // 0x40000000
		unsigned				:	1;
	};
	UIPC_Config(u32 _Hex = 0) 
    {
        Hex = _Hex;
        INT_MASK = 1;
    }
};

// STATE_TO_SAVE
UIPC_Status g_IPC_Status;
UIPC_Config g_IPC_Config;
UIPC_Control g_IPC_Control;

u32 g_Address = 0;
u32 g_Reply = 0;
u32 g_SensorBarPower = 0;

void DoState(ChunkFile &f)
{
	f.Descend("WIPC");
	f.Do(g_IPC_Status);
	f.Do(g_IPC_Config);
	f.Do(g_IPC_Control);
	f.Do(g_Address);
	f.Do(g_Reply);
	f.Do(g_SensorBarPower);
	f.Ascend();
}

void UpdateInterrupts();

// Init
void Init()
{
    g_Address = 0;
    g_Reply = 0;
    g_SensorBarPower = 0;

    g_IPC_Status = UIPC_Status();
    g_IPC_Config = UIPC_Config();
    g_IPC_Control = UIPC_Control();
}

void Shutdown()
{
}

void HWCALL Read32(u32& _rReturnValue, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
	case IPC_CONTROL_REGISTER:	

        _rReturnValue = g_IPC_Control.Hex;

		LOG(WII_IPC, "IOP: Read32 from IPC_CONTROL_REGISTER(0x04) = 0x%08x", _rReturnValue);
		// CCPU::Break();

        // if ((REASON_REG & 0x14) == 0x14) CALL IPCReplayHanlder
        // if ((REASON_REG & 0x22) != 0x22) Jumps to the end

		break;

    case IPC_REPLY_REGISTER: // looks a little bit like a callback function
        _rReturnValue = g_Reply;
        LOG(WII_IPC, "IOP: Write32 to IPC_REPLAY_REGISTER(0x08) = 0x%08x ", _rReturnValue);
        break;

	case IPC_SENSOR_BAR_POWER_REGISTER:
		_rReturnValue = g_SensorBarPower;
		break;

	default:
		_dbg_assert_msg_(WII_IPC, 0, "IOP: Read32 from 0x%08x", _Address);
		break;
	}	
}

void HWCALL Write32(const u32 _Value, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
    // write only ??
	case IPC_COMMAND_REGISTER:					// __ios_Ipc2 ... a value from __responses is loaded
        {
            g_Address = _Value;
            LOG(WII_IPC, "IOP: Write32 to IPC_ADDRESS_REGISTER(0x00) = 0x%08x", g_Address);
        }
		break;

	case IPC_CONTROL_REGISTER:
        {
            LOG(WII_IPC, "IOP: Write32 to IPC_CONTROL_REGISTER(0x04) = 0x%08x (old: 0x%08x)", _Value, g_IPC_Control.Hex);

            UIPC_Control TempControl(_Value);
            _dbg_assert_msg_(WII_IPC, TempControl.pad == 0, "IOP: Write to UIPC_Control.pad", _Address);


            if (TempControl.AckReady)       { g_IPC_Control.AckReady = 0; }
            if (TempControl.ReplyReady)     { g_IPC_Control.ReplyReady = 0; }
            if (TempControl.Relaunch)       { g_IPC_Control.Relaunch = 0; }        
            
            g_IPC_Control.unk5 = TempControl.unk5;
            g_IPC_Control.unk6 = TempControl.unk6;
            g_IPC_Control.pad = TempControl.pad;

			if (TempControl.ExecuteCmd)     
			{ 
				WII_IPC_HLE_Interface::AckCommand(g_Address);
   			}
        }
		break;  

    case IPC_STATUS_REGISTER:  // ACR REGISTER IT IS CALLED IN DEBUG
        {           
            UIPC_Status NewStatus(_Value);
            if (NewStatus.INTERRUPT) g_IPC_Status.INTERRUPT = 0;    // clear interrupt

            LOG(WII_IPC, "IOP: Write32 to IPC_STATUS_REGISTER(0x30) = 0x%08x", _Value);
        }
        break;

	case IPC_CONFIG_REGISTER:	// __OSInterruptInit (0x40000000)
        {							
		    LOG(WII_IPC, "IOP: Write32 to IPC_CONFIG_REGISTER(0x33) = 0x%08x", _Value);
		    g_IPC_Config.Hex = _Value;
        }
		break;

	case IPC_SENSOR_BAR_POWER_REGISTER:
		g_SensorBarPower = _Value;
		break;

	default:
        {
		    _dbg_assert_msg_(WII_IPC, 0, "IOP: Write32 to 0x%08x", _Address);
        }
		break;
	}	

	//  update the interrupts
	UpdateInterrupts();
}

void UpdateInterrupts()
{
    if ((g_IPC_Control.AckReady == 1) ||
        (g_IPC_Control.ReplyReady == 1))
    {
        g_IPC_Status.INTERRUPT = 1;
    }

	// check if we have to generate an interrupt
	if (g_IPC_Config.INT_MASK & g_IPC_Status.INTERRUPT)
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_WII_IPC, true);
	}
	else
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_WII_IPC, false);
	}
}

bool IsReady()
{
	return ((g_IPC_Control.ReplyReady == 0) && (g_IPC_Control.AckReady == 0) && (g_IPC_Status.INTERRUPT == 0));
}

void GenerateAck(u32 _AnswerAddress)
{
    g_Reply = _AnswerAddress;
    g_IPC_Control.AckReady = 1;

    UpdateInterrupts();
}

void GenerateReply(u32 _AnswerAddress)
{
	g_Reply = _AnswerAddress;
	g_IPC_Control.ReplyReady = 1;

    UpdateInterrupts();
}

} // end of namespace IPC

