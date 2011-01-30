#include <IOKit/hid/IOHIDLib.h>

#include "../ControllerInterface.h"

namespace ciface
{
namespace OSX
{

class Joystick : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:
	class Input : public ControllerInterface::Device::Input
	{
		friend class Joystick;
	protected:
		virtual ControlState GetState(IOHIDDeviceRef device) const = 0;
	};

	class Button : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Button(IOHIDElementRef element);
		ControlState GetState(IOHIDDeviceRef device) const;
	private:
		IOHIDElementRef		m_element;
		std::string		m_name;
	};

	class Axis : public Input
	{
		friend class Joystick;
	public:
		enum direction {
			positive = 0,
			negative
		};
		std::string GetName() const;
	protected:
		Axis(IOHIDElementRef element, direction dir);
		ControlState GetState(IOHIDDeviceRef device) const;
	private:
		IOHIDElementRef		m_element;
		std::string		m_name;
		direction		m_direction;
		float			m_neutral;
		float			m_scale;
	};

	class Hat : public Input
	{
		friend class Joystick;
	public:
		enum direction {
			up = 0,
			right,
			down,
			left
		};
		std::string GetName() const;
	protected:
		Hat(IOHIDElementRef element, direction dir);
		ControlState GetState(IOHIDDeviceRef device) const;
	private:
		IOHIDElementRef		m_element;
		std::string		m_name;
		direction		m_direction;
	};

	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState(
		const ControllerInterface::Device::Input* const input) const;
	void SetOutputState(
		const ControllerInterface::Device::Output* const output,
		const ControlState state);

public:
	Joystick(IOHIDDeviceRef device, std::string name, int index);

	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;

private:
	IOHIDDeviceRef	m_device;
	std::string	m_device_name;
	int		m_index;
};

}
}
