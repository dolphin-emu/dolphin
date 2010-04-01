#ifndef _CIFACE_DIRECTINPUT_KBM_H_
#define _CIFACE_DIRECTINPUT_KBM_H_

#include "../ControllerInterface.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#include <wx/stopwatch.h>
#include <wx/utils.h>

namespace ciface
{
namespace DirectInput
{

void InitKeyboardMouse( IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices );

class KeyboardMouse : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

	struct State
	{
		BYTE			keyboard[256];
		DIMOUSESTATE2	mouse;
	};

	class Input : public ControllerInterface::Device::Input
	{
		friend class KeyboardMouse;
	protected:
		virtual ControlState GetState( const State* const boardstate ) = 0;
	};

	class Output : public ControllerInterface::Device::Output
	{
		friend class KeyboardMouse;
	protected:
		virtual void SetState( const ControlState state, unsigned char* const state_out ) = 0;
	};

	class Key : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Key( const unsigned int index ) : m_index(index) {}
		ControlState GetState( const State* const state );
	private:
		const unsigned int	m_index;
	};

	class Button : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Button( const unsigned int index ) : m_index(index) {}
		ControlState GetState( const State* const state );
	private:
		const unsigned int	m_index;
	};

	class Axis : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Axis( const unsigned int index, const LONG range ) : m_index(index), m_range(range) {}
		ControlState GetState( const State* const state );
	private:
		const unsigned int	m_index;
		const LONG			m_range;
	};

	class Light : public Output
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Light( const unsigned int index ) : m_index(index) {}
		void SetState( const ControlState state, unsigned char* const state_out );
	private:
		const unsigned int	m_index;
	};
	
	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const input, const ControlState state );

public:
	KeyboardMouse( const LPDIRECTINPUTDEVICE8 kb_device, const LPDIRECTINPUTDEVICE8 mo_device );
	~KeyboardMouse();

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const LPDIRECTINPUTDEVICE8	m_kb_device;
	const LPDIRECTINPUTDEVICE8	m_mo_device;

	wxLongLong					m_last_update;
	State						m_state_in;
	unsigned char				m_state_out[3];	// NUM CAPS SCROLL
	bool				m_current_state_out[3];	// NUM CAPS SCROLL
};

}
}

#endif
