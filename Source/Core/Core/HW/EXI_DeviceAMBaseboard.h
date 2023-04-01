// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/EXI_Device.h"

class PointerWrap;

class CEXIAMBaseboard : public IEXIDevice
{
public:
	CEXIAMBaseboard();

	void SetCS(int _iCS) override;
	bool IsPresent() const override;
	bool IsInterruptSet() override;
	void DoState(PointerWrap &p) override;

private:
	void TransferByte(u8& _uByte) override;
	int m_position;
	bool m_have_irq;
	unsigned char m_command[4];
};
