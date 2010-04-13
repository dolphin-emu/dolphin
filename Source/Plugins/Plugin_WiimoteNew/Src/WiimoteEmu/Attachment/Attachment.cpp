
#include "Attachment.h"

namespace WiimoteEmu
{

// Extension device IDs to be written to the last bytes of the extension reg
static const u8 gh3glp_id[]		= { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x03 };
static const u8 ghwtdrums_id[]	= { 0x01, 0x00, 0xa4, 0x20, 0x01, 0x03 };
// The id for nothing inserted
static const u8 nothing_id[]	= { 0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e };
// The id for a partially inserted extension
static const u8 partially_id[]	= { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };

Attachment::Attachment( const char* const _name ) : name( _name )
{
	reg.resize( WIIMOTE_REG_EXT_SIZE, 0);
}

None::None() : Attachment( "None" )
{
	// set up register
	memcpy( &reg[0xfa], nothing_id, sizeof(nothing_id) );
}

std::string Attachment::GetName() const
{
	return name;
}

}

void ControllerEmu::Extension::GetState( u8* const data )
{
	((WiimoteEmu::Attachment*)attachments[ active_extension ])->GetState( data );
}
