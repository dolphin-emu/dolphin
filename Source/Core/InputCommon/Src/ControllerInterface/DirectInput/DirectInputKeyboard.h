#ifndef _CIFACE_DIRECTINPUT_KEYBOARD_H_
#define _CIFACE_DIRECTINPUT_KEYBOARD_H_

#include "../ControllerInterface.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#include <wx/stopwatch.h>
#include <wx/utils.h>

namespace ciface
{
namespace DirectInput
{

void InitKeyboard(IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices);

class Keyboard : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

	struct State
	{
		
	};

	class Input : public ControllerInterface::Device::Input
	{
		friend class Keyboard;
	protected:
		virtual ControlState GetState(const BYTE keys[]) const = 0;
	};

	class Output : public ControllerInterface::Device::Output
	{
		friend class Keyboard;
	protected:
		virtual void SetState(const ControlState state, unsigned char* const state_out) = 0;
	};

	class Key : public Input
	{
		friend class Keyboard;
	public:
		std::string GetName() const;
	protected:
		Key(const unsigned int index) : m_index(index) {}
		ControlState GetState(const BYTE keys[]) const;
	private:
		const unsigned int	m_index;
	};

	class Light : public Output
	{
		friend class Keyboard;
	public:
		std::string GetName() const;
	protected:
		Light(const unsigned int index) : m_index(index) {}
		void SetState(const ControlState state, unsigned char* const state_out);
	private:
		const unsigned int	m_index;
	};
	
	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState( const ControllerInterface::Device::Input* const input ) const;
	void SetOutputState( const ControllerInterface::Device::Output* const input, const ControlState state );

public:
	Keyboard(const LPDIRECTINPUTDEVICE8 kb_device, const int index);
	~Keyboard();

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const LPDIRECTINPUTDEVICE8	m_kb_device;
	const int			m_index;

	wxLongLong			m_last_update;
	BYTE				m_state_in[256];
	unsigned char		m_state_out[3];	// NUM CAPS SCROLL
	bool				m_current_state_out[3];	// NUM CAPS SCROLL
};

}
}

#endif
