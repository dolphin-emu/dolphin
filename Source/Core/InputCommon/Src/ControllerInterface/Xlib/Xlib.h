#ifndef _CIFACE_XLIB_H_
#define _CIFACE_XLIB_H_

#include "../ControllerInterface.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace ciface
{
namespace Xlib
{

void Init(std::vector<ControllerInterface::Device*>& devices, void* const hwnd);

class KeyboardMouse : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;
	
protected:
	
	struct State
	{
		char keyboard[32];
		unsigned int buttons;
		struct
		{
			float x, y;
		} cursor;
	};
	
	class Input : public ControllerInterface::Device::Input
	{
		friend class KeyboardMouse;
		
	protected:
		virtual ControlState GetState(const State* const state) const = 0;
	};
	
	class Key : public Input
	{
		friend class KeyboardMouse;
		
	public:
		std::string GetName() const;
		
	protected:
		Key(Display* const display, KeyCode keycode);
		ControlState GetState(const State* const state) const;
		
	private:
		Display* const	m_display;
		const KeyCode	m_keycode;
		std::string		m_keyname;
		
	};
	
	class Button : public Input
	{
		friend class KeyboardMouse;
		
	public:
		std::string GetName() const;
		
	protected:
		Button(const unsigned int index) : m_index(index) {}
		ControlState GetState(const State* const state) const;
		
	private:
		const unsigned int m_index;
	};

	class Cursor : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const;
		bool IsDetectable() { return false; }
	protected:
		Cursor(const unsigned int index, const bool positive) : m_index(index), m_positive(positive) {}
		ControlState GetState(const State* const state) const;
	private:
		const unsigned int	m_index;
		const bool			m_positive;
	};
	
	bool UpdateInput();
	bool UpdateOutput();
	
	ControlState GetInputState(const ControllerInterface::Device::Input* const input) const;
	void SetOutputState(const ControllerInterface::Device::Output* const output, const ControlState state);
	
public:
	KeyboardMouse(Window window);
	~KeyboardMouse();
	
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
