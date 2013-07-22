#ifndef _DEVICEINTERFACE_H_
#define _DEVICEINTERFACE_H_

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>

#include "Common.h"
#include "Thread.h"
#include "ExpressionParser.h"
#include "Device.h"

// enable disable sources
#ifdef _WIN32
	#define CIFACE_USE_XINPUT
	#define CIFACE_USE_DINPUT
	#define CIFACE_USE_SDL
#endif
#if defined(HAVE_X11) && HAVE_X11
	#define CIFACE_USE_XLIB
	#define CIFACE_USE_SDL
	#if defined(HAVE_X11_XINPUT2) && HAVE_X11_XINPUT2
		#define CIFACE_USE_X11_XINPUT2
	#endif
#endif
#if defined(__APPLE__)
	#define CIFACE_USE_OSX
#endif
#ifdef ANDROID
	#define CIFACE_USE_ANDROID
#endif

using namespace ciface::Core;

//
//		ControllerInterface
//
// some crazy shit I made to control different device inputs and outputs
// from lots of different sources, hopefully more easily
//
class ControllerInterface : public DeviceContainer
{
public:

	//
	//		ControlReference
	//
	// these are what you create to actually use the inputs, InputReference or OutputReference
	//
	// after being bound to devices and controls with ControllerInterface::UpdateReference,
	//		each one can link to multiple devices and controls
	//		when you change a ControlReference's expression,
	//		you must use ControllerInterface::UpdateReference on it to rebind controls
	//
	class ControlReference
	{
		friend class ControllerInterface;
	public:
		virtual ControlState State(const ControlState state = 0) = 0;
		virtual Device::Control* Detect(const unsigned int ms, Device* const device) = 0;

		ControlState range;
		std::string			expression;
		const bool			is_input;
		ciface::ExpressionParser::ExpressionParseStatus parse_error;

		virtual ~ControlReference() {
			delete parsed_expression;
		}

		int BoundCount() {
			if (parsed_expression)
				return parsed_expression->num_controls;
			else
				return 0;
		}

	protected:
		ControlReference(const bool _is_input) : range(1), is_input(_is_input), parsed_expression(NULL) {}
		ciface::ExpressionParser::Expression *parsed_expression;
	};

	//
	//		InputReference
	//
	// control reference for inputs
	//
	class InputReference : public ControlReference
	{
	public:
		InputReference() : ControlReference(true) {}
		ControlState State(const ControlState state);
		Device::Control* Detect(const unsigned int ms, Device* const device);
	};

	//
	//		OutputReference
	//
	// control reference for outputs
	//
	class OutputReference : public ControlReference
	{
	public:
		OutputReference() : ControlReference(false) {}
		ControlState State(const ControlState state);
		Device::Control* Detect(const unsigned int ms, Device* const device);
	};

	ControllerInterface() : m_is_init(false), m_hwnd(NULL) {}
	
	void SetHwnd(void* const hwnd);
	void Initialize();
	void Shutdown();
	bool IsInit() const { return m_is_init; }

	void UpdateReference(ControlReference* control, const DeviceQualifier& default_device) const;
	bool UpdateInput(const bool force = false);
	bool UpdateOutput(const bool force = false);

	std::recursive_mutex update_lock;

private:
	bool					m_is_init;
	void*					m_hwnd;
};

extern ControllerInterface g_controller_interface;

#endif
