
#include "ConfigDiag.h"

void ConfigDialog::UpdateBitmaps(wxTimerEvent& WXUNUSED(event))
{

	GamepadPage* const current_page = (GamepadPage*)m_pad_notebook->GetPage( m_pad_notebook->GetSelection() );

	std::vector< ControlGroupBox* >::iterator g = current_page->control_groups.begin(),
	ge = current_page->control_groups.end();
	for ( ; g != ge; ++g  )
	{

		if ( (*g)->static_bitmap )
		{

			// don't want game thread updating input when we are using it here
			if ( false == m_plugin.interface_crit.TryEnter() )
				return;

			if ( false == is_game_running )
				m_plugin.controller_interface.UpdateInput();

			switch ( (*g)->control_group->type )
			{
			case GROUP_TYPE_STICK :
				{
					float x, y;
					float xx, yy;
					((ControllerEmu::AnalogStick*)(*g)->control_group)->GetState( &x, &y, 32.0, 32-1.5 );

					xx = ((ControllerEmu::AnalogStick*)(*g)->control_group)->controls[3]->control_ref->State();
					xx -= ((ControllerEmu::AnalogStick*)(*g)->control_group)->controls[2]->control_ref->State();
					yy = ((ControllerEmu::AnalogStick*)(*g)->control_group)->controls[1]->control_ref->State();
					yy -= ((ControllerEmu::AnalogStick*)(*g)->control_group)->controls[0]->control_ref->State();
					xx *= 32 - 1; xx += 32;
					yy *= 32 - 1; yy += 32;

					// setup
					wxBitmap bitmap(64, 64);
					wxMemoryDC dc;
					dc.SelectObject(bitmap);
					dc.Clear();

					// draw the shit

					// circle for visual aid for diagonal adjustment
					dc.SetPen(*wxLIGHT_GREY_PEN);
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawCircle( 32, 32, 32);

					// deadzone circle
					dc.SetBrush(*wxLIGHT_GREY_BRUSH);
					dc.DrawCircle( 32, 32, ((ControllerEmu::AnalogStick*)(*g)->control_group)->settings[0]->value * 32 );

					// raw dot
					dc.SetPen(*wxGREY_PEN);
					dc.SetBrush(*wxGREY_BRUSH);
					// i like the dot better than the cross i think
					dc.DrawRectangle( xx - 2, yy - 2, 4, 4 );
					//dc.DrawRectangle( xx-1, 64-yy-4, 2, 8 );
					//dc.DrawRectangle( xx-4, 64-yy-1, 8, 2 );

					// adjusted dot
					if ( x!=32 || y!=32 )
					{
						dc.SetPen(*wxRED_PEN);
						dc.SetBrush(*wxRED_BRUSH);
						dc.DrawRectangle( x-2, 64-y-2, 4, 4 );
						// i like the dot better than the cross i think
						//dc.DrawRectangle( x-1, 64-y-4, 2, 8 );
						//dc.DrawRectangle( x-4, 64-y-1, 8, 2 );
					}

					// box outline
					// Windows XP color
					dc.SetPen(wxPen(_T("#7f9db9")));
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawRectangle(0, 0, 64, 64);

					// done drawing
					dc.SelectObject(wxNullBitmap);

					// set the shit
					(*g)->static_bitmap->SetBitmap( bitmap );
				}
				break;
			case GROUP_TYPE_BUTTONS :
				{
					const unsigned int button_count = ((unsigned int)(*g)->control_group->controls.size());
					// setup
					wxBitmap bitmap(12*button_count+1, 12);
					wxMemoryDC dc;
					dc.SelectObject(bitmap);
					dc.Clear();

					// draw the shit
					dc.SetPen(*wxGREY_PEN);

					unsigned int * const bitmasks = new unsigned int[ button_count ];
					for ( unsigned int n = 0; n<button_count; ++n )
						bitmasks[n] = ( 1 << n );

					unsigned int buttons = 0;
					((ControllerEmu::Buttons*)(*g)->control_group)->GetState( &buttons, bitmasks );

					for ( unsigned int n = 0; n<button_count; ++n )
					{
						// TODO: threshold stuff, actually redo this crap with the GetState func
						if ( buttons & bitmasks[n] )
							dc.SetBrush( *wxRED_BRUSH );
						else
						{
							unsigned char amt = 255 - (*g)->control_group->controls[n]->control_ref->State() * 128;
							dc.SetBrush( wxBrush( wxColor( amt, amt, amt ) ) );
						}
						dc.DrawRectangle(n*12, 0, 14, 12);
					}

					delete bitmasks;

					// box outline
					// Windows XP color
					dc.SetPen(wxPen(_T("#7f9db9")));
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawRectangle(0, 0, bitmap.GetWidth(), bitmap.GetHeight());

					// done drawing
					dc.SelectObject(wxNullBitmap);

					// set the shit
					(*g)->static_bitmap->SetBitmap( bitmap );					
				}
				break;

			case GROUP_TYPE_MIXED_TRIGGERS :
				{
					const unsigned int trigger_count = ((unsigned int)((*g)->control_group->controls.size() / 2));
					// setup
					wxBitmap bitmap( 64+12+1, 12*trigger_count+1);
					wxMemoryDC dc;
					dc.SelectObject(bitmap);
					dc.Clear();

					// draw the shit
					dc.SetPen(*wxGREY_PEN);
					ControlState thresh =  (*g)->control_group->settings[0]->value;

					for ( unsigned int n = 0; n < trigger_count; ++n )
					{
						dc.SetBrush(*wxRED_BRUSH);
						ControlState trig_d = (*g)->control_group->controls[n]->control_ref->State();

						ControlState trig_a = trig_d > thresh ? 1
							: (*g)->control_group->controls[n+trigger_count]->control_ref->State();

						dc.DrawRectangle(0, n*12, 64+20, 14);
						if ( trig_d <= thresh )
							dc.SetBrush(*wxWHITE_BRUSH);
						dc.DrawRectangle(trig_a*64, n*12, 64+20, 14);
						dc.DrawRectangle(64, n*12, 32, 14);
					}
					dc.SetBrush(*wxTRANSPARENT_BRUSH);
					dc.DrawRectangle(thresh*64, 0, 128, trigger_count*14);

					// box outline
					// Windows XP color
					dc.SetPen(wxPen(_T("#7f9db9")));
					dc.DrawRectangle(0, 0, bitmap.GetWidth(), bitmap.GetHeight());

					// done drawing
					dc.SelectObject(wxNullBitmap);

					// set the shit
					(*g)->static_bitmap->SetBitmap( bitmap );	
				}
				break;
			default :
				break;
			}

			m_plugin.interface_crit.Leave();
		}
	}

}
