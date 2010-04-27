#pragma once

#include "../ControllerInterface.h"
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{

void Init( std::vector<ControllerInterface::Device*>& devices );
void DeInit();

class Keyboard : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;
	
protected:
	
	class Input : public ControllerInterface::Device::Input
	{
		friend class Keyboard;
	protected:
		virtual ControlState GetState() = 0;
	};
	
	class Key : public Input
	{
		friend class Keyboard;
	public:
		std::string GetName() const;
	protected:
		Key( IOHIDElementRef key_element );
		ControlState GetState();
	private:
		IOHIDElementRef	m_key_element;
		std::string		m_key_name;
	};
	
	bool UpdateInput();
	bool UpdateOutput();
	
	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state );
	
public:
	Keyboard(IOHIDDeviceRef device, int index);
	
	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;
	
private:
	IOHIDDeviceRef	m_device;
	int				m_device_index;
	std::string		m_device_name;
};

}
}
