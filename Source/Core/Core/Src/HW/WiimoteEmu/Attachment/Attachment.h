#ifndef _ATTACHMENT_EMU_H_
#define _ATTACHMENT_EMU_H_

#include "ControllerEmu.h"
#include "../WiimoteEmu.h"

namespace WiimoteEmu
{

class Attachment : public ControllerEmu
{
public:
	Attachment( const char* const _name );

	virtual void GetState( u8* const data, const bool focus = true ) {}
	std::string GetName() const;

	const char*	const	name;
	std::vector<u8>		reg;
};

class None : public Attachment
{
public:
	None();
};

}

#endif
