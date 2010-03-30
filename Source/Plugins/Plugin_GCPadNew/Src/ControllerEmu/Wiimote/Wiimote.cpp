
#include "Wiimote.h"

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

// reports

#define REPORT_UNKNOWN				0x10
#define REPORT_LEDS					0x11
#define REPORT_DATA_MODE			0x12
#define REPORT_IR_ENABLE			0x13
#define REPORT_SPEAKER_ENABLE		0x14
#define REPORT_STATUS_REQUEST		0x15
#define REPORT_WRITE_MEMORY			0x16
#define REPORT_READ_MEMORY			0x17
#define REPORT_SPEAKER_DATA			0x18
#define REPORT_SPEAKER_MUTE			0x19
#define REPORT_IR_ENABLE_2			0x1a
#define REPORT_STATUS				0x20
#define REPORT_READ_MEMORY_DATA		0x21
#define REPORT_ACKNOWLEDGE_OUTPUT	0x22
//#define REPORT_DATA_REPORTS			0x30-0x3f

// reporting modes

#define REPORT_MODE_CORE					0x30
#define REPORT_MODE_CORE_ACCEL				0x31
#define REPORT_MODE_CORE_EXTEN8				0x32
#define REPORT_MODE_CORE_ACCEL_IR12			0x33
#define REPORT_MODE_CORE_EXTEN19			0x34
#define REPORT_MODE_CORE_ACCEL_EXTEN16		0x35
#define REPORT_MODE_CORE_IR10_EXTEN9		0x36
#define REPORT_MODE_CORE_ACCEL_IR10_EXTEN6	0x37
#define REPORT_MODE_EXTEN21					0x3d
#define REPORT_MODE_IL_CORE_ACCEL_IR36_1	0x3e
#define REPORT_MODE_IL_CORE_ACCEL_IR36_2	0x3f

// this is all temporary, but sticking with the padspecs ...for now
const u16 button_bitmasks[] =
{
	WIIMOTE_A, WIIMOTE_B, WIIMOTE_ONE, WIIMOTE_TWO, WIIMOTE_MINUS, WIIMOTE_PLUS, WIIMOTE_HOME
};

const u16 dpad_bitmasks[] =
{
	WIIMOTE_PAD_UP, WIIMOTE_PAD_DOWN, WIIMOTE_PAD_LEFT, WIIMOTE_PAD_RIGHT
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

const char * const named_groups[] =
{
	"Buttons", "D-Pad"
};

Wiimote::Wiimote( const unsigned int index ) : m_index(index), m_report_mode(REPORT_MODE_CORE)
{

	// buttons
	groups.push_back( m_buttons = new Buttons( named_groups[0] ) );
	for ( unsigned int i=0; i < sizeof(named_buttons)/sizeof(*named_buttons); ++i )
		m_buttons->controls.push_back( new ControlGroup::Input( named_buttons[i] ) );

	// dpad
	groups.push_back( m_dpad = new Buttons( named_groups[1] ) );
	for ( unsigned int i=0; i < 4; ++i )
		m_dpad->controls.push_back( new ControlGroup::Input( named_directions[i] ) );

	// rumble
	groups.push_back( m_rumble = new ControlGroup( "Rumble" ) );
	m_rumble->controls.push_back( new ControlGroup::Output( "Motor" ) );

}

std::string Wiimote::GetName() const
{
	return std::string("Wiimote") + char('1'+m_index);
}

void Wiimote::Cycle()
{

	HIDReport	rpt;
	rpt << (u8)m_report_mode;

	switch ( m_report_mode )
	{
	//(a1) 30 BB BB
	case REPORT_MODE_CORE :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons;
		}
		break;
	//(a1) 31 BB BB AA AA AA
	case REPORT_MODE_CORE_ACCEL :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u16)0 << (u8)0;
		}
		break;
	//(a1) 32 BB BB EE EE EE EE EE EE EE EE
	case REPORT_MODE_CORE_EXTEN8 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0;	
		}
		break;
	//(a1) 33 BB BB AA AA AA II II II II II II II II II II II II
	case REPORT_MODE_CORE_ACCEL_IR12 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0 << (u32)0 << (u16)0 << (u8)0;
		}
		break;
	//(a1) 34 BB BB EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE 
	case REPORT_MODE_CORE_EXTEN19 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0 << (u64)0 << (u16)0 << (u8)0;
		}
		break;
	//(a1) 35 BB BB AA AA AA EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
	case REPORT_MODE_CORE_ACCEL_EXTEN16 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0 << (u64)0 << (u16)0 << (u8)0;
		}
		break;	
	//(a1) 36 BB BB II II II II II II II II II II EE EE EE EE EE EE EE EE EE
	case REPORT_MODE_CORE_IR10_EXTEN9 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0 << (u64)0 << (u16)0 << (u8)0;
		}
		break;
	//(a1) 37 BB BB AA AA AA II II II II II II II II II II EE EE EE EE EE EE
	case REPORT_MODE_CORE_ACCEL_IR10_EXTEN6 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0 << (u64)0 << (u16)0 << (u8)0;
		}
		break;
	//(a1) 3d EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
	case REPORT_MODE_EXTEN21 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt << buttons << (u64)0 << (u64)0 << (u16)0 << (u8)0;
		}
		break;
	//(a1) 3e/3f BB BB AA II II II II II II II II II II II II II II II II II II
	case REPORT_MODE_IL_CORE_ACCEL_IR36_1 :
	case REPORT_MODE_IL_CORE_ACCEL_IR36_2 :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			//rpt << buttons << (u64)0 << (u64)0 << (u64)0 << (u64)0 << (u32)0 << (u8)0;
			rpt << buttons << (u64)0 << (u64)0 << (u16)0 << (u8)0;
		}
		break;
	default :
		break;
	}

}

Wiimote& Wiimote::operator<<( HIDReport& rpt_in )
{
	const u8 report_id = rpt_in.front(); rpt_in.pop();

	HIDReport rpt_out;

	switch ( report_id )
	{
	case REPORT_LEDS :
		break;
	case REPORT_DATA_MODE :
		{
			rpt_in.pop();	// continuous
			m_report_mode = rpt_in.front();
		}
		break;
	case REPORT_IR_ENABLE :
		break;
	case REPORT_SPEAKER_ENABLE :
		break;
	case REPORT_STATUS_REQUEST :
		{
			u16 buttons;
			m_buttons->GetState( &buttons, button_bitmasks );
			m_dpad->GetState( &buttons, dpad_bitmasks );
			rpt_out << (u8)REPORT_STATUS << buttons << (u16)0 << (u8)0 << (u8)0xFF;
		}
		break;
	case REPORT_WRITE_MEMORY :
		break;
	case REPORT_READ_MEMORY :
		break;
	case REPORT_SPEAKER_DATA :
		break;
	case REPORT_SPEAKER_MUTE :
		break;
	case REPORT_IR_ENABLE_2 :
		break;
	case REPORT_STATUS :
		break;
	case REPORT_READ_MEMORY_DATA :
		break;
	case REPORT_ACKNOWLEDGE_OUTPUT :
		break;
	default :
		break;
	}

	return *this;
}
