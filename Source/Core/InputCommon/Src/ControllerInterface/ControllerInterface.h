#ifndef _DEVICEINTERFACE_H_
#define _DEVICEINTERFACE_H_

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include "Common.h"

// enable disable sources
#ifdef _WIN32
	#define CIFACE_USE_XINPUT
	#define CIFACE_USE_DIRECTINPUT_JOYSTICK
	#define CIFACE_USE_DIRECTINPUT_KBM
	#define CIFACE_USE_DIRECTINPUT
#endif
#if defined(HAVE_X11) && HAVE_X11
// Xlib is not tested at all currently, it is like 80% complete at least though
	#define CIFACE_USE_XLIB
#endif
#ifndef CIFACE_USE_DIRECTINPUT_JOYSTICK
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

	//
	//		Device
	//
	// Pretty obviously, a device class
	//
	class Device
	{
	public:

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
		};

		//
		//		Input
		//
		// an input on a device
		//
		class Input : public Control
		{
		public:
			virtual ~Input() {}
		};
		
		//
		//		Output
		//
		// guess wut it is, yup and output
		//
		class Output : public Control
		{
		public:
			virtual ~Output() {}
		};

		virtual ~Device();
		
		virtual std::string GetName() const = 0;
		virtual int GetId() const = 0;
		virtual std::string GetSource() const = 0;

		virtual ControlState GetInputState( const Input* const input ) = 0;
		virtual void SetOutputState( const Output* const output, const ControlState state ) = 0;

		virtual bool UpdateInput() = 0;
		virtual bool UpdateOutput() = 0;

		virtual void ClearInputState();

		const std::vector< Input* >& Inputs();
		const std::vector< Output* >& Outputs();

	protected:
		std::vector<Input*>		inputs;
		std::vector<Output*>	outputs;

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
		DeviceQualifier() : cid(-1){}
		DeviceQualifier( const std::string& _source, const int _id, const std::string& _name )
			: source(_source), cid(_id), name(_name) {}
		bool operator==(const Device* const dev) const;
		void FromDevice(const Device* const dev);
		void FromString(const std::string& str);
		std::string ToString() const;

		std::string		source;
		int				cid;
		std::string		name;
	};

	//
	//		ControlQualifier
	//
	// control qualifier includes input and output qualifiers
	// used to match controls on devices, only has name property
	// |input1|input2| form as well, || matches anything, might change this to * or something
	//
	class ControlQualifier
	{
	public:
		ControlQualifier() {};
		ControlQualifier( const std::string& _name ) : name(_name) {}
		virtual ~ControlQualifier() {}

		virtual bool operator==(const Device::Control* const in) const;
		void FromControl(const Device::Control* const in);

		std::string		name;
	};

	//
	//		InputQualifier
	//
	// ControlQualifier for inputs
	//
	class InputQualifier : public ControlQualifier
	{
	public:
		InputQualifier() {};
		InputQualifier( const std::string& _name ) : ControlQualifier(_name) {}
	};

	//
	//		OutputQualifier
	//
	// ControlQualifier for outputs
	//
	class OutputQualifier : public ControlQualifier
	{
	public:
		OutputQualifier() {};
		OutputQualifier( const std::string& _name ) : ControlQualifier(_name) {}
	};

	//
	//		ControlReference
	//
	// these are what you create to actually use the inputs, InputReference or OutputReference
	// they have a vector < struct { device , vector < controls > } >
	//
	// after being binded to devices and controls with ControllerInterface::UpdateReference,
	//		each one can binded to a devices, and 0+ controls the device
	// ControlReference can update its own controls when you change its control qualifier
	//		using ControlReference::UpdateControls but when you change its device qualifer
	//		you must use ControllerInterface::UpdateReference
	//
	class ControlReference
	{
	public:
		ControlReference( const bool _is_input ) : range(1), is_input(_is_input), device(NULL) {}
		virtual ~ControlReference() {}

		virtual ControlState State( const ControlState state = 0 ) = 0;
		virtual bool Detect( const unsigned int ms, const unsigned int count = 1 ) = 0;
		virtual void UpdateControls() = 0;

		ControlState		range;

		DeviceQualifier		device_qualifier;
		ControlQualifier	control_qualifier;

		const bool			is_input;
		Device*				device;

		std::vector<Device::Control*>	controls;

	};

	//
	//		InputReference
	//
	// control reference for inputs
	//
	class InputReference : public ControlReference
	{
	public:
		InputReference() : ControlReference( true ) {}
		ControlState State( const ControlState state );
		bool Detect( const unsigned int ms, const unsigned int count );
		void UpdateControls();

		unsigned int		mode;
	};

	//
	//		OutputReference
	//
	// control reference for outputs
	//
	class OutputReference : public ControlReference
	{
	public:
		OutputReference() : ControlReference( false ) {}
		ControlState State( const ControlState state );
		bool Detect( const unsigned int ms, const unsigned int count );
		void UpdateControls();
	};

	ControllerInterface() : m_is_init(false) {}
	
	void SetHwnd( void* const hwnd );
	void Init();
	void DeInit();
	bool IsInit();

	void UpdateReference( ControlReference* control );
	bool UpdateInput();
	bool UpdateOutput();

	const std::vector<Device*>& Devices();

private:
	bool					m_is_init;
	std::vector<Device*>	m_devices;
	void*					m_hwnd;
};

#endif
