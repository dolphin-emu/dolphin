#pragma once

#include "../ControllerInterface.h"
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{

void Init( std::vector<ControllerInterface::Device*>& devices );
void DeInit();

class KeyboardMouse : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;
	
protected:
	
	struct State
	{
		unsigned char buttons[32];
	};
	
	class Input : public ControllerInterface::Device::Input
	{
		friend class KeyboardMouse;
	protected:
		virtual ControlState GetState( const State* const state ) = 0;
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
		Key( IOHIDElementRef key_element );
		ControlState GetState( const State* const state );
	private:
		IOHIDElementRef m_key_element;
		std::string m_key_name;
	};
	
	class Button : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Button( IOHIDElementRef button_element );
		ControlState GetState( const State* const state );
	private:
		IOHIDElementRef m_button_element;
		std::string m_button_name;
	};
	
	class Axis : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Axis( IOHIDElementRef button_element );
		ControlState GetState( const State* const state );
	private:
		IOHIDElementRef m_axis_element;
		std::string m_axis_name;
	};
	
	class Light : public Output
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
	protected:
		Light( IOHIDElementRef light_element );
		void SetState( const ControlState state, unsigned char* const state_out );
	private:
		IOHIDElementRef m_light_element;
		std::string m_light_name;
	};
	
	bool UpdateInput();
	bool UpdateOutput();
	
	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state );
	
public:
	KeyboardMouse(IOHIDDeviceRef device);
	~KeyboardMouse();
	
	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;
	
private:
	State			m_state_in;
	unsigned char	m_state_out[6]; // ugly
	IOHIDDeviceRef	m_device;
	std::string		m_device_name;
};

}
}
