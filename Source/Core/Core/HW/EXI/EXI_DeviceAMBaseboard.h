// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#include "Common/IOFile.h"

#ifndef _EXIDEVICE_AMBASEBOARD_H
#define _EXIDEVICE_AMBASEBOARD_H


class PointerWrap;
class CEXIAMBaseboard : public ExpansionInterface::IEXIDevice
{
public:
	CEXIAMBaseboard();

	virtual void SetCS(int _iCS);
	virtual bool IsPresent();
	virtual bool IsInterruptSet();
	virtual void DoState(PointerWrap &p);

	~CEXIAMBaseboard();

private:
	void TransferByte(u8& _uByte);
	int m_position;
	bool m_have_irq;
	u32 m_irq_timer;
	u32 m_irq_status;
	unsigned char m_command[4];
	unsigned short m_backoffset;
	File::IOFile *m_backup;
};

#endif
