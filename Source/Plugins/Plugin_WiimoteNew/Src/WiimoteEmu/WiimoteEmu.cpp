
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
	// IR, maybe more
	// assuming last 2 bytes are checksum
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, /*0x74, 0xD3,*/ 0x00, 0x00,	// messing up the checksum on purpose
	0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, /*0x74, 0xD3,*/ 0x00, 0x00,
	// Accelerometer
	// 0g x,y,z, 1g x,y,z, 2 byte checksum
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E,
	0x82, 0x82, 0x82, 0x15, 0x9C, 0x9C, 0x9E, 0x38, 0x40, 0x3E
};


static const u8 eeprom_data_16D0[] = {
	0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00,
	0x33, 0xCC, 0x44, 0xBB, 0x00, 0x00, 0x66, 0x99,
	0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13
};

struct ReportFeatures
{
	u8		core, accel, ir, ext, size;
} const reporting_mode_features[] = 
{
    //0x30: Core Buttons
	{ 2, 0, 0, 0, 4 },
    //0x31: Core Buttons and Accelerometer
	{ 2, 4, 0, 0, 7 },
    //0x32: Core Buttons with 8 Extension bytes
	{ 2, 0, 0, 4, 12 },
    //0x33: Core Buttons and Accelerometer with 12 IR bytes
	{ 2, 4, 7, 0, 19 },
    //0x34: Core Buttons with 19 Extension bytes
	{ 2, 0, 0, 4, 23 },
    //0x35: Core Buttons and Accelerometer with 16 Extension Bytes
	{ 2, 4, 0, 7, 23 },
    //0x36: Core Buttons with 10 IR bytes and 9 Extension Bytes
	{ 2, 0, 4, 14, 23 },
    //0x37: Core Buttons and Accelerometer with 10 IR bytes and 6 Extension Bytes
	{ 2, 4, 7, 17, 23 },
    //0x3d: 21 Extension Bytes
	{ 0, 0, 0, 2, 23 },
    //0x3e / 0x3f: Interleaved Core Buttons and Accelerometer with 36 IR bytes
	// UNSUPPORTED
	{ 0, 0, 0, 0, 23 },
};

// array of accel data to emulate shaking
const u8 shake_data[8] = { 0x40, 0x01, 0x40, 0x80, 0xC0, 0xFF, 0xC0, 0x80 };

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

	m_rumble_on = false;

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
	m_reg_ir			= &m_register[0xB00000][0];

	// status
	memset( &m_status, 0, sizeof(m_status) );
	// Battery levels in voltage
	//   0x00 - 0x32: level 1
	//   0x33 - 0x43: level 2
	//   0x33 - 0x54: level 3
	//   0x55 - 0xff: level 4
	m_status.battery = 0x5f;

	m_shake_step = 0;

	// clear read request queue
	while (m_read_requests.size())
	{
		delete[] m_read_requests.front().data;
		m_read_requests.pop();
	}
}

Wiimote::Wiimote( const unsigned int index ) : m_index(index)
{
	// ---- set up all the controls ----

	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	for ( unsigned int i=0; i < sizeof(named_buttons)/sizeof(*named_buttons); ++i )
		m_buttons->controls.push_back( new ControlGroup::Input( named_buttons[i] ) );

	// ir
	groups.push_back( m_ir = new Cursor( "IR", &g_WiimoteInitialize ) );
	m_ir->controls.push_back( new ControlGroup::Input( "Forward" ) );
	m_ir->controls.push_back( new ControlGroup::Input( "Hide" ) );

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

	// if windows is focused or background input is enabled
	const bool focus = g_WiimoteInitialize.pRendererHasFocus() || (options->settings[0]->value != 0);

	// no rumble if no focus
	if (false == focus)
		m_rumble_on = false;
	m_rumble->controls[0]->control_ref->State(m_rumble_on);

	// update buttons in status struct
	m_status.buttons = 0;
	if ( focus )
	{
		m_buttons->GetState( &m_status.buttons, button_bitmasks );
		m_dpad->GetState( &m_status.buttons, is_sideways ? dpad_sideways_bitmasks : dpad_bitmasks );
	}

	// check if there is a read data request
	if ( m_read_requests.size() )
	{
		ReadRequest& rr = m_read_requests.front();
		// send up to 16 bytes to the wii
		SendReadDataReply(m_reporting_channel, rr);
		//SendReadDataReply(rr.channel, rr);

		// if there is no more data, remove from queue
		if (0 == rr.size)
		{
			delete[] rr.data;
			m_read_requests.pop();
		}

		// dont send any other reports
		return;
	}

	// -- maybe this should happen before the read request stuff?
	// check if a status report needs to be sent
	// this happens on wiimote sync and when extensions are switched
	if (m_extension->active_extension != m_extension->switch_extension)
	{
		RequestStatus(m_reporting_channel);

		// Wiibrew: Following a connection or disconnection event on the Extension Port,
		// data reporting is disabled and the Data Reporting Mode must be reset before new data can arrive.
		// after a game receives an unrequested status report,
		// it expects data reports to stop until it sets the reporting mode again
		m_reporting_auto = false;
	}

	if ( false == m_reporting_auto )
		return;

	// figure out what data we need
	const ReportFeatures& rpt = reporting_mode_features[m_reporting_mode - WM_REPORT_CORE];

	// set up output report
	// made data bigger than needed in case the wii specifies the wrong ir mode for a reporting mode
	u8 data[46];
	memset( data, 0, sizeof(data) );

	data[0] = 0xA1;
	data[1] = m_reporting_mode;

	// core buttons
	if (rpt.core)
		*(wm_core*)(data + rpt.core) = m_status.buttons;

	// accelerometer
	if (rpt.accel)
	{
		// tilt
		float x, y;
		m_tilt->GetState( &x, &y, 0, focus ? (PI / 2) : 0 ); // 90 degrees

		// this isn't doing anything with those low bits in the calib data, o well

		const accel_cal* const cal = (accel_cal*)&m_eeprom[0x16];
		const u8* const zero_g = &cal->zero_g.x;
		u8 one_g[3];
		for ( unsigned int i=0; i<3; ++i )
			one_g[i] = (&cal->one_g.x)[i] - zero_g[i];

		// this math should be good enough :P
		data[rpt.accel + 2] = u8(sin( (PI / 2) - std::max( abs(x), abs(y) ) ) * one_g[2] + zero_g[2]);
		
		if (is_sideways)
		{
			data[rpt.accel + 0] = u8(sin(y) * -one_g[1] + zero_g[1]);
			data[rpt.accel + 1] = u8(sin(x) * -one_g[0] + zero_g[0]);
		}
		else
		{
			data[rpt.accel + 0] = u8(sin(x) * -one_g[0] + zero_g[0]);
			data[rpt.accel + 1] = u8(sin(y) * one_g[1] + zero_g[1]);
		}

		// shake
		const unsigned int btns[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20 };
		unsigned int shake = 0;
		if (focus)
			m_shake->GetState( &shake, btns );
		if (shake)
		{
			for ( unsigned int i=0; i<3; ++i )
				if (shake & (1 << i))
					data[rpt.accel + i] = shake_data[m_shake_step];
			m_shake_step = (m_shake_step + 1) % sizeof(shake_data);
		}
		else
			m_shake_step = 0;

		// swing
		//u8 swing[3];
		//m_swing->GetState( swing, 0x80, 60 );
		//for ( unsigned int i=0; i<3; ++i )
		//	if ( swing[i] != 0x80 )
		//		data[rpt.accel + i] = swing[i];

	}

	// extension
	if (rpt.ext)
	{
		m_extension->GetState(data + rpt.ext, focus);
		wiimote_encrypt(&m_ext_key, data + rpt.ext, 0x00, sizeof(wm_extension));

		// i dont think anything accesses the extension data like this, but ill support it
		memcpy(m_reg_ext + 8, data + rpt.ext, sizeof(wm_extension));
	}

	// ir
	if (rpt.ir)
	{
		float xx = 10000, yy = 0, zz = 0;

		if (focus)
			m_ir->GetState(&xx, &yy, &zz, true);

		xx *= (-256 * 0.95f);
		xx += 512;

		yy *= (-256 * 0.90f);
		yy += 490;

		const unsigned int distance = (unsigned int)(100 + 100 * zz);

		// TODO: make roll affect the dot positions
		const unsigned int y = (unsigned int)yy;

		unsigned int x[4];
		x[0] = (unsigned int)(xx - distance);
		x[1] = (unsigned int)(xx + distance);
		x[2] = (unsigned int)(xx - 1.2f * distance);
		x[3] = (unsigned int)(xx + 1.2f * distance);

		// ir mode
		switch (m_reg_ir[0x33])
		{
		// basic
		case 1 :
			{
			memset(data + rpt.ir, 0xFF, 10);
			wm_ir_basic* const irdata = (wm_ir_basic*)(data + rpt.ir);
			if (y < 768)
			{
				for ( unsigned int i=0; i<2; ++i )
				{
					if (x[i*2] < 1024)
					{
						irdata[i].x1 = u8(x[i*2]);
						irdata[i].x1hi = x[i*2] >> 8;

						irdata[i].y1 = u8(y);
						irdata[i].y1hi = y >> 8;
					}
					if (x[i*2+1] < 1024)
					{
						irdata[i].x2 = u8(x[i*2+1]);
						irdata[i].x2hi = x[i*2+1] >> 8;

						irdata[i].y2 = u8(y);
						irdata[i].y2hi = y >> 8;
					}
				}
			}
			}
			break;
		// extended
		case 3 :
			{
			memset(data + rpt.ir, 0xFF, 12);
			wm_ir_extended* const irdata = (wm_ir_extended*)(data + rpt.ir);
			if (y < 768)
			{
				for ( unsigned int i=0; i<2; ++i )
					if (irdata[i].x < 1024)
					{
						irdata[i].x = u8(x[i]);
						irdata[i].xhi = x[i] >> 8;

						irdata[i].y = u8(y);
						irdata[i].yhi = y >> 8;

						irdata[i].size = 10;
					}
			}
			}
			break;
		// full
		case 5 :
			// UNSUPPORTED
			break;

		}
	}

	// send data report
	g_WiimoteInitialize.pWiimoteInput( m_index, m_reporting_channel, data, rpt.size );
}

void Wiimote::ControlChannel(const u16 _channelID, const void* _pData, u32 _Size) 
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
			g_WiimoteInitialize.pWiimoteInput(m_index, _channelID, &handshake, 1);

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

void Wiimote::InterruptChannel(const u16 _channelID, const void* _pData, u32 _Size)
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
