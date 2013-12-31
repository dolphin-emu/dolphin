// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Attachment.h"

namespace WiimoteEmu
{

// Extension device IDs to be written to the last bytes of the extension reg
// The id for nothing inserted
static const u8 nothing_id[]	= { 0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e };
// The id for a partially inserted extension
static const u8 partially_id[]	= { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };

Attachment::Attachment( const char* const _name, WiimoteEmu::ExtensionReg& _reg )
	: name( _name ), reg( _reg )
{
	memset(id, 0, sizeof(id));
	memset(calibration, 0, sizeof(calibration));
}

None::None( WiimoteEmu::ExtensionReg& _reg ) : Attachment( "None", _reg )
{
	// set up register
	memcpy(&id, nothing_id, sizeof(nothing_id));
}

std::string Attachment::GetName() const
{
	return name;
}

void Attachment::Reset()
{
	// set up register
	memset( &reg, 0, WIIMOTE_REG_EXT_SIZE );
	memcpy( &reg.constant_id, id, sizeof(id) );
	memcpy( &reg.calibration, calibration, sizeof(calibration) );
}

}

void ControllerEmu::Extension::GetState( u8* const data, const bool focus )
{
	((WiimoteEmu::Attachment*)attachments[ active_extension ])->GetState( data, focus );
}
