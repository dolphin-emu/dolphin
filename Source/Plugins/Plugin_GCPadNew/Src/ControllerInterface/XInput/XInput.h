#ifndef _CIFACE_XINPUT_H_
#define _CIFACE_XINPUT_H_

#include "../ControllerInterface.h"

#define NOMINMAX
#include <Windows.h>
#include <XInput.h>

namespace ciface
{
namespace XInput
{

void Init( std::vector<ControllerInterface::Device*>& devices );

class Device : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

	class Input : public ControllerInterface::Device::Input
	{
		friend class Device;
	protected:
		virtual ControlState GetState( const XINPUT_GAMEPAD* const gamepad ) = 0;
	};

	class Output : public ControllerInterface::Device::Output
	{
		friend class Device;
	protected:
		virtual void SetState( const ControlState state, XINPUT_VIBRATION* const vibration ) = 0;
	};

	class Button : public Input
	{
		friend class Device;
	public:
		std::string GetName() const;
	protected:
		Button( const unsigned int index ) : m_index(index) {}
		ControlState GetState( const XINPUT_GAMEPAD* const gamepad );
	private:
		const unsigned int	m_index;
	};

	class Axis : public Input
	{
		friend class Device;
	public:
		std::string GetName() const;
	protected:
		Axis( const unsigned int index, const SHORT range ) : m_index(index), m_range(range) {}
		ControlState GetState( const XINPUT_GAMEPAD* const gamepad );
	private:
		const unsigned int	m_index;
		const SHORT			m_range;
	};

	class Trigger : public Input
	{
		friend class Device;
	public:
		std::string GetName() const;
	protected:
		Trigger( const unsigned int index, const BYTE range ) : m_index(index), m_range(range) {}
		ControlState GetState( const XINPUT_GAMEPAD* const gamepad );
	private:
		const unsigned int	m_index;
		const BYTE			m_range;
	};

	class Motor : public Output
	{
		friend class Device;
	public:
		std::string GetName() const;
	protected:
		Motor( const unsigned int index, const WORD range ) : m_index(index), m_range(range) {}
		void SetState( const ControlState state, XINPUT_VIBRATION* const vibration );
	private:
		const unsigned int	m_index;
		const WORD			m_range;
	};

	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const input, const ControlState state );

	void ClearInputState();

public:
	Device( const XINPUT_CAPABILITIES* const capabilities, const unsigned int index );

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const unsigned int		m_index;
	XINPUT_STATE			m_state_in;
	XINPUT_VIBRATION		m_state_out;
	const unsigned int		m_subtype;
};


}
}


#endif
