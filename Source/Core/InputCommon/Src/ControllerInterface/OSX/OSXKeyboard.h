#include <IOKit/hid/IOHIDLib.h>

#include "../ControllerInterface.h"

namespace ciface
{
namespace OSX
{

class Keyboard : public ControllerInterface::Device
{
private:
	class Key : public Input
	{
	public:
		std::string GetName() const;
		Key(IOHIDElementRef element, IOHIDDeviceRef device);
		ControlState GetState() const;
	private:
		const IOHIDElementRef	m_element;
		const IOHIDDeviceRef	m_device;
		std::string		m_name;
	};

public:
	bool UpdateInput();
	bool UpdateOutput();

	Keyboard(IOHIDDeviceRef device, std::string name, int index);

	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;

private:
	const IOHIDDeviceRef	m_device;
	const std::string	m_device_name;
	int		m_index;
};

}
}
