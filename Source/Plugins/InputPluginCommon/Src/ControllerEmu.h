#ifndef _CONTROLLEREMU_H_
#define _CONTROLLEREMU_H_

// windows crap
#define NOMINMAX

#include "pluginspecs_pad.h"
#include "pluginspecs_wiimote.h"
//#include <CommonTypes.h>
#include <math.h>

#include <ControllerInterface/ControllerInterface.h>
#include "IniFile.h"

#include <vector>
#include <string>
#include <algorithm>

#define sign(x) ((x)?(x)<0?-1:1:0)

enum
{
	GROUP_TYPE_OTHER,
	GROUP_TYPE_STICK,
	GROUP_TYPE_MIXED_TRIGGERS,
	GROUP_TYPE_BUTTONS,
	GROUP_TYPE_FORCE,
	GROUP_TYPE_EXTENSION,
	GROUP_TYPE_TILT,
	GROUP_TYPE_CURSOR,
	GROUP_TYPE_TRIGGERS,
};

const char * const named_directions[] = 
{
	"Up",
	"Down",
	"Left",
	"Right"
};

void GetMousePos(float& x, float& y, const SWiimoteInitialize* const wiimote_initialize);

class ControllerEmu
{
public:

	class ControlGroup
	{
	public:

		class Control
		{
		protected:
			Control( ControllerInterface::ControlReference* const _ref, const char * const _name )
				: control_ref(_ref), name(_name){}
		public:

			virtual ~Control();
			ControllerInterface::ControlReference*		const control_ref;
			const char * const		name;

		};

		class Input : public Control
		{
		public:

			Input( const char * const _name )
				: Control( new ControllerInterface::InputReference, _name ) {}

		};

		class Output : public Control
		{
		public:

			Output( const char * const _name )
				: Control( new ControllerInterface::OutputReference, _name ) {}

		};

		class Setting
		{
		public:

			Setting(const char* const _name, const ControlState def_value
				, const unsigned int _low = 1, const unsigned int _high = 100 )
				: name(_name)
				, value(def_value)
				, default_value(def_value)
				, low(_low)
				, high(_high){}

			const char* const	name;
			ControlState		value;
			const ControlState	default_value;
			const unsigned int	low, high;
		};

		ControlGroup( const char* const _name, const unsigned int _type = GROUP_TYPE_OTHER ) : name(_name), type(_type) {}
		virtual ~ControlGroup();
	
		void LoadConfig( IniFile::Section& sec, const std::string& defdev = "", const std::string& base = "" );
		void SaveConfig( IniFile::Section& sec, const std::string& defdev = "", const std::string& base = "" );

		const char* const			name;
		const unsigned int			type;

		std::vector< Control* >		controls;
		std::vector< Setting* >		settings;

	};

	class AnalogStick : public ControlGroup
	{
	public:

		template <typename C>
		void GetState( C* const x, C* const y, const unsigned int base, const unsigned int range )
		{
			// this is all a mess

			ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
			ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

			ControlState deadzone = settings[0]->value;
			ControlState square = settings[1]->value;
			ControlState m = controls[4]->control_ref->State();

			// modifier code
			if ( m )
			{
				yy = (abs(yy)>deadzone) * sign(yy) * (m + deadzone/2);
				xx = (abs(xx)>deadzone) * sign(xx) * (m + deadzone/2);
			}

			// deadzone / square stick code
			if ( deadzone || square )
			{
				// this section might be all wrong, but its working good enough, i think

				ControlState ang = atan2( yy, xx ); 
				ControlState ang_sin = sin(ang);
				ControlState ang_cos = cos(ang);

				// the amt a full square stick would have at current angle
				ControlState square_full = std::min( ang_sin ? 1/abs(ang_sin) : 2, ang_cos ? 1/abs(ang_cos) : 2 );

				// the amt a full stick would have that was ( user setting squareness) at current angle
				// i think this is more like a pointed circle rather than a rounded square like it should be
				ControlState stick_full = ( 1 + ( square_full - 1 ) * square );

				ControlState dist = sqrt(xx*xx + yy*yy);

				// dead zone code
				dist = std::max( 0.0f, dist - deadzone * stick_full );
				dist /= ( 1 - deadzone );

				// square stick code
				ControlState amt = dist / stick_full;
				dist -= ((square_full - 1) * amt * square);

				yy = std::max( -1.0f, std::min( 1.0f, ang_sin * dist ) );
				xx = std::max( -1.0f, std::min( 1.0f, ang_cos * dist ) );
			}

			*y = C( yy * range + base );
			*x = C( xx * range + base );
		}

		AnalogStick( const char* const _name );

	};

	class Buttons : public ControlGroup
	{
	public:
		Buttons( const char* const _name );

		template <typename C>
		void GetState( C* const buttons, const C* bitmasks )
		{
			std::vector<Control*>::iterator i = controls.begin(),
				e = controls.end();
			for ( ; i!=e; ++i, ++bitmasks )
				if ( (*i)->control_ref->State() > settings[0]->value ) // threshold
					*buttons |= *bitmasks;
		}

	};

	class MixedTriggers : public ControlGroup
	{
	public:

		template <typename C, typename S>
		void GetState( C* const digital, const C* bitmasks, S* analog, const unsigned int range )
		{
			const unsigned int trig_count = ((unsigned int) (controls.size() / 2));
			for ( unsigned int i=0; i<trig_count; ++i,++bitmasks,++analog )
			{
				if ( controls[i]->control_ref->State() > settings[0]->value ) //threshold
				{
					*analog = range;
					*digital |= *bitmasks;
				}
				else
					*analog = S(controls[i+trig_count]->control_ref->State() * range);
					
			}
		}

		MixedTriggers( const char* const _name );

	};

	class Triggers : public ControlGroup
	{
	public:

		template <typename S>
		void GetState( S* analog, const unsigned int range )
		{
			const unsigned int trig_count = ((unsigned int) (controls.size()));
			const ControlState deadzone = settings[0]->value;
			for ( unsigned int i=0; i<trig_count; ++i,++analog )
				*analog = S( std::max(controls[i]->control_ref->State() - deadzone, 0.0f) / (1 - deadzone) * range );
		}

		Triggers( const char* const _name );

	};

	class Force : public ControlGroup
	{
	public:
		Force( const char* const _name );

		template <typename C, typename R>
		void GetState( C* axis, const u8 base, const R range )
		{
			const float deadzone = settings[0]->value;
			for ( unsigned int i=0; i<6; i+=2 )
			{
				const float state = controls[i+1]->control_ref->State() - controls[i]->control_ref->State();
				if (abs(state) > deadzone)
					*axis++ = (C)((state - (deadzone * sign(state))) / (1 - deadzone) * range + base);
					//*axis++ = state * range + base;
				else
					*axis++ = (C)(base);
			}
		}
	};

	class Tilt : public ControlGroup
	{
	public:
		Tilt( const char* const _name );

		template <typename C, typename R>
		void GetState( C* const x, C* const y, const unsigned int base, const R range )
		{
			// this is all a mess

			ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
			ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

			ControlState deadzone = settings[0]->value;
			ControlState circle = settings[1]->value;
			ControlState m = controls[4]->control_ref->State();

			// modifier code
			if ( m )
			{
				yy = (abs(yy)>deadzone) * sign(yy) * (m + deadzone/2);
				xx = (abs(xx)>deadzone) * sign(xx) * (m + deadzone/2);
			}

			// deadzone / circle stick code
			if ( deadzone || circle )
			{
				// this section might be all wrong, but its working good enough, i think

				ControlState ang = atan2( yy, xx ); 
				ControlState ang_sin = sin(ang);
				ControlState ang_cos = cos(ang);

				// the amt a full square stick would have at current angle
				ControlState square_full = std::min( ang_sin ? 1/abs(ang_sin) : 2, ang_cos ? 1/abs(ang_cos) : 2 );

				// the amt a full stick would have that was ( user setting circular ) at current angle
				// i think this is more like a pointed circle rather than a rounded square like it should be
				ControlState stick_full = (square_full * (1 - circle)) + (circle);

				ControlState dist = sqrt(xx*xx + yy*yy);

				// dead zone code
				dist = std::max( 0.0f, dist - deadzone * stick_full );
				dist /= (1 - deadzone);

				// circle stick code
				ControlState amt = dist / stick_full;
				dist += (square_full - 1) * amt * circle;

				yy = std::max( -1.0f, std::min( 1.0f, ang_sin * dist ) );
				xx = std::max( -1.0f, std::min( 1.0f, ang_cos * dist ) );
			}

			*y = C( yy * range + base );
			*x = C( xx * range + base );
		}
	};

	class Cursor : public ControlGroup
	{
	public:
		Cursor( const char* const _name, const SWiimoteInitialize* const _wiimote_initialize );

		template <typename C>
		void GetState( C* const x, C* const y, C* const forward, const bool adjusted = false )
		{
			const ControlState z = controls[4]->control_ref->State();
			
			// hide
			if (controls[5]->control_ref->State() > 0.5f)
			{
				*x = 10000; *y = 0; *forward = 0;
			}
			else
			{
				*forward = z;
				float xx, yy;
				GetMousePos(xx, yy, wiimote_initialize);

				// use mouse cursor, or user defined mapping if they have something mapped
				// this if seems horrible
				if ( controls[0]->control_ref->control_qualifier.name.size() || controls[1]->control_ref->control_qualifier.name.size() )
					yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
				else
					yy = -yy;

				if ( controls[2]->control_ref->control_qualifier.name.size() || controls[3]->control_ref->control_qualifier.name.size() )
					xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

				// adjust cursor according to settings
				if (adjusted)
				{
					xx *= ( settings[1]->value * 2 );
					yy *= ( settings[2]->value * 2 );
					yy += ( settings[0]->value - 0.5f );
				}

				*x = xx;
				*y = yy;
			}
		}

	private:
		const SWiimoteInitialize* const wiimote_initialize;

	};

	class Extension : public ControlGroup
	{
	public:
		Extension( const char* const _name )
			: ControlGroup( _name, GROUP_TYPE_EXTENSION )
			, switch_extension(0)
			, active_extension(0) {}
		~Extension();

		void GetState( u8* const data, const bool focus = true );

		std::vector<ControllerEmu*>		attachments;

		int	switch_extension;
		int	active_extension;
	};

	virtual ~ControllerEmu();

	virtual std::string GetName() const = 0;

	void LoadConfig( IniFile::Section& sec, const std::string& base = "" );
	void SaveConfig( IniFile::Section& sec, const std::string& base = "" );
	void UpdateDefaultDevice();

	void UpdateReferences( ControllerInterface& devi );

	std::vector< ControlGroup* >		groups;

	ControllerInterface::DeviceQualifier	default_device;

};


#endif
