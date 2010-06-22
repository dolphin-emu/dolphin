#ifndef _CIFACE_DIRECTINPUT_MOUSE_H_
#define _CIFACE_DIRECTINPUT_MOUSE_H_

#include "../ControllerInterface.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

namespace ciface
{
namespace DirectInput
{

void InitMouse(IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices);

class Mouse : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

	class Input : public ControllerInterface::Device::Input
	{
		friend class Mouse;
	protected:
		virtual ControlState GetState(const DIMOUSESTATE2* const boardstate) const = 0;
	};

	class Button : public Input
	{
		friend class Mouse;
	public:
		std::string GetName() const;
	protected:
		Button(const unsigned int index) : m_index(index) {}
		ControlState GetState(const DIMOUSESTATE2* const state) const;
	private:
		const unsigned int	m_index;
	};

	class Axis : public Input
	{
		friend class Mouse;
	public:
		std::string GetName() const;
	protected:
		Axis(const unsigned int index, const LONG range) : m_index(index), m_range(range) {}
		ControlState GetState(const DIMOUSESTATE2* const state) const;
	private:
		const unsigned int	m_index;
		const LONG			m_range;
	};
	
	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState( const ControllerInterface::Device::Input* const input ) const;
	void SetOutputState( const ControllerInterface::Device::Output* const input, const ControlState state );

public:
	Mouse(const LPDIRECTINPUTDEVICE8 mo_device, const int index);
	~Mouse();

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const LPDIRECTINPUTDEVICE8	m_mo_device;
	const int			m_index;

	DWORD			m_last_update;
	DIMOUSESTATE2		m_state_in;
};

}
}

#endif
