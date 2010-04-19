
#include "Attachment/Classic.h"
#include "Attachment/Nunchuk.h"

#include "WiimoteEmu.h"
#include "WiimoteHid.h"

#include <Timer.h>
#include <Common.h>

// buttons

#define WIIMOTE_PAD_LEFT		0x01
#define WIIMOTE_PAD_RIGHT		0x02
#define WIIMOTE_PAD_DOWN		0x04
#define WIIMOTE_PAD_UP			0x08
#define WIIMOTE_PLUS 			0x10

#define WIIMOTE_TWO				0x0100
#define WIIMOTE_ONE				0x0200
#define WIIMOTE_B				0x0400
#define WIIMOTE_A				0x0800
#define WIIMOTE_MINUS	 		0x1000
#define	WIIMOTE_HOME			0x8000

namespace WiimoteEmu
{

/* An example of a factory default first bytes of the Eeprom memory. There are differences between
   different Wiimotes, my Wiimote had different neutral values for the accelerometer. */
static const u8 eeprom_data_0[] = {
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, 0x74, 0xD3,
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, 0x74, 0xD3,
	// Accelerometer neutral values
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E,
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E
};
static const u8 eeprom_data_16D0[] = {
	0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00,
	0x33, 0xCC, 0x44, 0xBB, 0x00, 0x00, 0x66, 0x99,
	0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13
};

// array of accel data to emulate shaking
const u8 shake_data[8] = { 0x80, 0x40, 0x01, 0x40, 0x80, 0xC0, 0xFF, 0xC0 };

const u16 button_bitmasks[] =
{
	WIIMOTE_A, WIIMOTE_B, WIIMOTE_ONE, WIIMOTE_TWO, WIIMOTE_MINUS, WIIMOTE_PLUS, WIIMOTE_HOME
};

const u16 dpad_bitmasks[] =
{
	WIIMOTE_PAD_UP, WIIMOTE_PAD_DOWN, WIIMOTE_PAD_LEFT, WIIMOTE_PAD_RIGHT
};
const u16 dpad_sideways_bitmasks[] =
{
	WIIMOTE_PAD_RIGHT, WIIMOTE_PAD_LEFT, WIIMOTE_PAD_UP, WIIMOTE_PAD_DOWN
};

const char* const named_buttons[] =
{
	"A",
	"B",
	"One",
	"Two",
	"Minus",
	"Plus",
	"Home",
};

void Wiimote::Reset()
{
	m_reporting_mode = WM_REPORT_CORE;
	// i think these two are good
	m_reporting_channel = 0;
	m_reporting_auto = false;

	// will make the first Update() call send a status request
	// the first call to RequestStatus() will then set up the status struct extension bit
	m_extension->active_extension = -1;

	// eeprom
	memset( m_eeprom, 0, sizeof(m_eeprom) );
	// calibration data
	memcpy( m_eeprom, eeprom_data_0, sizeof(eeprom_data_0) );
	// dunno what this is for, copied from old plugin
	memcpy( m_eeprom + 0x16D0, eeprom_data_16D0, sizeof(eeprom_data_16D0) );

	// set up the register
	m_register.clear();
	m_register[0xa20000].resize(WIIMOTE_REG_SPEAKER_SIZE,0);
	m_register[0xa40000].resize(WIIMOTE_REG_EXT_SIZE,0);
	m_register[0xa60000].resize(WIIMOTE_REG_EXT_SIZE,0);
	m_register[0xB00000].resize(WIIMOTE_REG_IR_SIZE,0);

	//m_reg_speaker		= &m_register[0xa20000][0];
	m_reg_ext			= &m_register[0xa40000][0];
	//m_reg_motion_plus	= &m_register[0xa60000][0];
	//m_reg_ir			= &m_register[0xB00000][0];

	// status
	memset( &m_status, 0, sizeof(m_status) );
	// Battery levels in voltage
	//   0x00 - 0x32: level 1
	//   0x33 - 0x43: level 2
	//   0x33 - 0x54: level 3
	//   0x55 - 0xff: level 4
	m_status.battery = 0x5f;
}

Wiimote::Wiimote( const unsigned int index, SWiimoteInitialize* const wiimote_initialize )
	: m_wiimote_init( wiimote_initialize )
	, m_index(index)
{
	// ---- set up all the controls ----

	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	for ( unsigned int i=0; i < sizeof(named_buttons)/sizeof(*named_buttons); ++i )
		m_buttons->controls.push_back( new ControlGroup::Input( named_buttons[i] ) );

	// ir
	//groups.push_back( m_rumble = new ControlGroup( "IR" ) );
	//m_rumble->controls.push_back( new ControlGroup::Output( "X" ) );
	//m_rumble->controls.push_back( new ControlGroup::Output( "Y" ) );
	//m_rumble->controls.push_back( new ControlGroup::Output( "Distance" ) );
	//m_rumble->controls.push_back( new ControlGroup::Output( "Hide" ) );

	// forces
	groups.push_back( m_tilt = new Tilt( "Pitch and Roll" ) );
	//groups.push_back( m_tilt = new Tilt( "Tilt" ) );
	//groups.push_back( m_swing = new Force( "Swing" ) );

	// shake
	groups.push_back( m_shake = new Buttons( "Shake" ) );
	m_shake->controls.push_back( new ControlGroup::Input( "X" ) );
	m_shake->controls.push_back( new ControlGroup::Input( "Y" ) );
	m_shake->controls.push_back( new ControlGroup::Input( "Z" ) );

	// extension
	groups.push_back( m_extension = new Extension( "Extension" ) );
	m_extension->attachments.push_back( new WiimoteEmu::None() );
	m_extension->attachments.push_back( new WiimoteEmu::Nunchuk() );
	m_extension->attachments.push_back( new WiimoteEmu::Classic() );
	//m_extension->attachments.push_back( new Attachment::GH3() );

	// dpad
	groups.push_back( m_dpad = new Buttons( "D-Pad" ) );
	for ( unsigned int i=0; i < 4; ++i )
		m_dpad->controls.push_back( new ControlGroup::Input( named_directions[i] ) );

	// rumble
	groups.push_back( m_rumble = new ControlGroup( "Rumble" ) );
	m_rumble->controls.push_back( new ControlGroup::Output( "Motor" ) );

	// options
	groups.push_back( options = new ControlGroup( "Options" ) );
	options->settings.push_back( new ControlGroup::Setting( "Background Input", false ) );
	options->settings.push_back( new ControlGroup::Setting( "Sideways Wiimote", false ) );


	// --- reset eeprom/register/values to default ---
	Reset();
}

std::string Wiimote::GetName() const
{
	return std::string("Wiimote") + char('1'+m_index);
}

void Wiimote::Update()
{
	const bool is_sideways = options->settings[1]->value > 0;

	// update buttons in status struct
	m_status.buttons = 0;
	m_buttons->GetState( &m_status.buttons, button_bitmasks );
	m_dpad->GetState( &m_status.buttons, is_sideways ? dpad_sideways_bitmasks : dpad_bitmasks );

	// check if a status report needs to be sent
	// this happens on wiimote sync and when extensions are switched
	if (m_extension->active_extension != m_extension->switch_extension)
	{
		RequestStatus( m_reporting_channel, NULL );

		// Wiibrew: Following a connection or disconnection event on the Extension Port,
		// data reporting is disabled and the Data Reporting Mode must be reset before new data can arrive.

		// after a game receives an unrequested status report,
		// it expects data reports to stop until it sets the reporting mode again
		m_reporting_auto = false;
	}

	if ( false == m_reporting_auto )
		return;

	// figure out what data we need
	size_t rpt_size = 0;
	size_t rpt_core = 0;
	size_t rpt_accel = 0;
	size_t rpt_ir = 0;
	size_t rpt_ext = 0;

	switch ( m_reporting_mode )
	{
	//(a1) 30 BB BB
	case WM_REPORT_CORE :
		rpt_size	= 2 + 2;
		rpt_core	= 2;
		break;
	//(a1) 31 BB BB AA AA AA
	case WM_REPORT_CORE_ACCEL :
		rpt_size	= 2 + 2 + 3;
		rpt_core	= 2;
		rpt_accel	= 2 + 2;
		break;
	//(a1) 33 BB BB AA AA AA II II II II II II II II II II II II
	case WM_REPORT_CORE_ACCEL_IR12 :
		rpt_size	= 2 + 2 + 3 + 12;
		rpt_core	= 2;
		rpt_accel	= 2 + 2;
		rpt_ir		= 2 + 2 + 3;
		break;
	//(a1) 35 BB BB AA AA AA EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
	case WM_REPORT_CORE_ACCEL_EXT16 :
		rpt_size	= 2 + 2 + 3 + 16;
		rpt_core	= 2;
		rpt_accel	= 2 + 2;
		rpt_ext		= 2 + 2 + 3;
		break;	
	//(a1) 37 BB BB AA AA AA II II II II II II II II II II EE EE EE EE EE EE
	case WM_REPORT_CORE_ACCEL_IR10_EXT6 :
		rpt_size	= 2 + 2 + 3 + 10 + 6;
		rpt_core	= 2;
		rpt_accel	= 2 + 2;
		rpt_ir		= 2 + 2 + 3;
		rpt_ext		= 2 + 2 + 3 + 10;
		break;
	default :
		//PanicAlert( "Unsupported Reporting Mode" );
		return;
		break;
	}

	// set up output report
	u8* const rpt = new u8[rpt_size];
	memset( rpt, 0, rpt_size );

	rpt[0] = 0xA1;
	rpt[1] = m_reporting_mode;

	// core buttons - always 2
	if (rpt_core)
		*(wm_core*)(rpt + rpt_core) = m_status.buttons;

	// accelerometer
	if (rpt_accel)
	{
		// tilt
		float x, y;
		m_tilt->GetState( &x, &y, 0, (PI / 2) ); // 90 degrees

		// this isn't doing anything with those low bits in the calib data, o well

		const accel_cal* const cal = (accel_cal*)&m_eeprom[0x16];
		const u8* const zero_g = &cal->zero_g.x;
		u8 one_g[3];
		for ( unsigned int i=0; i<3; ++i )
			one_g[i] = (&cal->one_g.x)[i] - zero_g[i];

		// this math should be good enough :P
		rpt[rpt_accel + 2] = u8(sin( (PI / 2) - std::max( abs(x), abs(y) ) ) * one_g[2] + zero_g[2]);
		
		if (is_sideways)
		{
			rpt[rpt_accel + 0] = u8(sin(y) * -one_g[1] + zero_g[1]);
			rpt[rpt_accel + 1] = u8(sin(x) * -one_g[0] + zero_g[0]);
		}
		else
		{
			rpt[rpt_accel + 0] = u8(sin(x) * -one_g[0] + zero_g[0]);
			rpt[rpt_accel + 1] = u8(sin(y) * one_g[1] + zero_g[1]);
		}

		// shake
		const unsigned int btns[] = { 0x01, 0x02, 0x04 };
		unsigned int shake = 0;
		m_shake->GetState( &shake, btns );
		static unsigned int shake_step = 0;
		if (shake)
		{
			shake_step = (shake_step + 1) % sizeof(shake_data);
			for ( unsigned int i=0; i<3; ++i )
				if ( shake & (1 << i) )
					rpt[rpt_accel + i] = shake_data[shake_step];
		}
		else
			shake_step = 0;
	}

	// TODO: IR
	if (rpt_ir)
	{
	}

	// extension
	if (rpt_ext)
	{
		// temporary
		m_extension->GetState(rpt + rpt_ext);
		wiimote_encrypt(&m_ext_key, rpt + rpt_ext, 0x00, sizeof(wm_extension));

		// i dont think anything accesses the extension data like this, but ill support it
		memcpy( m_reg_ext + 8, rpt + rpt_ext, sizeof(wm_extension));
	}

	// send input report
	m_wiimote_init->pWiimoteInput( m_index, m_reporting_channel, rpt, (u32)rpt_size );

	delete[] rpt;
}

void Wiimote::ControlChannel(u16 _channelID, const void* _pData, u32 _Size) 
{

	// Check for custom communication
	if (99 == _channelID)
	{
		// wiimote disconnected
		//PanicAlert( "Wiimote Disconnected" );

		// reset eeprom/register/reporting mode
		Reset();
		return;
	}

	hid_packet* hidp = (hid_packet*)_pData;

	INFO_LOG(WIIMOTE, "Emu ControlChannel (page: %i, type: 0x%02x, param: 0x%02x)", m_index, hidp->type, hidp->param);

	switch(hidp->type)
	{
	case HID_TYPE_HANDSHAKE :
		PanicAlert("HID_TYPE_HANDSHAKE - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUPUT");
		break;

	case HID_TYPE_SET_REPORT :
		if (HID_PARAM_INPUT == hidp->param)
		{
			PanicAlert("HID_TYPE_SET_REPORT - INPUT"); 
		}
		else
		{
			// AyuanX: My experiment shows Control Channel is never used
			// shuffle2: but homebrew uses this, so we'll do what we must :)
			HidOutputReport(_channelID, (wm_report*)hidp->data);

			u8 handshake = HID_HANDSHAKE_SUCCESS;
			m_wiimote_init->pWiimoteInput(m_index, _channelID, &handshake, 1);

			PanicAlert("HID_TYPE_DATA - OUTPUT: Ambiguous Control Channel Report!");
		}
		break;

	case HID_TYPE_DATA :
		PanicAlert("HID_TYPE_DATA - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUTPUT");
		break;

	default :
		PanicAlert("HidControlChannel: Unknown type %x and param %x", hidp->type, hidp->param);
		break;
	}

}

void Wiimote::InterruptChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	hid_packet* hidp = (hid_packet*)_pData;

	switch (hidp->type)
	{
	case HID_TYPE_DATA:
		switch (hidp->param)
		{
		case HID_PARAM_OUTPUT :
			{
				wm_report* sr = (wm_report*)hidp->data;
				HidOutputReport(_channelID, sr);
			}
			break;

		default :
			PanicAlert("HidInput: HID_TYPE_DATA - param 0x%02x", hidp->type, hidp->param);
			break;
		}
		break;

	default:
		PanicAlert("HidInput: Unknown type 0x%02x and param 0x%02x", hidp->type, hidp->param);
		break;
	}
}

// TODO: i need to test this
void Wiimote::Register::Read( size_t address, void* dst, size_t length )
{
	while (length)
	{
		const std::vector<u8>* block = NULL;
		size_t addr_start = 0;
		size_t addr_end = address+length;

		// TODO: don't need to start at begin() each time
		// find block and start of next block
		const_iterator
			i = begin(),
			e = end();
		for ( ; i!=e; ++i )
			if ( address >= i->first )
			{
				block = &i->second;
				addr_start = i->first;
			}
			else
			{
				addr_end = std::min( i->first, addr_end );
				break;
			}

		// read bytes from a mapped block
		if (block)
		{
			const size_t offset = std::min( address - addr_start, block->size() );
			const size_t amt = std::min( block->size()-offset, length );

			memcpy( dst, &block->operator[](offset), amt );
			
			address += amt;
			dst = ((u8*)dst) + amt;
			length -= amt;
		}

		// read zeros for unmapped regions
		const size_t amt = addr_end - address;

		memset( dst, 0, amt );

		address += amt;
		dst = ((u8*)dst) + amt;
		length -= amt;
	}
}

// TODO: i need to test this
void Wiimote::Register::Write( size_t address, void* src, size_t length )
{
	while (length)
	{
		std::vector<u8>* block = NULL;
		size_t addr_start = 0;
		size_t addr_end = address+length;

		// TODO: don't need to start at begin() each time
		// find block and start of next block
		iterator
			i = begin(),
			e = end();
		for ( ; i!=e; ++i )
			if ( address >= i->first )
			{
				block = &i->second;
				addr_start = i->first;
			}
			else
			{
				addr_end = std::min( i->first, addr_end );
				break;
			}

		// write bytes to a mapped block
		if (block)
		{
			const size_t offset = std::min( address - addr_start, block->size() );
			const size_t amt = std::min( block->size()-offset, length );

			memcpy( &block->operator[](offset), src, amt );
			
			address += amt;
			src = ((u8*)src) + amt;
			length -= amt;
		}

		// do nothing for unmapped regions
		const size_t amt = addr_end - address;

		address += amt;
		src = ((u8*)src) + amt;
		length -= amt;
	}
}

}
