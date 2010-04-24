#include "ControllerEmu.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#endif

ControllerEmu::~ControllerEmu()
{
	// control groups
	std::vector<ControlGroup*>::const_iterator
		i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
		delete *i;
}

ControllerEmu::ControlGroup::~ControlGroup()
{
	// controls
	std::vector<Control*>::const_iterator
		ci = controls.begin(),
		ce = controls.end();
	for ( ; ci!=ce; ++ci )
		delete *ci;

	// settings
	std::vector<Setting*>::const_iterator
		si = settings.begin(),
		se = settings.end();
	for ( ; si!=se; ++si )
		delete *si;
}

ControllerEmu::Extension::~Extension()
{
	// attachments
	std::vector<ControllerEmu*>::const_iterator
		ai = attachments.begin(),
		ae = attachments.end();
	for ( ; ai!=ae; ++ai )
		delete *ai;
}
ControllerEmu::ControlGroup::Control::~Control()
{
	delete control_ref;
}

void ControllerEmu::UpdateReferences( ControllerInterface& devi )
{
	std::vector<ControlGroup*>::const_iterator
		i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
	{
		std::vector<ControlGroup::Control*>::const_iterator
			ci = (*i)->controls.begin(),
			ce = (*i)->controls.end();
		for ( ; ci!=ce; ++ci )
			devi.UpdateReference( (*ci)->control_ref );

		// extension
		if ( GROUP_TYPE_EXTENSION == (*i)->type )
		{
			std::vector<ControllerEmu*>::const_iterator
				ai = ((Extension*)*i)->attachments.begin(),
				ae = ((Extension*)*i)->attachments.end();
			for ( ; ai!=ae; ++ai )
				(*ai)->UpdateReferences( devi );
		}
	}
}

void ControllerEmu::UpdateDefaultDevice()
{
	std::vector<ControlGroup*>::const_iterator
		i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
	{
		std::vector<ControlGroup::Control*>::const_iterator
			ci = (*i)->controls.begin(),
			ce = (*i)->controls.end();
		for ( ; ci!=ce; ++ci )
			(*ci)->control_ref->device_qualifier = default_device;

		// extension
		if ( GROUP_TYPE_EXTENSION == (*i)->type )
		{
			std::vector<ControllerEmu*>::const_iterator
				ai = ((Extension*)*i)->attachments.begin(),
				ae = ((Extension*)*i)->attachments.end();
			for ( ; ai!=ae; ++ai )
			{
				(*ai)->default_device = default_device;
				(*ai)->UpdateDefaultDevice();
			}
		}
	}
}

void ControllerEmu::ControlGroup::LoadConfig( IniFile::Section& sec, const std::string& defdev, const std::string& base )
{
	std::string group( base + name ); group += "/";

	// settings
	std::vector<ControlGroup::Setting*>::const_iterator
		si = settings.begin(),
		se = settings.end();
	for ( ; si!=se; ++si )
		(*si)->value = sec.Get(group+(*si)->name, (*si)->default_value*100) / 100;

	// controls
	std::vector<ControlGroup::Control*>::const_iterator
		ci = controls.begin(),
		ce = controls.end();
	for ( ; ci!=ce; ++ci )
	{
		// control and dev qualifier
		(*ci)->control_ref->control_qualifier.name = sec[group + (*ci)->name];
		(*ci)->control_ref->device_qualifier.FromString( sec.Get( group+(*ci)->name+"/Device", defdev ) );

		// range
		(*ci)->control_ref->range = sec.Get( group+(*ci)->name+"/Range", 100.0f ) / 100;

		// input mode
		if ( (*ci)->control_ref->is_input )
			((ControllerInterface::InputReference*)((*ci)->control_ref))->mode
				= sec.Get( group+(*ci)->name+"/Mode", 0 );
	}

	// extensions
	if ( GROUP_TYPE_EXTENSION == type )
	{
		Extension* const ex = ((Extension*)this);

		ex->switch_extension = 0;
		unsigned int n = 0;
		const std::string extname = sec[ base + name ];

		std::vector<ControllerEmu*>::const_iterator
			ai = ((Extension*)this)->attachments.begin(),
			ae = ((Extension*)this)->attachments.end();
		for ( ; ai!=ae; ++ai,++n )
		{
			(*ai)->default_device.FromString( defdev );
			(*ai)->LoadConfig( sec, base + (*ai)->GetName() + "/" );

			if ( (*ai)->GetName() == extname )
				ex->switch_extension = n;
		}
	}
}

void ControllerEmu::LoadConfig( IniFile::Section& sec, const std::string& base )
{
	std::string defdev = default_device.ToString();
	if ( base.empty() )
	{
		defdev =  sec[ base + "Device" ];
		default_device.FromString( defdev );
	}

	std::vector<ControlGroup*>::const_iterator i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
		(*i)->LoadConfig( sec, defdev, base );
}

void ControllerEmu::ControlGroup::SaveConfig( IniFile::Section& sec, const std::string& defdev, const std::string& base )
{
	std::string group( base + name ); group += "/";

	// settings
	std::vector<ControlGroup::Setting*>::const_iterator
		si = settings.begin(),
		se = settings.end();
	for ( ; si!=se; ++si )
		sec.Set( group+(*si)->name, (*si)->value*100, (*si)->default_value*100 );

	// controls
	std::vector<ControlGroup::Control*>::const_iterator
		ci = controls.begin(),
		ce = controls.end();
	for ( ; ci!=ce; ++ci )
	{
		// control and dev qualifier
		sec.Set( group+(*ci)->name, (*ci)->control_ref->control_qualifier.name );
		sec.Set( group+(*ci)->name+"/Device", (*ci)->control_ref->device_qualifier.ToString(), defdev );

		// range
		sec.Set( group+(*ci)->name+"/Range", (*ci)->control_ref->range*100, 100 );

		// input mode
		if ( (*ci)->control_ref->is_input )
			sec.Set( group+(*ci)->name+"/Mode",
				((ControllerInterface::InputReference*)((*ci)->control_ref))->mode, (unsigned int)0 );
	}

	// extensions
	if ( GROUP_TYPE_EXTENSION == type )
	{
		Extension* const ext = ((Extension*)this);
		sec.Set( base + name, ext->attachments[ext->switch_extension]->GetName(), std::string("None") );

		std::vector<ControllerEmu*>::const_iterator
			ai = ((Extension*)this)->attachments.begin(),
			ae = ((Extension*)this)->attachments.end();
		for ( ; ai!=ae; ++ai )
			(*ai)->SaveConfig( sec, base + (*ai)->GetName() + "/" );
	}
}

void ControllerEmu::SaveConfig( IniFile::Section& sec, const std::string& base )
{
	const std::string defdev = default_device.ToString();
	if ( base.empty() )
		sec.Set( std::string(" ") + base + "Device", defdev );

	std::vector<ControlGroup*>::const_iterator i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
		(*i)->SaveConfig( sec, defdev, base );
}

ControllerEmu::AnalogStick::AnalogStick( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_STICK )
{
	for ( unsigned int i = 0; i < 4; ++i )
		controls.push_back( new Input( named_directions[i] ) );

	controls.push_back( new Input( "Modifier" ) );

	settings.push_back( new Setting("Dead Zone", 0, 1, 50 ) );
	settings.push_back( new Setting("Square Stick", 0 ) );

}

ControllerEmu::Buttons::Buttons( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_BUTTONS )
{
	settings.push_back( new Setting("Threshold", 0.5f ) );
}

ControllerEmu::MixedTriggers::MixedTriggers( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_MIXED_TRIGGERS )
{
	settings.push_back( new Setting("Threshold", 0.9f ) );
}

ControllerEmu::Triggers::Triggers( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_TRIGGERS )
{
	settings.push_back( new Setting("Dead Zone", 0, 1, 50 ) );
}

ControllerEmu::Force::Force( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_FORCE )
{
	controls.push_back( new Input( "X-" ) );
	controls.push_back( new Input( "X+" ) );
	controls.push_back( new Input( "Y-" ) );
	controls.push_back( new Input( "Y+" ) );
	controls.push_back( new Input( "Z-" ) );
	controls.push_back( new Input( "Z+" ) );
}

void ControllerEmu::Force::GetState( u8* data, const u8 base, const u8 range )
{
	for ( unsigned int i=0; i<3; ++i,++data )
		*data = u8( ( controls[i*2+1]->control_ref->State() - controls[i*2]->control_ref->State() ) * range + base );
}

ControllerEmu::Tilt::Tilt( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_TILT )
{
	//for ( unsigned int i = 0; i < 4; ++i )
		//controls.push_back( new Input( named_directions[i] ) );
	controls.push_back( new Input( "Forward" ) );
	controls.push_back( new Input( "Backward" ) );
	controls.push_back( new Input( "Left" ) );
	controls.push_back( new Input( "Right" ) );

	controls.push_back( new Input( "Modifier" ) );

	settings.push_back( new Setting("Dead Zone", 0, 1, 50 ) );
	settings.push_back( new Setting("Circle Stick", 0 ) );
}

ControllerEmu::Cursor::Cursor( const char* const _name, const SWiimoteInitialize* const _wiimote_initialize )
	: ControlGroup( _name, GROUP_TYPE_CURSOR )
	//, z(0)
	, wiimote_initialize(_wiimote_initialize)
{
	for ( unsigned int i = 0; i < 4; ++i )
		controls.push_back( new Input( named_directions[i] ) );

	settings.push_back( new Setting("Width", 0.5f ) );
	settings.push_back( new Setting("Height", 0.5f ) );
	settings.push_back( new Setting("Top", 0.5f ) );

}

void GetMousePos(float& x, float& y, const SWiimoteInitialize* const wiimote_initialize)
{
	unsigned int win_width = 2, win_height = 2;

#ifdef _WIN32
	// Get the cursor position for the entire screen
	POINT point = { 1, 1 };
	GetCursorPos(&point);
	// Get the cursor position relative to the upper left corner of the rendering window
	ScreenToClient(wiimote_initialize->hWnd, &point);

	// Get the size of the rendering window. (In my case Rect.top and Rect.left was zero.)
	RECT Rect;
	GetClientRect(wiimote_initialize->hWnd, &Rect);
	// Width and height is the size of the rendering window
	win_width = Rect.right - Rect.left;
	win_height = Rect.bottom - Rect.top;

#elif defined(HAVE_X11) && HAVE_X11
	int root_x, root_y;
	struct
	{
		int x, y;
	} point = { 1, 1 };

	Display* const wm_display = (Display*)wiimote_initialize->hWnd;
	Window glwin = *(Window *)wiimote_initialize->pXWindow;

	XWindowAttributes win_attribs;
	XGetWindowAttributes (wm_display, glwin, &win_attribs);
	win_width = win_attribs.width;
	win_height = win_attribs.height;
	Window root_dummy, child_win;
	unsigned int mask;
	XQueryPointer(wm_display, glwin, &root_dummy, &child_win, &root_x, &root_y, &point.x, &point.y, &mask);
#endif

#if ( defined(_WIN32) || (defined(HAVE_X11) && HAVE_X11))
	// Return the mouse position as a range from -1 to 1
	x = (float)point.x / (float)win_width * 2 - 1;
	y = (float)point.y / (float)win_height * 2 - 1;
#else
	x = 0;
	y = 0;
#endif

}
