#ifndef _CONEMU_WIIMOTE_H_
#define _CONEMU_WIIMOTE_H_

//#define USE_WIIMOTE_EMU_SPEAKER

//#include <Core.h>
//#include <HW/StreamADPCM.h>
// just used to get the OpenAL includes :p
//#include <OpenALStream.h>

#include "ControllerEmu.h"
#include "ChunkFile.h"

#include "WiimoteHid.h"
#include "Encryption.h"
#include "UDPWrapper.h"

#include <vector>
#include <queue>

#define PI	3.14159265358979323846

// Registry sizes 
#define WIIMOTE_EEPROM_SIZE			(16*1024)
#define WIIMOTE_EEPROM_FREE_SIZE	0x1700
#define WIIMOTE_REG_SPEAKER_SIZE	10
#define WIIMOTE_REG_EXT_SIZE		0x100
#define WIIMOTE_REG_IR_SIZE			0x34

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiimoteEmu
{

struct ReportFeatures
{
	u8		core, accel, ir, ext, size;
};

struct AccelData
{
	double x,y,z;
};	

extern const ReportFeatures reporting_mode_features[];

void EmulateShake(AccelData* const accel_data
	  , ControllerEmu::Buttons* const buttons_group
	  , unsigned int* const shake_step);

void EmulateTilt(AccelData* const accel
	 , ControllerEmu::Tilt* const tilt_group
	 , const bool focus, const bool sideways = false, const bool upright = false);

void EmulateSwing(AccelData* const accel
	 , ControllerEmu::Force* const tilt_group
	 , const bool sideways = false, const bool upright = false);

inline double trim(double a)
{
	if (a<=0) return 0;
	if (a>=255) return 255;
	return a;
}

class Wiimote : public ControllerEmu
{
public:

	enum
	{
		PAD_LEFT =		0x01,
		PAD_RIGHT =		0x02,
		PAD_DOWN =		0x04,
		PAD_UP =		0x08,
		BUTTON_PLUS =	0x10,

		BUTTON_TWO =	0x0100,
		BUTTON_ONE =	0x0200,
		BUTTON_B =		0x0400,
		BUTTON_A =		0x0800,
		BUTTON_MINUS =	 0x1000,
		BUTTON_HOME =	0x8000,
	};

	Wiimote( const unsigned int index );
	std::string GetName() const;

	void Update();
	void InterruptChannel(const u16 _channelID, const void* _pData, u32 _Size);
	void ControlChannel(const u16 _channelID, const void* _pData, u32 _Size);

	void DoState(PointerWrap& p);

	void LoadDefaults(const ControllerInterface& ciface);

protected:
	bool Step();
	void HidOutputReport(const wm_report* const sr, const bool send_ack = true);
	void HandleExtensionSwap();

	void GetCoreData(u8* const data);
	void GetAccelData(u8* const data, u8* const buttons);
	void GetIRData(u8* const data, bool use_accel);
	void GetExtData(u8* const data);

	bool HaveExtension() const { return m_extension->active_extension > 0; }
	bool WantExtension() const { return m_extension->switch_extension != 0; }

private:
	struct ReadRequest
	{
		//u16		channel;
		unsigned int	address, size, position;
		u8*		data;
	};

	void Reset();

	void ReportMode(const wm_report_mode* const dr);
	void SendAck(const u8 _reportID);
	void RequestStatus(const wm_request_status* const rs = NULL);
	void ReadData(const wm_read_data* const rd);
	void WriteData(const wm_write_data* const wd);
	void SendReadDataReply(ReadRequest& _request);

#ifdef USE_WIIMOTE_EMU_SPEAKER
	void SpeakerData(wm_speaker_data* sd);
#endif

	// control groups
	Buttons*				m_buttons;
	Buttons*				m_dpad;
	Buttons*				m_shake;
	Cursor*					m_ir;
	Tilt*					m_tilt;
	Force*					m_swing;
	ControlGroup*			m_rumble;
	Extension*				m_extension;
	ControlGroup*			m_options;
	// WiiMote accel data
	AccelData				m_accel;

	//UDPWiimote
	UDPWrapper* m_udp;

	// wiimote index, 0-3
	const unsigned int		m_index;

	bool		m_rumble_on;
	bool		m_speaker_mute;

	bool		m_reporting_auto;
	u8			m_reporting_mode;
	u16			m_reporting_channel;

	unsigned int			m_shake_step[3];

	wm_status_report		m_status;

	class Register : public std::map< size_t, std::vector<u8> >
	{
	public:
		void Write( size_t address, const void* src, size_t length );
		void Read( size_t address, void* dst, size_t length );

	} m_register;

	// read data request queue
	// maybe it isn't actualy a queue
	// maybe read requests cancel any current requests
	std::queue< ReadRequest >	m_read_requests;

#ifdef USE_WIIMOTE_EMU_SPEAKER
	// speaker stuff
	struct SoundBuffer
	{
		s16* samples;
		ALuint buffer;
	};
	std::queue<SoundBuffer>	m_audio_buffers;
	ALuint					m_audio_source;
	struct
	{
		int		hist1p, hist2p;
	}	m_channel_status;
#endif

	u8		m_eeprom[WIIMOTE_EEPROM_SIZE];

	u8*		m_reg_motion_plus;

	struct IrReg
	{
		u8	data[0x33];
		u8	mode;

	}	*m_reg_ir;

	struct ExtensionReg
	{
		u8	unknown1[0x08];

		// address 0x08
		u8	controller_data[0x06];
		u8	unknown2[0x12];

		// address 0x20
		u8	calibration[0x10];
		u8	unknown3[0x10];

		// address 0x40
		u8	encryption_key[0x10];
		u8	unknown4[0xA0];

		// address 0xF0
		u8	encryption;
		u8	unknown5[0x9];

		// address 0xFA
		u8	constant_id[6];

	}	*m_reg_ext;

	struct SpeakerReg
	{
		u16		unknown;
		u8		format;
		u16		sample_rate;
		u8		volume;
		u8		unk[4];

	}	*m_reg_speaker;

	wiimote_key		m_ext_key;
};

}

#endif
