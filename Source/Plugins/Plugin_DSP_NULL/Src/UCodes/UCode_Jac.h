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

#ifndef _UCODE_JAC
#define _UCODE_JAC

#include "UCodes.h"

class CUCode_Jac
	: public IUCode
{
	private:

		enum EDSP_Codes
		{
			DSP_INIT   = 0xDCD10000,
			DSP_RESUME = 0xDCD10001,
			DSP_YIELD  = 0xDCD10002,
			DSP_DONE   = 0xDCD10003,
			DSP_SYNC   = 0xDCD10004,
			DSP_UNKN   = 0xDCD10005,
		};

		// List in progre
		bool m_bListInProgress;
		int m_numSteps;
		int m_step;
		u8 m_Buffer[1024];
		void ExecuteList();


		u32 m_readOffset;

		u8 Read8()
		{
			return(m_Buffer[m_readOffset++]);
		}


		u16 Read16()
		{
			u16 res = *(u16*)&m_Buffer[m_readOffset];
			m_readOffset += 2;
			return(res);
		}


		u32 Read32()
		{
			u32 res = *(u32*)&m_Buffer[m_readOffset];
			m_readOffset += 4;
			return(res);
		}


	public:

		// constructor
		CUCode_Jac(CMailHandler& _rMailHandler);

		// destructor
		virtual ~CUCode_Jac();

		// handleMail
		void HandleMail(u32 _uMail);

		void Update();
};

#endif

