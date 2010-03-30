#ifndef _CONEMU_WIIMOTE_H_
#define _CONEMU_WIIMOTE_H_

#include "../../ControllerEmu.h"

#include <queue>

class HIDReport : public std::queue<u8>
{
public:
	template <typename T>
	HIDReport& operator<<( const T& t )
	{
		const u8* const e = ((u8*)&t) + sizeof(t);
		const u8* i = (u8*)&t;
		for ( ;i<e; ++i )
			push(*i);
		return *this;
	}
};

class Wiimote : public ControllerEmu
{
public:

	Wiimote( const unsigned int index );
	void Cycle();
	Wiimote& operator<<( HIDReport& );
	
	std::string GetName() const;

private:

	Buttons*				m_buttons;
	Buttons*				m_dpad;
	ControlGroup*			m_rumble;
	// TODO: add forces
	// TODO: add ir

	const unsigned int		m_index;
	unsigned int			m_report_mode;

};


#endif
