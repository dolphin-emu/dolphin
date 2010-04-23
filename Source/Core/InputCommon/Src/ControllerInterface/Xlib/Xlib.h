#ifndef _CIFACE_XLIB_H_
#define _CIFACE_XLIB_H_

#include "../ControllerInterface.h"

#include <X11/Xlib.h>

namespace ciface
{
namespace Xlib
{

void Init(std::vector<ControllerInterface::Device*>& devices, void* const hwnd);

class Keyboard : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

	protected:

	struct State
	{
		char keyboard[32];
		unsigned int buttons;
	};

	class Input : public ControllerInterface::Device::Input
	{
		friend class Keyboard;

		protected:
		virtual ControlState GetState(const State* const state) = 0;
	};

	class Key : public Input
	{
		friend class Keyboard;

		public:
		std::string GetName() const;

		protected:
		Key(Display* const display, KeyCode keycode);
		ControlState GetState(const State* const state);

		private:
		Display* const	m_display;
		const KeyCode	m_keycode;
		std::string		m_keyname;

	};

	class Button : public Input
	{
		friend class Keyboard;

		public:
		std::string GetName() const;

		protected:
		Button(const unsigned int index) : m_index(index) {}
		ControlState GetState(const State* const state);

		private:
		const unsigned int m_index;
	};

	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState(const ControllerInterface::Device::Input* const input);
	void SetOutputState(const ControllerInterface::Device::Output* const output, const ControlState state);

	public:
	Keyboard(Display* display);
	~Keyboard();

	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;

	private:
	Window			m_window;
	Display*		m_display;
	State			m_state;
};

}
}

#endif
