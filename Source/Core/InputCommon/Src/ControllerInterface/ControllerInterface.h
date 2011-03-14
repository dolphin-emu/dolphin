#ifndef _DEVICEINTERFACE_H_
#define _DEVICEINTERFACE_H_

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>

#include "Common.h"
#include "Thread.h"

// enable disable sources
#ifdef _WIN32
	#define CIFACE_USE_XINPUT
	#define CIFACE_USE_DINPUT_JOYSTICK
	#define CIFACE_USE_DINPUT_KBM
	#define CIFACE_USE_DINPUT
//#ifndef CIFACE_USE_DINPUT_JOYSTICK
// enable SDL 1.2 in addition to DirectInput on windows,
// to support a few gamepads that aren't behaving with DInput
	#define CIFACE_USE_SDL
//#endif
#endif
#if defined(HAVE_X11) && HAVE_X11
	#define CIFACE_USE_XLIB
	#define CIFACE_USE_SDL
#endif
#if defined(__APPLE__)
	#define CIFACE_USE_OSX
#endif

// idk in case i wanted to change it to double or somethin, idk what's best
typedef float ControlState;

//
//		ControllerInterface
//
// some crazy shit i made to control different device inputs and outputs
// from lots of different sources, hopefully more easily
//
class ControllerInterface
{
public:

	// Forward declarations
	class DeviceQualifier;

	//
	//		Device
	//
	// a device class
	//
	class Device
	{
	public:
		class Input;
		class Output;

		//
		//		Control
		//
		//  control includes inputs and outputs
		//
		class Control		// input or output
		{
		public:
			virtual std::string GetName() const = 0;
			virtual ~Control() {}

			virtual Input* ToInput() { return NULL; }
			virtual Output* ToOutput() { return NULL; }
		};

		//
		//		Input
		//
		// an input on a device
		//
		class Input : public Control
		{
		public:
			// things like absolute axes/ absolute mouse position will override this
			virtual bool IsDetectable() { return true; }

			virtual ControlState GetState() const = 0;

			Input* ToInput() { return this; }
		};
		
		//
		//		Output
		//
		// an output on a device
		//
		class Output : public Control
		{
		public:
			virtual ~Output() {}

			virtual void SetState(ControlState state) = 0;

			Output* ToOutput() { return this; }
		};

		virtual ~Device();
		
		virtual std::string GetName() const = 0;
		virtual int GetId() const = 0;
		virtual std::string GetSource() const = 0;
		virtual bool UpdateInput() = 0;
		virtual bool UpdateOutput() = 0;

		virtual void ClearInputState();

		const std::vector<Input*>& Inputs() const { return m_inputs; }
		const std::vector<Output*>& Outputs() const { return m_outputs; }

		Input* FindInput(const std::string& name) const;
		Output* FindOutput(const std::string& name) const;

	protected:
		void AddInput(Input* const i);
		void AddOutput(Output* const o);

	private:
		std::vector<Input*>		m_inputs;
		std::vector<Output*>	m_outputs;
	};

	//
	//		DeviceQualifier
	//
	// device qualifier used to match devices
	// currently has ( source, id, name ) properties which match a device
	//
	class DeviceQualifier
	{
	public:
		DeviceQualifier() : cid(-1) {}
		DeviceQualifier(const std::string& _source, const int _id, const std::string& _name)
			: source(_source), cid(_id), name(_name) {}
		void FromDevice(const Device* const dev);
		void FromString(const std::string& str);
		std::string ToString() const;
		bool operator==(const DeviceQualifier& devq) const;
		bool operator==(const Device* const dev) const;

		std::string		source;
		int				cid;
		std::string		name;
	};

	//
	//		ControlReference
	//
	// these are what you create to actually use the inputs, InputReference or OutputReference
	//
	// after being binded to devices and controls with ControllerInterface::UpdateReference,
	//		each one can link to multiple devices and controls
	//		when you change a ControlReference's expression,
	//		you must use ControllerInterface::UpdateReference on it to rebind controls
	//
	class ControlReference
	{
		friend class ControllerInterface;

	public:
		virtual ~ControlReference() {}

		virtual ControlState State(const ControlState state = 0) = 0;
		virtual Device::Control* Detect(const unsigned int ms, Device* const device) = 0;
		size_t BoundCount() const { return m_controls.size(); }

		ControlState		range;
		std::string			expression;
		const bool			is_input;

	protected:
		ControlReference(const bool _is_input) : range(1), is_input(_is_input) {}

		struct DeviceControl
		{
			DeviceControl() : control(NULL), mode(0) {}

			Device::Control*	control;
			int		mode;
		};

		std::vector<DeviceControl>	m_controls;
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

	Device::Input* FindInput(const std::string& name, const Device* def_dev) const;
	Device::Output* FindOutput(const std::string& name, const Device* def_dev) const;

	const std::vector<Device*>& Devices() const { return m_devices; }
	Device* FindDevice(const DeviceQualifier& devq) const;

	std::recursive_mutex update_lock;

private:
	bool					m_is_init;
	std::vector<Device*>	m_devices;
	void*					m_hwnd;
};

typedef std::vector<ControllerInterface::Device*> DeviceList;

extern ControllerInterface g_controller_interface;

#endif
