// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <iostream> // System

#include "wiiuse.h" // Externals

#include "StringUtil.h"
#include "Timer.h"
#include "pluginspecs_wiimote.h"

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "EmuDefinitions.h"
#include "EmuMain.h"
#include "main.h"
#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigBasicDlg.h"
	#include "ConfigRecordingDlg.h"
	#include "ConfigPadDlg.h"
#endif
#include "Config.h"

namespace WiiMoteReal
{
int GetIRDataSize(struct wiimote_t* wm)
{
	if (WIIUSE_USING_EXP(wm))
		return 10;
	else
		return 12;
}

void handle_ctrl_status(struct wiimote_t* wm)
{
	DEBUG_LOG(WIIMOTE, "--- CONTROLLER STATUS [wiimote id %i] ---", wm->unid);

	DEBUG_LOG(WIIMOTE, "attachment:      %i", wm->exp.type);
	DEBUG_LOG(WIIMOTE, "speaker:         %i", WIIUSE_USING_SPEAKER(wm));
	DEBUG_LOG(WIIMOTE, "ir:              %i", WIIUSE_USING_IR(wm));
	DEBUG_LOG(WIIMOTE, "leds:            %i %i %i %i", WIIUSE_IS_LED_SET(wm, 1), WIIUSE_IS_LED_SET(wm, 2), WIIUSE_IS_LED_SET(wm, 3), WIIUSE_IS_LED_SET(wm, 4));
	DEBUG_LOG(WIIMOTE, "battery:         %f %%", wm->battery_level);
}

bool IRDataOK(struct wiimote_t* wm)
{
	// This check is valid because 0 should only be returned if the data
	// hasn't been filled in by wiiuse
	int IRDataSize = GetIRDataSize(wm);
	for (int i = 7; i < IRDataSize; i++)
		if (wm->event_buf[i] == 0)
			return false;

	return true;
}

void handle_event(struct wiimote_t* wm)
{
	//DEBUG_LOG(WIIMOTE, "--- EVENT [id %i] ---", wm->unid);

	// if a button is pressed, report it
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_A))		DEBUG_LOG(WIIMOTE, "A pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_B))		DEBUG_LOG(WIIMOTE, "B pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_UP))		DEBUG_LOG(WIIMOTE, "UP pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_DOWN))	DEBUG_LOG(WIIMOTE, "DOWN pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_LEFT))	DEBUG_LOG(WIIMOTE, "LEFT pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_RIGHT))	DEBUG_LOG(WIIMOTE, "RIGHT pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_MINUS))	DEBUG_LOG(WIIMOTE, "MINUS pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_PLUS))	DEBUG_LOG(WIIMOTE, "PLUS pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_ONE))		DEBUG_LOG(WIIMOTE, "ONE pressed");
	//if (IS_PRESSED(wm, WIIMOTE_BUTTON_ONE))		g_Run = false;
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_TWO))		DEBUG_LOG(WIIMOTE, "TWO pressed");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_HOME))	DEBUG_LOG(WIIMOTE, "HOME pressed");
	// Create shortcut to the nunchuck

	struct nunchuk_t* nc = NULL;

	if (wm->exp.type == EXP_NUNCHUK) {
		
		nc = (nunchuk_t*)&wm->exp.nunchuk;
		if (IS_PRESSED(nc, NUNCHUK_BUTTON_C))
			DEBUG_LOG(WIIMOTE, "C pressed");
		if (IS_PRESSED(nc, NUNCHUK_BUTTON_Z))
			DEBUG_LOG(WIIMOTE, "Z pressed");
	}





	// Pressing minus will tell the wiimote we are no longer interested in movement.
	// This is useful because it saves battery power.
	if (IS_JUST_PRESSED(wm, WIIMOTE_BUTTON_MINUS))
	{
		wiiuse_motion_sensing(wm, 0);
		wiiuse_set_ir(wm, 0);
		g_MotionSensing = false;
	}

	// Turn aceelerometer and IR reporting on, there is some kind of bug that prevents us from turing these on
	// directly after each other, so we have to wait for another wiiuse_poll() this way
	if (IS_JUST_PRESSED(wm, WIIMOTE_BUTTON_PLUS))
	{
		wiiuse_motion_sensing(wm, 1);
		g_MotionSensing = true;
	}
	// Turn IR reporting on
	if (g_MotionSensing && !WIIUSE_USING_IR(wm))
		wiiuse_set_ir(wm, 1);

    if (!m_RecordingConfigFrame) return;

	// Print battery status 
#if defined(HAVE_WX) && HAVE_WX
	if(m_RecordingConfigFrame && g_Config.bUpdateRealWiimote)
		m_RecordingConfigFrame->m_GaugeBattery->SetValue((int)floor((wm->battery_level * 100) + 0.5));
#endif

	// If the accelerometer is turned on then print angles
	if (WIIUSE_USING_ACC(wm) && WIIUSE_USING_IR(wm))
	{
		/*
		std::string Tmp;
		Tmp += StringFromFormat("Roll: %2.1f  ", wm->orient.roll);
		Tmp += StringFromFormat("Pitch: %2.1f  ", wm->orient.pitch);
		Tmp += StringFromFormat("Battery: %1.2f\n", wm->battery_level);
		Tmp += StringFromFormat("G-Force x, y, z: %1.2f %1.2f %1.2f\n", wm->gforce.x, wm->gforce.y, wm->gforce.z);
		Tmp += StringFromFormat("Accel x, y, z: %03i %03i %03i\n", wm->accel.x, wm->accel.y, wm->accel.z); */

		// wm->event_buf is cleared at the end of all wiiuse_poll(), so wm->event_buf will always be zero
		// after that. To get the raw IR data we need to read the wiimote again. This seems to work most of the time,
		// it seems to fails with a regular interval about each tenth read.
		if (wiiuse_io_read(wm))
			if (IRDataOK(wm))
				memcpy(g_EventBuffer, wm->event_buf, GetIRDataSize(wm));
/*
		// Go through each of the 4 possible IR sources
		for (int i = 0; i < 4; ++i)
		{
			// Check if the source is visible
			if (wm->ir.dot[i].visible)
				Tmp += StringFromFormat("IR source %i: (%u, %u)\n", i, wm->ir.dot[i].x, wm->ir.dot[i].y);
		}

		Tmp += "\n";
		Tmp += StringFromFormat("IR cursor: (%u, %u)\n", wm->ir.x, wm->ir.y);
		Tmp += StringFromFormat("IR z distance: %f\n", wm->ir.z);

		if(wm->exp.type == EXP_NUNCHUK)
		{
			Tmp += "\n";
			Tmp += StringFromFormat("Nunchuck accel x, y, z: %03i %03i %03i\n", nc->accel.x, nc->accel.y, nc->accel.z);
		} */

		//Tmp += "\n";
		//std::string TmpData = ArrayToString(g_EventBuffer, ReportSize, 0, 30);
		//Tmp += "Data: " + TmpData;

		//DEBUG_LOG(WIIMOTE, "%s", Tmp.c_str());

#if defined(HAVE_WX) && HAVE_WX
		if(m_RecordingConfigFrame && g_Config.bUpdateRealWiimote)
		{
			// Produce adjusted accelerometer values		
			float _Gx = (float)(wm->accel.x - wm->accel_calib.cal_zero.x) / (float)wm->accel_calib.cal_g.x;
			float _Gy = (float)(wm->accel.y - wm->accel_calib.cal_zero.y) / (float)wm->accel_calib.cal_g.y;
			float _Gz = (float)(wm->accel.z - wm->accel_calib.cal_zero.z) / (float)wm->accel_calib.cal_g.z;
			
			// Conver the data to integers
			int Gx = (int)(_Gx * 100);
			int Gy = (int)(_Gy * 100);
			int Gz = (int)(_Gz * 100);

			
			{ //Updating Wiimote Gauges.
				
				m_RecordingConfigFrame->m_GaugeRoll[0]->SetValue(wm->orient.roll + 180);
				m_RecordingConfigFrame->m_GaugeRoll[1]->SetValue(wm->orient.pitch + 180);

				// Show g. forces between -3 and 3
				m_RecordingConfigFrame->m_GaugeGForce[0]->SetValue((int)floor((wm->gforce.x * 100) + 300.5));
				m_RecordingConfigFrame->m_GaugeGForce[1]->SetValue((int)floor((wm->gforce.y * 100) + 300.5));
				m_RecordingConfigFrame->m_GaugeGForce[2]->SetValue((int)floor((wm->gforce.z * 100) + 300.5));				

				m_RecordingConfigFrame->m_GaugeAccel[0]->SetValue(wm->accel.x);
				m_RecordingConfigFrame->m_GaugeAccel[1]->SetValue(wm->accel.y);
				m_RecordingConfigFrame->m_GaugeAccel[2]->SetValue(wm->accel.z);

				if(wm->exp.type == EXP_NUNCHUK) // Updating Nunchuck Gauges
				{
								
					m_RecordingConfigFrame->m_GaugeGForceNunchuk[0]->SetValue((int)floor((nc->gforce.x * 100) + 300.5));
					m_RecordingConfigFrame->m_GaugeGForceNunchuk[1]->SetValue((int)floor((nc->gforce.y * 100) + 300.5));
					m_RecordingConfigFrame->m_GaugeGForceNunchuk[2]->SetValue((int)floor((nc->gforce.z * 100) + 300.5));	

					m_RecordingConfigFrame->m_GaugeAccelNunchuk[0]->SetValue(nc->accel.x);
					m_RecordingConfigFrame->m_GaugeAccelNunchuk[1]->SetValue(nc->accel.y);
					m_RecordingConfigFrame->m_GaugeAccelNunchuk[2]->SetValue(nc->accel.z);
					
					//Produce valid data for recording
					float _GNCx = (float)(nc->accel.x - nc->accel_calib.cal_zero.x) / (float)nc->accel_calib.cal_g.x;
					float _GNCy = (float)(nc->accel.y - nc->accel_calib.cal_zero.y) / (float)nc->accel_calib.cal_g.y;
					float _GNCz = (float)(nc->accel.z - nc->accel_calib.cal_zero.z) / (float)nc->accel_calib.cal_g.z;
			
					// Conver the data to integers
					int GNCx = (int)(_GNCx * 100);
					int GNCy = (int)(_GNCy * 100);
					int GNCz = (int)(_GNCz * 100);

				}

				m_RecordingConfigFrame->m_TextIR->SetLabel(wxString::Format(
					wxT("Cursor: %03u %03u\nDistance:%4.0f"), wm->ir.x, wm->ir.y, wm->ir.z));

				//m_RecordingConfigFrame->m_TextAccNeutralCurrent->SetLabel(wxString::Format(
				//	wxT("Current: %03u %03u %03u"), Gx, Gy, Gz));

				if(m_RecordingConfigFrame->m_bRecording) {
					if(wm->exp.type == EXP_NUNCHUK) {
						DEBUG_LOG(WIIMOTE, "Wiiuse Recorded accel x, y, z: %03i %03i %03i", Gx, Gy, Gz);
					}
					else {
						DEBUG_LOG(WIIMOTE, "Wiiuse Recorded accel x, y, z: %03i %03i %03i;  NCx, NCy, NCz: %03i %03i %03i",Gx,Gy,Gz, GNCx, GNCy, GNCz);
					}
				}
			}


			// Send the data to be saved //todo: passing nunchuck x,y,z vars as well
			m_RecordingConfigFrame->DoRecordMovement(Gx, Gy, Gz, g_EventBuffer + 6, GetIRDataSize(wm));

			// Turn recording on and off
			if (IS_PRESSED(wm, WIIMOTE_BUTTON_A)) m_RecordingConfigFrame->DoRecordA(true);
				else m_RecordingConfigFrame->DoRecordA(false);

			// ------------------------------------
			// Show roll and pitch in the status box
			// --------------
			if(!g_DebugData)
			{
				DEBUG_LOG(WIIMOTE, "Roll:%03i Pitch:%03i", (int)wm->orient.roll, (int)wm->orient.pitch);
			}
			if(m_PadConfigFrame)
			{
				// Convert Roll and Pitch from 180 to 0x8000
				int Roll = (int)wm->orient.roll * (0x8000 / 180);
				int Pitch = (int)wm->orient.pitch * (0x8000 / 180);
				// Convert it to the box
				m_PadConfigFrame->Convert2Box(Roll);
				m_PadConfigFrame->Convert2Box(Pitch);
				// Show roll and pitch in the axis boxes
				m_PadConfigFrame->m_bmpDotRightOut[0]->SetPosition(wxPoint(Roll, Pitch));
			}
		}
#endif
	}
	// Otherwise remove the values
	else
	{
#if defined(HAVE_WX) && HAVE_WX
		if (m_RecordingConfigFrame)
		{
			NOTICE_LOG(BOOT, "readwiimote, reset bars to zero");
			m_RecordingConfigFrame->m_GaugeRoll[0]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeRoll[1]->SetValue(0);

			m_RecordingConfigFrame->m_GaugeGForce[0]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeGForce[1]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeGForce[2]->SetValue(0);

			m_RecordingConfigFrame->m_GaugeAccel[0]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeAccel[1]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeAccel[2]->SetValue(0);

			m_RecordingConfigFrame->m_GaugeAccelNunchuk[0]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeAccelNunchuk[1]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeAccelNunchuk[2]->SetValue(0);

			m_RecordingConfigFrame->m_GaugeGForceNunchuk[0]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeGForceNunchuk[1]->SetValue(0);
			m_RecordingConfigFrame->m_GaugeGForceNunchuk[2]->SetValue(0);

			m_RecordingConfigFrame->m_TextIR->SetLabel(wxT("Cursor:\nDistance:"));
		}
#endif
	}
}

void ReadWiimote()
{
	/* I place this outside wiiuse_poll() to produce a continous recording regardless of the status
	   change of the Wiimote, wiiuse_poll() is only true if the status has changed. However, this the
	   timing functions for recording playback that checks the time of the recording this should not
	   be needed. But I still use it becase it seemed like state_changed() or the threshold values or
	   something else might fail so that only huge status changed were reported. */
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		handle_event(g_WiiMotesFromWiiUse[i]);
	}
	// Declaration
	std::string Temp;

	/* Timeout for data reading. This is used in Initialize() to read the Eeprom, if we have not gotten
	   what we wanted in the WIIUSE_READ_DATA case we stop this loop and enable the regular
	   wiiuse_io_read() and wiiuse_io_write() loop again. */
	if (g_RunTemporary)
	{
		// The SecondsToWait holds if the update rate of wiiuse_poll() is kept at the default value of 10 ms
		static const int SecondsToWait = 2;
		g_RunTemporaryCountdown++;
		if(g_RunTemporaryCountdown > (SecondsToWait * 100))
		{
			g_RunTemporaryCountdown = 0;
			g_RunTemporary = false;
		}
	}

	// Read formatted Wiimote data
	if (wiiuse_poll(g_WiiMotesFromWiiUse, MAX_WIIMOTES))
	{
		/*
		 *	This happens if something happened on any wiimote.
		 *	So go through each one and check if anything happened.
		 */
		int i = 0;
		for (; i < MAX_WIIMOTES; ++i)
		{
			switch (g_WiiMotesFromWiiUse[i]->event)
			{
				case WIIUSE_EVENT:
					/* a generic event occured */
					//handle_event(g_WiiMotesFromWiiUse[i]);
					break;

				case WIIUSE_STATUS:
					/* a status event occured */
					//handle_ctrl_status(g_WiiMotesFromWiiUse[i]);
					break;

				case WIIUSE_DISCONNECT:
				case WIIUSE_UNEXPECTED_DISCONNECT:
					/* the wiimote disconnected */
					//handle_disconnect(wiimotes[i]);
					break;

				case WIIUSE_READ_DATA:
					/*
					 *	Data we requested to read was returned.
					 *	Take a look at wiimotes[i]->read_req
					 *	for the data.
					 */
					if(g_WiiMotesFromWiiUse[i]->read_req->size == sizeof(WiiMoteEmu::EepromData_0)
						&& g_WiiMotesFromWiiUse[i]->read_req->addr == 0)
					{				
						Temp = ArrayToString(g_WiiMotesFromWiiUse[i]->read_req->buf, sizeof(WiiMoteEmu::EepromData_0), 0, 30);
						//memcpy(WiiMoteEmu::g_Eeprom[i], g_WiiMotesFromWiiUse[i]->read_req->buf, sizeof(WiiMoteEmu::EepromData_0));
						DEBUG_LOG(WIIMOTE, "EEPROM: %s", Temp.c_str());
						g_RunTemporary = false;
					}
					break;

				case WIIUSE_NUNCHUK_INSERTED:
					/*
					 *	a nunchuk was inserted
					 *	This is a good place to set any nunchuk specific
					 *	threshold values.  By default they are the same
					 *	as the wiimote.
					 */
					 //wiiuse_set_nunchuk_orient_threshold(g_WiiMotesFromWiiUse[i], 90.0f);
					 //wiiuse_set_nunchuk_accel_threshold(g_WiiMotesFromWiiUse[i], 100);
					break;

				case WIIUSE_CLASSIC_CTRL_INSERTED:
					DEBUG_LOG(WIIMOTE, "Classic controller inserted.");
					break;

				case WIIUSE_GUITAR_HERO_3_CTRL_INSERTED:
					// some expansion was inserted
					//handle_ctrl_status(wiimotes[i]);
					DEBUG_LOG(WIIMOTE, "Guitar Hero 3 controller inserted.");
					break;

				case WIIUSE_NUNCHUK_REMOVED:
					DEBUG_LOG(WIIMOTE, "Nunchuck was removed.");
					break;

				case WIIUSE_CLASSIC_CTRL_REMOVED:
				case WIIUSE_GUITAR_HERO_3_CTRL_REMOVED:
					// some expansion was removed
					//handle_ctrl_status(wiimotes[i]);
					DEBUG_LOG(WIIMOTE, "An expansion was removed.");
					break;

				default:
					break;
			}
		}
	}
}


}; // end of namespace
