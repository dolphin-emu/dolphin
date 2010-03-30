#ifndef _CIFACE_XLIB_H_
#define _CIFACE_XLIB_H_

#include "../ControllerInterface.h"

#include <X11/Xlib.h>

namespace ciface
{
namespace XInput
{

void Init( std::vector<ControllerInterface::Device*>& devices, void* const hwnd );

class Keyboard : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

	struct State
	{
		char keyboard[32];
		// mouse crap will go here
	};

	class Input : public ControllerInterface::Input
	{
		friend class Keyboard;
	protected:
		ControlState GetState( const State* const state ) = 0;
	}

	class Key : public Input
	{
		friend class Keyboard;
	public:
		std::string GetName() const;
	protected:
		Key( KeySym keysym ) : m_keysym(keysym) {}
		ControlState GetState( const State* const state );
	private:
		const KeySym m_keysym
		
	};

	bool UpdateInput();
	bool UpdateOutput();

	ControlState Keyboard::GetInputState( const ControllerInterface::Device::Input* const input );
	void Keyboard::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state );

public:
	Keyboard( Display* const display );
	~Keyboard();

	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;

private:
	Display* const	m_display;
	State			m_state;
};

}
}

#endif
