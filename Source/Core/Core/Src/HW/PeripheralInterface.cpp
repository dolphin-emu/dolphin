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
#include <stdio.h>

#include "../PowerPC/PowerPC.h"

#include "../HW/CPU.h"
#include "PeripheralInterface.h"
#include "GPFifo.h"

u32 volatile CPeripheralInterface::m_InterruptMask;
u32 volatile CPeripheralInterface::m_InterruptCause;

// addresses for CPU fifo accesses
u32 CPeripheralInterface::Fifo_CPUBase;
u32 CPeripheralInterface::Fifo_CPUEnd;
u32 CPeripheralInterface::Fifo_CPUWritePointer;

void CPeripheralInterface::Init()
{
	m_InterruptMask = 0;
	m_InterruptCause = 0;

	Fifo_CPUBase = 0;
	Fifo_CPUEnd = 0;
	Fifo_CPUWritePointer = 0;

	m_InterruptCause |= INT_CAUSE_RST_BUTTON;		// Reset button state
}

void CPeripheralInterface::Read32(u32& _uReturnValue, const u32 _iAddress)
{
	//LOG(PERIPHERALINTERFACE, "(r32) 0x%08x", _iAddress);

	switch(_iAddress & 0xFFF) 
	{
	case PI_INTERRUPT_CAUSE:
		_uReturnValue = m_InterruptCause;
		return;
	
	case PI_INTERRUPT_MASK:
		_uReturnValue = m_InterruptMask;
		return;

	case PI_FIFO_BASE:
		LOG(PERIPHERALINTERFACE,"read cpu fifo base, value = %08x",Fifo_CPUBase);
		_uReturnValue = Fifo_CPUBase;
		return;

	case PI_FIFO_END:
		LOG(PERIPHERALINTERFACE,"read cpu fifo end, value = %08x",Fifo_CPUEnd);
		_uReturnValue = Fifo_CPUEnd;
		return;

	case PI_FIFO_WPTR:
		LOG(PERIPHERALINTERFACE,"read writepointer, value = %08x",Fifo_CPUWritePointer);
		_uReturnValue = Fifo_CPUWritePointer;  //really writes in 32-byte chunks

		// Monk's gcube does some crazy align trickery here.
		return;

	case PI_RESET_CODE:
		_uReturnValue = 0x80000000;
		return;

	case PI_MB_REV:
		_uReturnValue = 0x20000000; // HW2 production board
		return;
		
	default:
		LOG(PERIPHERALINTERFACE,"!!!!Unknown write!!!! 0x%08x", _iAddress);
		break;
	}
	
	_uReturnValue = 0xAFFE0000;
}

void CPeripheralInterface::Write32(const u32 _uValue, const u32 _iAddress)
{
	LOG(PERIPHERALINTERFACE, "(w32) 0x%08x @ 0x%08x", _uValue, _iAddress);
	switch(_iAddress & 0xFFF) 
	{
	case PI_INTERRUPT_CAUSE:
		m_InterruptCause &= ~_uValue; //writes turns them off
		UpdateException();
		return;

	case PI_INTERRUPT_MASK: 
		m_InterruptMask = _uValue;
		LOG(PERIPHERALINTERFACE,"New Interrupt mask: %08x",m_InterruptMask);
		UpdateException();
		return;
	
	case PI_FIFO_BASE:
		Fifo_CPUBase = _uValue & 0xFFFFFFE0;
		LOG(PERIPHERALINTERFACE,"Fifo base = %08x", _uValue);
		break;

	case PI_FIFO_END:
		Fifo_CPUEnd = _uValue & 0xFFFFFFE0;
		LOG(PERIPHERALINTERFACE,"Fifo end = %08x", _uValue);
		break;

	case PI_FIFO_WPTR:
		Fifo_CPUWritePointer = _uValue & 0xFFFFFFE0;		
		LOG(PERIPHERALINTERFACE,"Fifo writeptr = %08x", _uValue);
		break;

    case PI_FIFO_RESET:
        // Fifo_CPUWritePointer = Fifo_CPUBase; ??
		// PanicAlert("Unknown write to PI_FIFO_RESET (%08x)", _uValue);
        break;

	case PI_RESET_CODE:
        {
            LOG(PERIPHERALINTERFACE,"PI Reset = %08x ???", _uValue);

            if ((_uValue != 0x80000001) && (_uValue != 0x80000005)) // DVDLowReset 
            {
				switch (_uValue) {
				case 3:
					PanicAlert("Game wants to go to memory card manager. Since BIOS is being HLE:d - can't do that.\n"
						       "We might pop up a fake memcard manager here and then reset the game in the future :)\n");
					break;
				default:
					{
					TCHAR szTemp[256];
					sprintf(szTemp, "Game wants to reset the machine. PI_RESET_CODE: (%08x)", _uValue);
					PanicAlert(szTemp);
					}
					break;
				}
            }
        }
		break;

	default:
		LOG(PERIPHERALINTERFACE,"!!!!Unknown write!!!! 0x%08x", _iAddress);
		PanicAlert("Unknown write to PI");
		break;
	}
}

void CPeripheralInterface::UpdateException()
{
	if ((m_InterruptCause & m_InterruptMask) != 0)
		PowerPC::ppcState.Exceptions |= EXCEPTION_EXTERNAL_INT;
	else
		PowerPC::ppcState.Exceptions &= ~EXCEPTION_EXTERNAL_INT;
}

const char *CPeripheralInterface::Debug_GetInterruptName(InterruptCause _causemask)
{
	switch (_causemask)
	{
	case INT_CAUSE_ERROR:			return "INT_CAUSE_ERROR";
	case INT_CAUSE_DI:				return "INT_CAUSE_DI";
	case INT_CAUSE_RSW:				return "INT_CAUSE_RSW";
	case INT_CAUSE_SI:				return "INT_CAUSE_SI";
	case INT_CAUSE_EXI:				return "INT_CAUSE_EXI";
	case INT_CAUSE_AUDIO:			return "INT_CAUSE_AUDIO";
	case INT_CAUSE_DSP:				return "INT_CAUSE_DSP";
	case INT_CAUSE_MEMORY:			return "INT_CAUSE_MEMORY";
	case INT_CAUSE_VI:				return "INT_CAUSE_VI";
	case INT_CAUSE_PE_TOKEN:		return "INT_CAUSE_PE_TOKEN";
	case INT_CAUSE_PE_FINISH:		return "INT_CAUSE_PE_FINISH";
	case INT_CAUSE_CP:				return "INT_CAUSE_CP";
	case INT_CAUSE_DEBUG:			return "INT_CAUSE_DEBUG";
    case INT_CAUSE_WII_IPC:			return "INT_CAUSE_WII_IPC";
	case INT_CAUSE_HSP:				return "INT_CAUSE_HSP";
	case INT_CAUSE_RST_BUTTON:      return "INT_CAUSE_RST_BUTTON";
	}

	return "!!! ERROR-unknown Interrupt !!!";
}

void CPeripheralInterface::SetInterrupt(InterruptCause _causemask, bool _bSet)
{
	//TODO(ector): add sanity check that current thread id is cpu thread

    if (_bSet && !(m_InterruptCause & (u32)_causemask))
    {
        LOG(PERIPHERALINTERFACE,"Setting Interrupt %s (%s)",Debug_GetInterruptName(_causemask), "set");
    }

    if (!_bSet && (m_InterruptCause & (u32)_causemask))
    {
        LOG(PERIPHERALINTERFACE,"Setting Interrupt %s (%s)",Debug_GetInterruptName(_causemask), "clear");
    }
	
	if (_bSet)
		m_InterruptCause |= (u32)_causemask;
	else
		m_InterruptCause &= ~(u32)_causemask;   // is there any reason to have this possibility?
												// F|RES: i think the hw devices reset the interrupt in the PI to 0 
												// if the interrupt cause is eliminated. that isnt done by software (afaik)
	UpdateException();
}

void CPeripheralInterface::SetResetButton(bool _bSet)
{
	if (_bSet)
		m_InterruptCause &= ~INT_CAUSE_RST_BUTTON;
	else
		m_InterruptCause |= INT_CAUSE_RST_BUTTON;			
}

