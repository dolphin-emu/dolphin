#ifndef _CONTROLLEREMU_H_
#define _CONTROLLEREMU_H_

// windows crap
#define NOMINMAX

#include "pluginspecs_pad.h"
#include "pluginspecs_wiimote.h"
//#include <CommonTypes.h>
#include <math.h>

#include "ControllerInterface/ControllerInterface.h"
#include "IniFile.h"

#include <vector>
#include <string>
#include <algorithm>

#define sign(x) ((x)?(x)<0?-1:1:0)

enum
{
	GROUP_TYPE_OTHER
	,GROUP_TYPE_STICK
	,GROUP_TYPE_MIXED_TRIGGERS
	,GROUP_TYPE_BUTTONS
};

const char * const named_directions[] = 
{
	"Up",
	"Down",
	"Left",
	"Right"
};

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

			
			//virtual std::string GetName() const = 0;

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

			Setting(const char* const _name, const float def_value ) : name(_name), value(def_value), default_value(def_value) {}

			const char* const	name;
			ControlState		value;
			const ControlState	default_value;
		};

		ControlGroup( const char* const _name, const unsigned int _type = GROUP_TYPE_OTHER ) : name(_name), type(_type) {}
		virtual ~ControlGroup();
	
		//ControlGroup( const ControlGroup& ctrl );

		//const unsigned int		type;
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
				yy = (abs(yy)>deadzone) * sign(yy) * m;
				xx = (abs(xx)>deadzone) * sign(xx) * m;
			}

			// deadzone / square stick code
			if ( deadzone || square )
			{
				// this section might be all wrong, but its working good enough, i think

				ControlState ang = atan2( yy, xx ); 
				ControlState ang_sin = sin(ang);
				ControlState ang_cos = cos(ang);

				// the amt a full square stick would have at current angle
				ControlState square_full = std::min( 1/abs(ang_sin), 1/abs(ang_cos) );

				// the amt a full stick would have that was ( user setting squareness) at current angle
				// i think this is more like a pointed circle rather than a rounded square like it should be
				ControlState stick_full = ( 1 + ( square_full - 1 ) * square );

				ControlState dist = sqrt(xx*xx + yy*yy);

				// dead zone code
				dist = std::max( 0.0f, dist - deadzone * stick_full );
				dist /= ( 1 - deadzone );

				// square stick code
				ControlState amt = ( dist ) / stick_full;
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

	virtual ~ControllerEmu();

	virtual std::string GetName() const = 0;

	void LoadConfig( IniFile::Section& sec );
	void SaveConfig( IniFile::Section& sec );
	void UpdateDefaultDevice();

	void UpdateReferences( ControllerInterface& devi );

	std::vector< ControlGroup* >		groups;


	ControllerInterface::DeviceQualifier	default_device;

};




#endif
