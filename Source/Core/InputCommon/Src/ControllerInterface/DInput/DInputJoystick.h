#ifndef _CIFACE_DINPUT_JOYSTICK_H_
#define _CIFACE_DINPUT_JOYSTICK_H_

#include "../Device.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#include <list>

namespace ciface
{
namespace DInput
{

void InitJoystick(IDirectInput8* const idi8, std::vector<Core::Device*>& devices, HWND hwnd);

class Joystick : public Core::Device
{
private:
	struct EffectState
	{
		EffectState(LPDIRECTINPUTEFFECT eff) : iface(eff), params(NULL), size(0) {}

		LPDIRECTINPUTEFFECT		iface;
		void*	params;	// null when force hasn't changed
		u8		size;	// zero when force should stop
	};

	class Button : public Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, const BYTE& button) : m_index(index), m_button(button) {}
		ControlState GetState() const;
	private:
		const BYTE& m_button;
		const u8 m_index;
	};

	class Axis : public Input
	{
	public:
		std::string GetName() const;
		Axis(u8 index, const LONG& axis, LONG base, LONG range) : m_index(index), m_axis(axis), m_base(base), m_range(range) {}
		ControlState GetState() const;
	private:
		const LONG& m_axis;
		const LONG m_base, m_range;
		const u8 m_index;
	};

	class Hat : public Input
	{
	public:
		std::string GetName() const;
		Hat(u8 index, const DWORD& hat, u8 direction) : m_index(index), m_hat(hat), m_direction(direction) {}
		ControlState GetState() const;
	private:
		const DWORD& m_hat;
		const u8 m_index, m_direction;
	};

	template <typename P>
	class Force : public Output
	{
	public:
		std::string GetName() const;
		Force(u8 index, EffectState& state);
		void SetState(ControlState state);
	private:
		EffectState& m_state;
		P params;
		const u8 m_index;
	};
	typedef Force<DICONSTANTFORCE>	ForceConstant;
	typedef Force<DIRAMPFORCE>		ForceRamp;
	typedef Force<DIPERIODIC>		ForcePeriodic;

public:
	bool UpdateInput();
	bool UpdateOutput();

	void ClearInputState();

	Joystick(const LPDIRECTINPUTDEVICE8 device, const unsigned int index);
	~Joystick();
	
	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const LPDIRECTINPUTDEVICE8		m_device;
	const unsigned int				m_index;

	DIJOYSTATE					m_state_in;
	std::list<EffectState>		m_state_out;

	bool						m_buffered;
};

}
}

#endif
