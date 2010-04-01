#ifndef _CIFACE_DIRECTINPUT_JOYSTICK_H_
#define _CIFACE_DIRECTINPUT_JOYSTICK_H_

#include "../ControllerInterface.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#ifdef CIFACE_USE_XINPUT
	// this takes so long, idk if it should be enabled :(
	#define NO_DUPLICATE_DINPUT_XINPUT
	#include <wbemidl.h>
	#include <oleauto.h>
#endif

namespace ciface
{
namespace DirectInput
{

void InitJoystick( IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices/*, HWND hwnd*/ );

class Joystick : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

	struct EffectState
	{
		EffectState( LPDIRECTINPUTEFFECT eff ) : changed(0), iface(eff) {}
		LPDIRECTINPUTEFFECT			iface;
		ControlState				magnitude;
		bool						changed;
	};

	class Input : public ControllerInterface::Device::Input
	{
		friend class Joystick;
	protected:
		virtual ControlState GetState( const DIJOYSTATE* const joystate ) = 0;
	};

	// can probably eliminate this base class
	class Output : public ControllerInterface::Device::Output
	{
		friend class Joystick;
	protected:
		virtual void SetState( const ControlState state, EffectState* const joystate ) = 0;
	};

	class Button : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Button( const unsigned int index ) : m_index(index) {}
		ControlState GetState( const DIJOYSTATE* const joystate );
	private:
		const unsigned int	m_index;
	};

	class Axis : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Axis( const unsigned int index, const LONG base, const LONG range ) : m_index(index), m_base(base), m_range(range) {}
		ControlState GetState( const DIJOYSTATE* const joystate );
	private:
		const unsigned int	m_index;
		const LONG			m_base;
		const LONG			m_range;
	};

	class Hat : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Hat( const unsigned int index, const unsigned int direction ) : m_index(index), m_direction(direction) {}
		ControlState GetState( const DIJOYSTATE* const joystate );
	private:
		const unsigned int	m_index;
		const unsigned int	m_direction;
	};

	class Force : public Output
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Force( const unsigned int index ) : m_index(index) {}
		void SetState( const ControlState state, EffectState* const joystate );
	private:
		const unsigned int	m_index;
	};

	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const input, const ControlState state );

	void ClearInputState();

public:
	Joystick( /*const LPCDIDEVICEINSTANCE lpddi, */const LPDIRECTINPUTDEVICE8 device, const unsigned int index );
	~Joystick();
	
	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const LPDIRECTINPUTDEVICE8			m_device;
	const unsigned int					m_index;
	//const std::string					m_name;

	DIJOYSTATE							m_state_in;
	std::vector<EffectState>			m_state_out;

	bool								m_must_poll;
};

}
}

#endif
