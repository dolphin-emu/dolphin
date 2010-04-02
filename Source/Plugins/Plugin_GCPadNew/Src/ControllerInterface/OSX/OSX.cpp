#include "../ControllerInterface.h"

#ifdef CIFACE_USE_OSX

#include "OSX.h"
#include "OSXPrivate.h"

namespace ciface
{
	namespace OSX
	{
		
		
		void Init( std::vector<ControllerInterface::Device*>& devices, void* hwnd )
		{
			// mouse will be added to this, Keyboard class will be turned into KeyboardMouse
			// single device for combined keyboard/mouse, this will allow combinations like shift+click more easily
			//devices.push_back( new Keyboard( hwnd ) );
		}
		
		Keyboard::Keyboard( void *View )
		{
			
			memset( &m_state, 0, sizeof(m_state) );
			OSX_Init(View);
			// This is REALLY dumb right here
			// Should REALLY cover the entire UTF-8 Range
			for (int a = 0; a < 256; ++a)
				inputs.push_back( new Key( (char)a ) );
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
			memset(m_state.keyboard, 0, 256);
			OSX_UpdateKeys(256, m_state.keyboard );
			
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
			return "OSX";
		}
		
		int Keyboard::GetId() const
		{
			return 0;
		}
		
		
		ControlState Keyboard::Key::GetState( State* const state )
		{
			for(unsigned int a = 0; a < 256; ++a)
				if(state->keyboard[a] == m_index)
					return true;
			return false;
		}
		
		std::string Keyboard::Key::GetName() const
		{
			char temp[16];
			sprintf(temp, "Key:%c", m_index);
			return std::string(temp);
		}
		
		
	}
}

#endif
