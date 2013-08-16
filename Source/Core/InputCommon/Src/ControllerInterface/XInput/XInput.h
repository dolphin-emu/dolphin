#ifndef _CIFACE_XINPUT_H_
#define _CIFACE_XINPUT_H_

#include "../Device.h"

#define NOMINMAX
#include <Windows.h>
#include <XInput.h>

namespace ciface
{
namespace XInput
{

void Init(std::vector<Core::Device*>& devices);

class Device : public Core::Device
{
private:
	class Button : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, const WORD& buttons) : m_index(index), m_buttons(buttons) {}
		ControlState GetState() const;
	private:
		const WORD& m_buttons;
		u8 m_index;
	};

	class Axis : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Axis(u8 index, const SHORT& axis, SHORT range) : m_index(index), m_axis(axis), m_range(range) {}
		ControlState GetState() const;
	private:
		const SHORT& m_axis;
		const SHORT m_range;
		const u8 m_index;
	};

	class Trigger : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Trigger(u8 index, const BYTE& trigger, BYTE range) : m_index(index), m_trigger(trigger), m_range(range) {}
		ControlState GetState() const;
	private:
		const BYTE& m_trigger;
		const BYTE m_range;
		const u8 m_index;
	};

	class Motor : public Core::Device::Output
	{
	public:
		std::string GetName() const;
		Motor(u8 index, WORD& motor, WORD range) : m_index(index), m_motor(motor), m_range(range) {}
		void SetState(ControlState state);
	private:
		WORD& m_motor;
		const WORD m_range;
		const u8 m_index;
	};

public:
	bool UpdateInput();
	bool UpdateOutput();

	void ClearInputState();

	Device(const XINPUT_CAPABILITIES& capabilities, u8 index);

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	XINPUT_STATE m_state_in;
	XINPUT_VIBRATION m_state_out, m_current_state_out;
	const BYTE m_subtype;
	const u8 m_index;
};


}
}


#endif
