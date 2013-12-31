// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CPUCOREBASE_H
#define _CPUCOREBASE_H

class CPUCoreBase
{
public:
	virtual ~CPUCoreBase() {}

	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void ClearCache() = 0;
	virtual void Run() = 0;
	virtual void SingleStep() = 0;
	virtual const char *GetName() = 0;
};

#endif
