#pragma once

#include "../ControllerInterface.h"
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{

class Mouse : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;
	
protected:
	
	class Input : public ControllerInterface::Device::Input
	{
		friend class Mouse;
	protected:
		virtual ControlState GetState(IOHIDDeviceRef device) = 0;
	};
	
	class Button : public Input
	{
		friend class Mouse;
	public:
		std::string GetName() const;
	protected:
		Button( IOHIDElementRef element );
		ControlState GetState(IOHIDDeviceRef device);
	private:
		IOHIDElementRef	m_element;
		std::string		m_name;
	};
	
	class Axis : public Input
	{
		friend class Mouse;
	public:
		enum direction {
			positive = 0,
			negative
		};
		std::string GetName() const;
	protected:
		Axis( IOHIDElementRef element, direction dir );
		ControlState GetState(IOHIDDeviceRef device);
	private:
		IOHIDElementRef	m_element;
		std::string		m_name;
		direction		m_direction;
		float			m_range;
	};
	
	bool UpdateInput();
	bool UpdateOutput();
	
	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state );
	
public:
	Mouse(IOHIDDeviceRef device);
	
	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;
	
private:
	IOHIDDeviceRef	m_device;
	std::string		m_device_name;
};

}
}
