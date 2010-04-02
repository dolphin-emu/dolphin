#ifndef _CIFACE_OSX_H_
#define _CIFACE_OSX_H_

#include "../ControllerInterface.h"

namespace ciface
{
	namespace OSX
	{
		
		void Init( std::vector<ControllerInterface::Device*>& devices, void* hwnd );
		
		class Keyboard : public ControllerInterface::Device
		{
			friend class ControllerInterface;
			friend class ControllerInterface::ControlReference;
			
		protected:
			
			struct State
			{
				char keyboard[256]; // really dumb
				// mouse crap will go here
			};
			
			class Input : public ControllerInterface::Device::Input
			{
				friend class Keyboard;
			protected:
				Input( const unsigned char index ) : m_index(index) {}
				virtual ControlState GetState( State* const js ) = 0;
				
				const unsigned char	m_index;
			};
			
			class Key : public Input
			{
				friend class Keyboard;
			public:
				std::string GetName() const;
			protected:
				Key( const unsigned char key ) : Input(key) {}
				ControlState GetState( State* const js );
				
			};
			
			bool UpdateInput();
			bool UpdateOutput();
			
			ControlState GetInputState( const ControllerInterface::Device::Input* const input );
			void SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state );
			
		public:
			Keyboard( void* view );
			~Keyboard();
			
			std::string GetName() const;
			std::string GetSource() const;
			int GetId() const;
			
		private:
			State			m_state;
		};
		
	}
}

#endif
