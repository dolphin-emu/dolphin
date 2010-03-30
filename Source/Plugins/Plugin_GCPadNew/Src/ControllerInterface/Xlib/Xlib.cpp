#include "../ControllerInterface.h"

#ifdef CIFACE_USE_XLIB

#include "Xlib.h"

namespace ciface
{
namespace Xlib
{


void Init( std::vector<ControllerInterface::Device*>& devices, void* const hwnd )
{
	// mouse will be added to this, Keyboard class will be turned into KeyboardMouse
	// single device for combined keyboard/mouse, this will allow combinations like shift+click more easily
	devices.push_back( new Keyboard( (Display*)display ) );
}

Keyboard::Keyboard( Display* const display ) : m_display(display)
{

	memset( &m_state, 0, sizeof(m_state) );

	// this is probably dumb
	for ( KeySym i = 0; i < 1024; ++i )
	{
		if ( XKeysymToString( m_keysym ) )	// if it isnt NULL
			inputs.push_back( new Key( i ) );
	}

}


Keyboard::~Keyboard()
{
	// might not need this func
}


ControlState Keyboard::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ((Input*)input)->GetState( &m_state );
}

void Keyboard::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state )
{

}

bool Keyboard::UpdateInput()
{
	XQueryKeymap( m_display, m_state.keyboard );

	// mouse stuff in here too

	return true;
}

bool Keyboard::UpdateOutput()
{
	return true;
}


std::string Keyboard::GetName() const
{
	return "Keyboard";
	//return "Keyboard Mouse";	// change to this later
}

std::string Keyboard::GetSource() const
{
	return "Xlib";
}

int Keyboard::GetId() const
{
	return 0;
}


ControlState Keyboard::Key::GetState( const State* const state )
{
	key_code = XKeysymToKeycode(m_display, m_keysym );
	return (state->keyboard[key_code/8] & (1 << (key_code%8))); // need >0 ?
}

std::string Keyboard::Key::GetName() const
{
	return XKeysymToString( m_keysym );
}


}
}

#endif
