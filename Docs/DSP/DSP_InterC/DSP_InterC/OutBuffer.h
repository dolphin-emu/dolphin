// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

namespace OutBuffer
{
	void Init();
	
	void Add(const char* _fmt, ...);
	void AddCode(const char* _fmt, ...);

	const char* GetRegName(uint16 reg);
	const char* GetMemName(uint16 addr);
}





// TODO: reference additional headers your program requires here
