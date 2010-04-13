#ifndef _CONEMU_WIIMOTE_H_
#define _CONEMU_WIIMOTE_H_

#include <ControllerEmu.h>

#include "WiimoteHid.h"
#include "Encryption.h"

#include <vector>

#define PI	3.14159265358979323846

// Registry sizes 
#define WIIMOTE_EEPROM_SIZE			(16*1024)
#define WIIMOTE_EEPROM_FREE_SIZE	0x16ff
#define WIIMOTE_REG_SPEAKER_SIZE	10
#define WIIMOTE_REG_EXT_SIZE		0x100
#define WIIMOTE_REG_IR_SIZE			0x34

namespace WiimoteEmu
{

extern const u8 shake_data[8];

class Wiimote : public ControllerEmu
{
public:

	Wiimote( const unsigned int index, SWiimoteInitialize* const wiimote_initialize );
	void Reset();

	void Update();
	void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size);
	void ControlChannel(u16 _channelID, const void* _pData, u32 _Size);

	void ReportMode(u16 _channelID, wm_report_mode* dr);
	void HidOutputReport(u16 _channelID, wm_report* sr);
	void SendAck(u16 _channelID, u8 _reportID);
	void RequestStatus(u16 _channelID, wm_request_status* rs, int Extension = -1);

	void WriteData(u16 _channelID, wm_write_data* wd);
	void ReadData(u16 _channelID, wm_read_data* rd);
	void SendReadDataReply(u16 _channelID, const void* _Base, unsigned int _Address, unsigned int _Size);

	std::string GetName() const;

private:

	SWiimoteInitialize* const	m_wiimote_init;

	Buttons*				m_buttons;
	Buttons*				m_dpad;
	Buttons*				m_shake;
	Tilt*					m_tilt;
	Force*					m_swing;
	ControlGroup*			m_rumble;
	Extension*				m_extension;
	// TODO: add ir

	const unsigned int		m_index;

	bool					m_reporting_auto;
	unsigned int			m_reporting_mode;
	unsigned int			m_reporting_channel;

	wm_status_report		m_status;

	class Register : public std::map< size_t, std::vector<u8> >
	{
	public:
		void Write( size_t address, void* src, size_t length );
		void Read( size_t address, void* dst, size_t length );

	} m_register;

	//u8		m_eeprom[WIIMOTE_EEPROM_SIZE];
	u8		m_eeprom[WIIMOTE_EEPROM_SIZE];

	//u8*		m_reg_speaker;
	//u8*		m_reg_motion_plus;
	//u8*		m_reg_ir;
	u8*		m_reg_ext;

	wiimote_key		m_ext_key;
};

}

#endif
