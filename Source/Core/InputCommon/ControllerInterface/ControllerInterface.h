// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/ExpressionParser.h"

// enable disable sources
#ifdef _WIN32
	#define CIFACE_USE_XINPUT
	#define CIFACE_USE_DINPUT
#endif
#if defined(HAVE_X11) && HAVE_X11
	#define CIFACE_USE_XLIB
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
#if defined(HAVE_SDL) && HAVE_SDL
	#define CIFACE_USE_SDL
#endif

//
// ControllerInterface
//
// Some crazy shit I made to control different device inputs and outputs
// from lots of different sources, hopefully more easily.
//
class ControllerInterface : public ciface::Core::DeviceContainer
{
public:

	//
	// ControlReference
	//
	// These are what you create to actually use the inputs, InputReference or OutputReference.
	//
	// After being bound to devices and controls with ControllerInterface::UpdateReference,
	// each one can link to multiple devices and controls
	// when you change a ControlReference's expression,
	// you must use ControllerInterface::UpdateReference on it to rebind controls
	//
	class ControlReference
	{
		friend class ControllerInterface;
	public:
		virtual ControlState State(const ControlState state = 0) = 0;
		virtual ciface::Core::Device::Control* Detect(const unsigned int ms, ciface::Core::Device* const device) = 0;

		ControlState range;
		std::string  expression;
		const bool   is_input;
		ciface::ExpressionParser::ExpressionParseStatus parse_error;

		virtual ~ControlReference()
		{
			delete parsed_expression;
		}

		int BoundCount()
		{
			if (parsed_expression)
				return parsed_expression->num_controls;
			else
				return 0;
		}

	protected:
		ControlReference(const bool _is_input) : range(1), is_input(_is_input), parsed_expression(nullptr) {}
		ciface::ExpressionParser::Expression *parsed_expression;
	};

	//
	// InputReference
	//
	// Control reference for inputs
	//
	class InputReference : public ControlReference
	{
	public:
		InputReference() : ControlReference(true) {}
		ControlState State(const ControlState state) override;
		ciface::Core::Device::Control* Detect(const unsigned int ms, ciface::Core::Device* const device) override;
	};

	//
	// OutputReference
	//
	// Control reference for outputs
	//
	class OutputReference : public ControlReference
	{
	public:
		OutputReference() : ControlReference(false) {}
		ControlState State(const ControlState state) override;
		ciface::Core::Device::Control* Detect(const unsigned int ms, ciface::Core::Device* const device) override;
	};

	ControllerInterface() : m_is_init(false), m_hwnd(nullptr) {}

	void Initialize(void* const hwnd);
	void Reinitialize();
	void Shutdown();
	bool IsInit() const { return m_is_init; }

	void UpdateReference(ControlReference* control, const ciface::Core::DeviceQualifier& default_device) const;
	bool UpdateInput(const bool force = false);
	bool UpdateOutput(const bool force = false);

	std::recursive_mutex update_lock;

private:
	bool   m_is_init;
	void*  m_hwnd;
};

extern ControllerInterface g_controller_interface;
