// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "DolphinWX/WXInputBase.h"

#include "Core/Core.h"

#include "DolphinWX/ConfigVR.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/HotkeysXInput.h"

#include <wx/msgdlg.h>

static wxString async_desc = wxTRANSLATE("Render head rotation updates in a separate thread at full frame rate using Timewarp even when the game runs at a lower frame rate.");
static wxString temp_desc = wxTRANSLATE("Game specific VR option, in metres or degrees");
static wxString minfov_desc = wxTRANSLATE("Minimum horizontal degrees of your view that the action will fill.\nWhen the game tries to render from a distance with a zoom lens, this will move thhe camera closer instead. When the FOV is less than the minimum the camera will move forward until objects at Aim Distance fill the minimum FOV.\nIf unsure, leave this at 10 degrees.");
static wxString scale_desc = wxTRANSLATE("(Don't change this until the game's Units Per Metre setting is already lifesize!)\n\nScale multiplier for all VR worlds.\n1x = lifesize, 2x = Giant size\n0.5x = Child size, 0.17x = Barbie doll size, 0.02x = Lego size\n\nIf unsure, use 1.00.");
static wxString lean_desc = wxTRANSLATE("How many degrees leaning back should count as vertical.\n0 = sitting/standing, 45 = reclining\n90 = playing lying on your back, -90 = on your front\n\nIf unsure, use 0.");
static wxString extraframes_desc = wxTRANSLATE("How many extra frames to render via timewarp.  Set to 0 for 60fps games, 1 for 30fps games, 2 for 20fps games and the framelimiter to your Rift's refresh rate.  For 25fps PAL games, set to 2 and set the frame limiter to 60 (assuming the Rift's refresh rate is set to 75hz).  If unsure, use 0.");
static wxString replaybuffer_desc = wxTRANSLATE("How many extra frames to render through replaying the video loop.  Set to 0 for 60fps games, 1 for 30fps games, 2 for 20fps games and the framelimiter to your Rift's refresh rate.  For 25fps PAL games, set to 2 and set the frame limiter to 60 (assuming the Rift's refresh rate is set to 75hz).  If unsure, use 0.");
static wxString stabilizeroll_desc = wxTRANSLATE("Counteract the game's camera roll. Requires 'Read Camera Angles' to be checked in the 'VR Game' tab.\nIf unsure, leave this checked.");
static wxString stabilizepitch_desc = wxTRANSLATE("Counteract the game's camera pitch. Fixes tilt in 3rd person games and allows free y-axis aiming in 1st person games.  Requires 'Read Camera Angles' to be checked in the 'VR Game' tab.\nIf unsure, leave this checked.");
static wxString stabilizeyaw_desc = wxTRANSLATE("Counteract the game's camera yaw. Use this when playing standing up or in a swivel chair. Also allows completely free x-axis aiming in 1st person games.  Requires 'Read Camera Angles' to be checked in the 'VR Game' tab.\nIf unsure, leave this unchecked.");
static wxString stabilizex_desc = wxTRANSLATE("Counteract the game's 'sideways' camera movement. Requires 'Read Camera Angles' to be checked in the 'VR Game' tab. This might reduce motion sickness, but will stop the HUD from lining up correctly. Use freelook to move the camera yourself. This option is for playing standing up in a large room, or with the scale set to toy sized.\nIf unsure, leave this unchecked.");
static wxString stabilizey_desc = wxTRANSLATE("Counteract the game's 'vertical' camera movement. Requires 'Read Camera Angles' to be checked in the 'VR Game' tab. This might reduce motion sickness, but will stop the HUD from lining up correctly. Use freelook to move the camera yourself. This option sometimes works when seated, but is mostly for playing standing up in a large room, or with the scale set to toy sized.\nIf unsure, leave this unchecked.");
static wxString stabilizez_desc = wxTRANSLATE("Counteract the game's 'forwards' camera movement. Requires 'Read Camera Angles' to be checked in the 'VR Game' tab. This might reduce motion sickness, but will stop the HUD from lining up correctly. Use freelook to move the camera yourself. This option is for playing standing up in a large room, or with the scale set to toy sized.\nIf unsure, leave this unchecked.");
static wxString keyhole_desc = wxTRANSLATE("Allows keyhole aiming in 1st person games by manipulating the yaw correction. Great for first person shooters.\nIf unsure, leave this unchecked.");
static wxString keyholewidth_desc = wxTRANSLATE("The width of the horizontal keyhole in degrees.\nIf unsure, use 45.");
static wxString keyholesnap_desc = wxTRANSLATE("Snaps the view a certain amount of degrees to the side if the edge of the keyhole is reached. This lowers motion sickness during 1st person turns, but may be considered less immersive.\nIf unsure, leave this unchcked.");
static wxString keyholesnapsize_desc = wxTRANSLATE("The size of the heyhole snap jump in degrees.\nIf unsure, use 30.");
static wxString pullup20_desc = wxTRANSLATE("Make headtracking work at 75fps for a 20fps game. Enable this on 20fps games to fix judder.\nIf unsure, leave this unchecked.");
static wxString pullup30_desc = wxTRANSLATE("Make headtracking work at 75fps for a 30fps game. Enable this on 30fps games to fix judder.\nIf unsure, leave this unchecked.");
static wxString pullup60_desc = wxTRANSLATE("Make headtracking work at 75fps for a 60fps game. Enable this on 60fps games to fix judder.\nIf unsure, leave this unchecked.");
static wxString opcodewarning_desc = wxTRANSLATE("Turn off Opcode Warnings.  Will ignore errors in the Opcode Replay Buffer, allowing many games with minor problems to be played.  This should be turned on if trying to play a game with Opcode Replay enabled. Once all the bugs in the Opcode Buffer are fixed, this option will be removed!");
static wxString replayvertex_desc = wxTRANSLATE("Records and replays the vertex buffer and vertex shader constants. This is only needed for certain games that overwrite the vertex buffer midframe, causing corruption with Opcode Replay enabled, e.g. Metroid Prime and DK Country.\nIf unsure, leave this unchecked.");
static wxString replayother_desc = wxTRANSLATE("Records and replays many other parts of the frame such as the Index Buffer, Geometery Shader Constants, and Pixel Shader constants.  This has not been found to help with any games (yet), and should be left disabled for most games. Enabling this will use more memory most likely for no reason.\nIf unsure, leave this unchecked.");
static wxString pullup20timewarp_desc = wxTRANSLATE("Timewarp headtracking up to 75fps for a 20fps game. Enable this on 20fps games to timewarp the headtracking to 75fps.\nIf unsure, leave this unchecked.");
static wxString pullup30timewarp_desc = wxTRANSLATE("Timewarp headtracking up to 75fps for a 30fps game. Enable this on 30fps games to timewarp the headtracking to 75fps.\nIf unsure, leave this unchecked.");
static wxString pullup60timewarp_desc = wxTRANSLATE("Timewarp headtracking up to 75fps for a 60fps game. Enable this on 60fps games to timewarp the headtracking to 75fps.\nIf unsure, leave this unchecked.");
static wxString timewarptweak_desc = wxTRANSLATE("How long before the expected Vsync the timewarped frame should be injected. Ideally this value should be around 0.0040, but some configurations may benefit from tweaking this value.  Only used if 'Extra Timewarped Frames' is non-zero. If unsure, set this to 0.0040.");
static wxString enablevr_desc = wxTRANSLATE("Enable Virtual Reality (if your HMD was detected when you started Dolphin).\n\nIf unsure, leave this checked.");
static wxString player_desc = wxTRANSLATE("During split-screen games, which player is wearing the Oculus Rift?\nPlayer 1 is top left, player 2 is top right, player 3 is bottom left, player 4 is bottom right.\nThe player in the Rift will only see their player's view.\n\nIf unsure, say Player 1.");
static wxString cameracontrol_desc = wxTRANSLATE("Let the game rotate the camera only in these axes, to prevent motion sickness.\nDoesn't work in all games.\n\nIf unsure, choose Yaw only.");
static wxString readpitch_desc = wxTRANSLATE("How many extra degrees pitch to add to the camera when reading camera angles.\nThis only has an effect when 'Read Camera Angles' is checked and 'Let the Game Camera Control' is not set to 'yaw, pitch, and roll'. It is for games where the read camera angle is looking at the floor.\nThis will normally be 0 or 90.\nIf unsure, leave this at 0 degrees.");
static wxString lowpersistence_desc = wxTRANSLATE("Use low persistence on DK2 to reduce motion blur when turning your head.\n\nIf unsure, leave this checked.");
static wxString dynamicpred_desc = wxTRANSLATE("\"Adjust prediction dynamically based on internally measured latency.\"\n\nIf unsure, leave this checked.");
static wxString nomirrortowindow_desc = wxTRANSLATE("Disable the mirrored window in direct mode.  Current tests show better performance with this unchecked and the mirrored window resized to 0 pixels by 0 pixels. Leave this unchecked.");
static wxString orientation_desc = wxTRANSLATE("Use orientation tracking.\n\nLeave this checked.");
static wxString magyaw_desc = wxTRANSLATE("Use the Rift's magnetometers to prevent yaw drift when out of camera range.\n\nIf unsure, leave this checked.");
static wxString position_desc = wxTRANSLATE("Use position tracking (both from camera and head/neck model).\n\nLeave this checked.");
static wxString chromatic_desc = wxTRANSLATE("Use chromatic aberration correction to prevent red and blue fringes at the edges of objects.\n\nIf unsure, leave this checked.");
static wxString timewarp_desc = wxTRANSLATE("Shift the warped display after rendering to correct for head movement during rendering.\n\nIf unsure, leave this checked.");
static wxString vignette_desc = wxTRANSLATE("Fade out the edges of the screen to make the screen sides look less harsh.\nNot needed on DK1.\n\nIf unsure, leave this unchecked.");
static wxString norestore_desc = wxTRANSLATE("Tell the Oculus SDK not to restore OpenGL state.\n\nIf unsure, leave this unchecked.");
static wxString flipvertical_desc = wxTRANSLATE("Flip the screen vertically.\n\nIf unsure, leave this unchecked.");
static wxString srgb_desc = wxTRANSLATE("\"Assume input images are in sRGB gamma-corrected color space.\"\n\nIf unsure, leave this unchecked.");
static wxString overdrive_desc = wxTRANSLATE("Try to fix true black smearing by overdriving brightness transitions.\n\nIf unsure, leave this unchecked.");
static wxString hqdistortion_desc = wxTRANSLATE("\"High-quality sampling of distortion buffer for anti-aliasing\".\n\nIf unsure, leave this unchecked.");
static wxString hudontop_desc = wxTRANSLATE("Always draw the HUD on top of everything else.\nUse this when you can't see the HUD because the world is covering it up.\n\nIf unsure, leave this unchecked.");
static wxString dontclearscreen_desc = wxTRANSLATE("Don't clear the screen every frame. This prevents some games from flashing.\n\nIf unsure, leave this unchecked.");
static wxString canreadcamera_desc = wxTRANSLATE("Read the camera angle from the angle of the first drawn 3D object.\nThis will only work in some games, but helps keep the ground horizontal.\n\nIf unsure, leave this unchecked.");
static wxString detectskybox_desc = wxTRANSLATE("Assume any object drawn at 0,0,0 in camera space is a skybox.\nUse this if the sky looks too close, or to enable hiding or locking the skybox for motion sickness prevention (see the Motion Sickness tab).\n\nIf unsure, either setting is usually fine.");
static wxString nearclip_desc = wxTRANSLATE("Always draw things which are close to (or far from) the camera.\nThis fixes the problem of things disappearing when you move your head close.\nThere can still be problems when two objects are in front of the near clipping plane, if they are drawn in the wrong order.\n\nIf unsure, leave this checked.");
static wxString showcontroller_desc = wxTRANSLATE("Show the razer hydra, wiimote, or gamecube controller inside the game world. Note: Only works in Direct3D for now. \n\nIf unsure, leave this unchecked.");
static wxString showhands_desc = wxTRANSLATE("Show your hands inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString showfeet_desc = wxTRANSLATE("Show your feet inside the game world.\nBased on your height in Oculus Configuration Utility.\n\nIf unsure, leave this unchecked.");
static wxString showgamecamera_desc = wxTRANSLATE("Show the location of the game's camera inside the game world.\nNormally this will be where your head is unless you move your head.\n\nIf unsure, leave this unchecked.");
static wxString showgamecamerafrustum_desc = wxTRANSLATE("Show the pyramid coming out of the game camera representing what the game thinks you can see.\n\nIf unsure, leave this unchecked.");
static wxString showsensorbar_desc = wxTRANSLATE("Show the virtual or real sensor bar inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString showtrackingcamera_desc = wxTRANSLATE("Show the Oculus Rift tracking camera inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString showtrackingvolume_desc = wxTRANSLATE("Show the pyramid coming out of the Rift's tracking camera representing where your head can be tracked.\n\nIf unsure, leave this unchecked.");
static wxString showbasestation_desc = wxTRANSLATE("Show the Razer Hydra base station inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString hideskybox_desc = wxTRANSLATE("Don't draw the background or sky in some games.\nThat may reduce motion sickness by reducing vection in your periphery, but most games won't look as good.\n\nIf unsure, chose 'normal'.");
static wxString lockskybox_desc = wxTRANSLATE("Lock the game's background or sky to the real world in some games.\nTurning in-game will rotate the level but not the background. Oculus suggested this at Connect. It will reduce motion sickness at the cost of decreased immersion.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessprevention_desc = wxTRANSLATE("A list of options that can help reduce motion sickness:\nReduce FOV - Lower the FOV temporarily during certain actions selected below.\nBlack Screen - Blank the screen entirely during certain actions selected below.\n\nIf unsure, select 'none'.");
static wxString motionsicknessfov_desc = wxTRANSLATE("Set the restricted FOV to be used when the motion sickness reduction option is activated. A range somewhere between 80 to 40 degrees is ideal.");
static wxString motionsicknessalways_desc = wxTRANSLATE("Always enable the selected motion sickness reduction option.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessfreelook_desc = wxTRANSLATE("Enable the selected motion sickness reduction option during fast freelook movements.\n\nIf unsure, leave this unchecked.");
static wxString motionsickness2d_desc = wxTRANSLATE("Enable the selected motion sickness reduction option in 2D scenes, when there is no 3D world detected.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessleftstick_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the left joystick.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessrightstick_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the right joystick.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessdpad_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the D-Pad.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessir_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the IR pointer.\n\nIf unsure, leave this unchecked.");

BEGIN_EVENT_TABLE(CConfigVR, wxDialog)

EVT_CLOSE(CConfigVR::OnClose)
EVT_BUTTON(wxID_OK, CConfigVR::OnOk)

END_EVENT_TABLE()

CConfigVR::CConfigVR(wxWindow* parent, wxWindowID id, const wxString& title, 
	const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	, vconfig(g_Config)

{
	vconfig.LoadVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

	Bind(wxEVT_UPDATE_UI, &CConfigVR::OnUpdateUI, this);

	CreateGUIControls();
}

CConfigVR::~CConfigVR()
{
}

namespace
{
	const float INPUT_DETECT_THRESHOLD = 0.55f;
}

// Used to restrict changing of some options while emulator is running
void CConfigVR::UpdateGUI()
{
	//if (Core::IsRunning())
	//{
		// Disable the Core stuff on GeneralPage
		//CPUThread->Disable();
		//SkipIdle->Disable();
		//EnableCheats->Disable();

		//CPUEngine->Disable();
		//_NTSCJ->Disable();
	//}
}

#define VR_NUM_COLUMNS 2

void CConfigVR::CreateGUIControls()
{
	// Configuration controls sizes
	wxSize size(150, 20);
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxNotebook *Notebook = new wxNotebook(this, wxID_ANY);

	// -- VR --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, wxTRANSLATE("VR"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);

		// - vr
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 10);
		wxFlexGridSizer* const szr_options = new wxFlexGridSizer(4, 5, 10);

		// Scale
		{
			SettingNumber* const spin_scale = CreateNumber(page_vr, vconfig.fScale,
				wxGetTranslation(scale_desc), 0.001f, 100.0f, 0.01f);
			wxStaticText* label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Scale Multiplier:   x"));

			spin_scale->SetToolTip(wxGetTranslation(scale_desc));
			label->SetToolTip(wxGetTranslation(scale_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_scale);
		}
		// Lean back angle
		{
			SettingNumber* const spin_lean = CreateNumber(page_vr, vconfig.fLeanBackAngle,
				wxGetTranslation(lean_desc), -180.0f, 180.0f, 1.0f);
			wxStaticText* label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Lean Back Angle:"));

			spin_lean->SetToolTip(wxGetTranslation(lean_desc));
			label->SetToolTip(wxGetTranslation(lean_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_lean);
		}
		// Game Camera Control
		{
			checkbox_roll = CreateCheckBox(page_vr, wxTRANSLATE("Roll"), wxGetTranslation(stabilizeroll_desc), vconfig.bStabilizeRoll);
			checkbox_pitch = CreateCheckBox(page_vr, wxTRANSLATE("Pitch"), wxGetTranslation(stabilizepitch_desc), vconfig.bStabilizePitch);
			checkbox_yaw = CreateCheckBox(page_vr, wxTRANSLATE("Yaw"), wxGetTranslation(stabilizeyaw_desc), vconfig.bStabilizeYaw);
			checkbox_x = CreateCheckBox(page_vr, wxTRANSLATE("X"), wxGetTranslation(stabilizex_desc), vconfig.bStabilizeX);
			checkbox_y = CreateCheckBox(page_vr, wxTRANSLATE("Y"), wxGetTranslation(stabilizey_desc), vconfig.bStabilizeY);
			checkbox_z = CreateCheckBox(page_vr, wxTRANSLATE("Z"), wxGetTranslation(stabilizez_desc), vconfig.bStabilizeZ);

			checkbox_keyhole = CreateCheckBox(page_vr, wxTRANSLATE("Keyhole"), wxGetTranslation(keyhole_desc), vconfig.bKeyhole);
			checkbox_yaw->Bind(wxEVT_CHECKBOX, &CConfigVR::OnYawCheckbox, this);
			checkbox_keyhole->Bind(wxEVT_CHECKBOX, &CConfigVR::OnKeyholeCheckbox, this);

			wxStaticText* label_keyhole_width = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Keyhole Width:"));
			keyhole_width = CreateNumber(page_vr, vconfig.fKeyholeWidth, wxGetTranslation(keyholewidth_desc), 10.0f, 175.0f, 1.0f);
			keyhole_width->Enable(vconfig.bKeyhole);

			wxStaticText* label_snap = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Keyhole Snap:"));
			checkbox_keyhole_snap = CreateCheckBox(page_vr, wxTRANSLATE("Keyhole Snap"), wxGetTranslation(keyholesnap_desc), vconfig.bKeyholeSnap);
			checkbox_keyhole_snap->Bind(wxEVT_CHECKBOX, &CConfigVR::OnKeyholeSnapCheckbox, this);

			wxStaticText* label_snap_size = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Keyhole Snap Size:"));
			keyhole_snap_size = CreateNumber(page_vr, vconfig.fKeyholeSnapSize, wxGetTranslation(keyholesnapsize_desc), 10.0f, 120.0f, 1.0f);
			keyhole_snap_size->Enable(vconfig.bKeyholeSnap);

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Stabilize: ")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_roll, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_pitch, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_yaw, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Stabilize: ")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_x, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_y, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_z, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Keyhole: ")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_keyhole, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(label_keyhole_width, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(keyhole_width);
			szr_vr->Add(label_snap, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_keyhole_snap, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(label_snap_size, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(keyhole_snap_size);
		}
		// VR Player
		{
			const wxString vr_choices[] = { wxTRANSLATE("Player 1"), wxTRANSLATE("Player 2"), wxTRANSLATE("Player 3"), wxTRANSLATE("Player 4") };

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Player wearing HMD:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iVRPlayer, wxGetTranslation(player_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iVRPlayer);
			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			szr_vr->Add(spacer1, 1, 0, 0);
			szr_vr->Add(spacer2, 1, 0, 0);
		}
		{
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Enable VR"), wxGetTranslation(enablevr_desc), vconfig.bEnableVR));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Low Persistence"), wxGetTranslation(lowpersistence_desc), vconfig.bLowPersistence));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Dynamic Prediction"), wxGetTranslation(dynamicpred_desc), vconfig.bDynamicPrediction));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Orientation Tracking"), wxGetTranslation(orientation_desc), vconfig.bOrientationTracking));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Magnetic Yaw"), wxGetTranslation(magyaw_desc), vconfig.bMagYawCorrection));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Position Tracking"), wxGetTranslation(position_desc), vconfig.bPositionTracking));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Chromatic Aberration"), wxGetTranslation(chromatic_desc), vconfig.bChromatic));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Timewarp"), wxGetTranslation(timewarp_desc), vconfig.bTimewarp));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Vignette"), wxGetTranslation(vignette_desc), vconfig.bVignette));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Don't Restore"), wxGetTranslation(norestore_desc), vconfig.bNoRestore));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Flip Vertical"), wxGetTranslation(flipvertical_desc), vconfig.bFlipVertical));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("sRGB"), wxGetTranslation(srgb_desc), vconfig.bSRGB));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Overdrive"), wxGetTranslation(overdrive_desc), vconfig.bOverdrive));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("HQ Distortion"), wxGetTranslation(hqdistortion_desc), vconfig.bHqDistortion));
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Direct Mode - Disable Mirroring"), wxGetTranslation(nomirrortowindow_desc), vconfig.bNoMirrorToWindow));

#ifdef OCULUSSDK042
			szr_vr->Add(async_timewarp_checkbox = CreateCheckBox(page_vr, wxTRANSLATE("Asynchronous timewarp"), wxGetTranslation(async_desc), SConfig::GetInstance().bAsynchronousTimewarp));
#endif
			szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Disable Near-Clipping"), wxGetTranslation(nearclip_desc), vconfig.bDisableNearClipping));
		}

		// Opcode Replay Buffer GUI Options
		wxFlexGridSizer* const szr_opcode = new wxFlexGridSizer(4, 10, 10);
		{
			spin_replay_buffer = new U32Setting(page_vr, wxTRANSLATE("Extra Opcode Replay Frames:"), vconfig.iExtraVideoLoops, 0, 100000);
			RegisterControl(spin_replay_buffer, wxGetTranslation(replaybuffer_desc));
			spin_replay_buffer->SetToolTip(wxGetTranslation(replaybuffer_desc));
			spin_replay_buffer->SetValue(vconfig.iExtraVideoLoops);
			spin_replay_buffer->Bind(wxEVT_SPINCTRL, &CConfigVR::OnOpcodeSpinCtrl, this);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Extra Opcode Replay Frames:"));

			label->SetToolTip(wxGetTranslation(replaybuffer_desc));
			szr_opcode->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(spin_replay_buffer);
			spin_replay_buffer->Enable(!vconfig.bSynchronousTimewarp && !vconfig.bOpcodeReplay || vconfig.iExtraVideoLoops);

			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			szr_opcode->Add(spacer1, 1, 0, 0);
			szr_opcode->Add(spacer2, 1, 0, 0);

			//spin_replay_buffer_divider = new U32Setting(page_vr, wxTRANSLATE("Extra Opcode Replay Frame Divider:"), vconfig.iExtraVideoLoopsDivider, 0, 100000);
			//RegisterControl(spin_replay_buffer_divider, wxGetTranslation(replaybuffer_desc));
			//spin_replay_buffer_divider->SetToolTip(wxGetTranslation(replaybuffer_desc));
			//spin_replay_buffer_divider->SetValue(vconfig.iExtraVideoLoopsDivider);
			//wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Extra Opcode Replay Frame Divider:"));

			//label->SetToolTip(wxGetTranslation(replaybuffer_desc));
			//szr_opcode->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			//szr_opcode->Add(spin_replay_buffer_divider);
			//spin_replay_buffer_divider->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp && !vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);

			szr_opcode->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Quick Opcode Replay Settings:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20 = CreateCheckBox(page_vr, wxTRANSLATE("Pullup 20fps to 75fps"), wxGetTranslation(pullup20_desc), vconfig.bPullUp20fps);
			checkbox_pullup30 = CreateCheckBox(page_vr, wxTRANSLATE("Pullup 30fps to 75fps"), wxGetTranslation(pullup30_desc), vconfig.bPullUp30fps);
			checkbox_pullup60 = CreateCheckBox(page_vr, wxTRANSLATE("Pullup 60fps to 75fps"), wxGetTranslation(pullup60_desc), vconfig.bPullUp60fps);
			SettingCheckBox* checkbox_disable_warnings = CreateCheckBox(page_vr, wxTRANSLATE("Disable Opcode Warnings"), wxGetTranslation(opcodewarning_desc), vconfig.bOpcodeWarningDisable);
			checkbox_replay_vertex_data = CreateCheckBox(page_vr, wxTRANSLATE("Replay Vertex Data"), wxGetTranslation(replayvertex_desc), vconfig.bReplayVertexData);
			checkbox_replay_other_data = CreateCheckBox(page_vr, wxTRANSLATE("Replay Other Data"), wxGetTranslation(replayother_desc), vconfig.bReplayOtherData);
			szr_opcode->Add(checkbox_pullup20, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(checkbox_pullup30, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(checkbox_pullup60, 1, wxALIGN_CENTER_VERTICAL, 0);
			wxStaticText* spacer3 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			szr_opcode->Add(spacer3, 1, 0, 0);
			szr_opcode->Add(checkbox_replay_vertex_data, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(checkbox_replay_other_data, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(checkbox_disable_warnings, 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup30->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup60->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup20->Enable(!vconfig.bSynchronousTimewarp && !vconfig.iExtraVideoLoops);
			checkbox_pullup30->Enable(!vconfig.bSynchronousTimewarp && !vconfig.iExtraVideoLoops);
			checkbox_pullup60->Enable(!vconfig.bSynchronousTimewarp && !vconfig.iExtraVideoLoops);
			checkbox_replay_vertex_data->Enable(vconfig.bOpcodeReplay);
			checkbox_replay_other_data->Enable(vconfig.bOpcodeReplay);
		}

		// Synchronous Timewarp GUI Options
		wxFlexGridSizer* const szr_timewarp = new wxFlexGridSizer(4, 10, 10);
		{
			spin_timewarped_frames = new U32Setting(page_vr, wxTRANSLATE("Extra Timewarped Frames:"), vconfig.iExtraTimewarpedFrames, 0, 4);
			RegisterControl(spin_timewarped_frames, wxGetTranslation(extraframes_desc));
			spin_timewarped_frames->SetToolTip(wxGetTranslation(extraframes_desc));
			spin_timewarped_frames->SetValue(vconfig.iExtraTimewarpedFrames);
			spin_timewarped_frames->Bind(wxEVT_SPINCTRL, &CConfigVR::OnTimewarpSpinCtrl, this);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Extra Timewarped Frames:"));

			label->SetToolTip(wxGetTranslation(wxGetTranslation(extraframes_desc)));
			szr_timewarp->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(spin_timewarped_frames);
			spin_timewarped_frames->Enable(!vconfig.bSynchronousTimewarp && !vconfig.bOpcodeReplay || vconfig.iExtraTimewarpedFrames);
			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			szr_timewarp->Add(spacer1, 1, 0, 0);
			szr_timewarp->Add(spacer2, 1, 0, 0);
		}
		{
			spin_timewarp_tweak = CreateNumber(page_vr, vconfig.fTimeWarpTweak, wxTRANSLATE("Timewarp VSync Tweak:"), -1.0f, 1.0f, 0.0001f);
			RegisterControl(spin_timewarp_tweak, wxGetTranslation(timewarptweak_desc));
			spin_timewarp_tweak->SetToolTip(wxGetTranslation(timewarptweak_desc));
			spin_timewarp_tweak->SetValue(vconfig.fTimeWarpTweak);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Timewarp VSync Tweak:"));
			spin_timewarp_tweak->Enable(!vconfig.bOpcodeReplay);

			label->SetToolTip(wxGetTranslation(timewarptweak_desc));
			szr_timewarp->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(spin_timewarp_tweak);
			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE(""));
			szr_timewarp->Add(spacer1, 1, 0, 0);
			szr_timewarp->Add(spacer2, 1, 0, 0);

			szr_timewarp->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Timewarp Pull-Up:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20_timewarp = CreateCheckBox(page_vr, wxTRANSLATE("Timewarp 20fps to 75fps"), wxGetTranslation(pullup20timewarp_desc), vconfig.bPullUp20fpsTimewarp);
			checkbox_pullup30_timewarp = CreateCheckBox(page_vr, wxTRANSLATE("Timewarp 30fps to 75fps"), wxGetTranslation(pullup30timewarp_desc), vconfig.bPullUp30fpsTimewarp);
			checkbox_pullup60_timewarp = CreateCheckBox(page_vr, wxTRANSLATE("Timewarp 60fps to 75fps"), wxGetTranslation(pullup60timewarp_desc), vconfig.bPullUp60fpsTimewarp);
			szr_timewarp->Add(checkbox_pullup20_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(checkbox_pullup30_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(checkbox_pullup60_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup30_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup60_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup20_timewarp->Enable(!vconfig.bOpcodeReplay);
			checkbox_pullup30_timewarp->Enable(!vconfig.bOpcodeReplay);
			checkbox_pullup60_timewarp->Enable(!vconfig.bOpcodeReplay);
		}

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, wxTRANSLATE("All Games"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);
		wxStaticBoxSizer* const group_opcode = new wxStaticBoxSizer(wxVERTICAL, page_vr, wxTRANSLATE("Opcode Replay (BETA) - Note: Enable 'Idle Skipping' in Config Tab"));
		group_opcode->Add(szr_opcode, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_opcode, 0, wxEXPAND | wxALL, 5);
		wxStaticBoxSizer* const group_timewarp = new wxStaticBoxSizer(wxVERTICAL, page_vr, wxTRANSLATE("Synchronous Timewarp - Note: Enable 'Idle Skipping' in Config Tab"));
		group_timewarp->Add(szr_timewarp, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_timewarp, 0, wxEXPAND | wxALL, 5);

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	// -- VR Game --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, wxTRANSLATE("VR Game"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);

		// - vr
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 5);

		// Units Per Metre
		{
			SettingNumber *const spin_scale = CreateNumber(page_vr, vconfig.fUnitsPerMetre,
				wxGetTranslation(temp_desc), 0.0000001f, 10000000, 0.5f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Units Per Metre:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_scale);
		}
		// HUD 3D Items Closer (3D items drawn on the HUD, like A button in Zelda 64)
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fHud3DCloser,
				wxGetTranslation(temp_desc), 0.0f, 1.0f, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("HUD 3D Items Closer:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// HUD distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fHudDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("HUD Distance:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// HUD thickness
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fHudThickness,
				wxGetTranslation(temp_desc), 0, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("HUD Thickness:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera forward
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fCameraForward,
				wxGetTranslation(temp_desc), -10000, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Camera Forward:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fCameraPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Camera Pitch:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Aim distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fAimDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Aim Distance:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Height
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenHeight,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("2D Screen Height:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("2D Screen Distance:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Thickness
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenThickness,
				wxGetTranslation(temp_desc), 0, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("2D Screen Thickness:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Up
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenUp,
				wxGetTranslation(temp_desc), -10000, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("2D Screen Up:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("2D Screen Pitch:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Min FOV
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fMinFOV,
				wxGetTranslation(temp_desc), 0, 179, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Min HFOV:"));
			label->SetToolTip(wxGetTranslation(minfov_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera read pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fReadPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Camera Read Pitch:"));
			label->SetToolTip(wxGetTranslation(readpitch_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}

		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("HUD on Top"), wxGetTranslation(hudontop_desc), vconfig.bHudOnTop));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Don't Clear Screen"), wxGetTranslation(dontclearscreen_desc), vconfig.bDontClearScreen));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Read Camera Angles"), wxGetTranslation(canreadcamera_desc), vconfig.bCanReadCameraAngles));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Detect Skybox"), wxGetTranslation(detectskybox_desc), vconfig.bDetectSkybox));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, wxTRANSLATE("For This Game Only"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		wxButton* const btn_save = new wxButton(page_vr, wxID_ANY, wxTRANSLATE("Save"));
		btn_save->Bind(wxEVT_BUTTON, &CConfigVR::Event_ClickSave, this);
		szr_vr->Add(btn_save, 1, wxALIGN_CENTER_VERTICAL, 0);
		wxButton* const btn_reset = new wxButton(page_vr, wxID_ANY, wxTRANSLATE("Reset to Defaults"));
		btn_reset->Bind(wxEVT_BUTTON, &CConfigVR::Event_ClickReset, this);
		szr_vr->Add(btn_reset, 1, wxALIGN_CENTER_VERTICAL, 0);
		if (SConfig::GetInstance().m_strUniqueID == "")
		{
			btn_save->Disable();
			btn_reset->Disable();
			page_vr->Disable();
		}

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	// -- Avatar --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, wxTRANSLATE("Avatar"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);

		// - vr
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 5);

		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Controller"), wxGetTranslation(showcontroller_desc), vconfig.bShowController));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Hands"), wxGetTranslation(showhands_desc), vconfig.bShowHands));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Feet"), wxGetTranslation(showfeet_desc), vconfig.bShowFeet));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Sensor Bar"), wxGetTranslation(showsensorbar_desc), vconfig.bShowSensorBar));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Game Camera"), wxGetTranslation(showgamecamera_desc), vconfig.bShowGameCamera));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Game Camera Frustum"), wxGetTranslation(showgamecamerafrustum_desc), vconfig.bShowGameFrustum));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Tracking Camera"), wxGetTranslation(showtrackingcamera_desc), vconfig.bShowTrackingCamera));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Tracking Volume"), wxGetTranslation(showtrackingvolume_desc), vconfig.bShowTrackingVolume));
		//szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Show Hydra Base Station"), wxGetTranslation(showbasestation_desc), vconfig.bShowBaseStation));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, wxTRANSLATE("Show Avatar"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	// -- Motion Sickness --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, wxTRANSLATE("Motion Sickness"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 5);

		// Sky / Background
		{
			const wxString vr_choices[] = { wxTRANSLATE("Normal"), wxTRANSLATE("Hide"), wxTRANSLATE("Lock") };
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Sky / Background:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iMotionSicknessSkybox, wxGetTranslation(hideskybox_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iMotionSicknessSkybox);
		}
		// Motion Sickness Prevention Method
		{
			const wxString vr_choices[] = { wxTRANSLATE("None"), wxTRANSLATE("Reduce FOV"), wxTRANSLATE("Black Screen")};
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Motion Sickness Prevention Method:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iMotionSicknessMethod, wxGetTranslation(motionsicknessprevention_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iMotionSicknessMethod);
		}
		// Motion Sickness FOV
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fMotionSicknessFOV,
				wxGetTranslation(motionsicknessfov_desc), 1, 179, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, wxTRANSLATE("Reduced FOV:"));
			label->SetToolTip(wxGetTranslation(motionsicknessfov_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}

		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("Always"), wxGetTranslation(motionsicknessalways_desc), vconfig.bMotionSicknessAlways));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("During Freelook"), wxGetTranslation(motionsicknessfreelook_desc), vconfig.bMotionSicknessFreelook));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("On 2D Screens"), wxGetTranslation(motionsickness2d_desc), vconfig.bMotionSickness2D));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("With Left Stick"), wxGetTranslation(motionsicknessleftstick_desc), vconfig.bMotionSicknessLeftStick));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("With Right Stick"), wxGetTranslation(motionsicknessrightstick_desc), vconfig.bMotionSicknessRightStick));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("With D-Pad"), wxGetTranslation(motionsicknessdpad_desc), vconfig.bMotionSicknessDPad));
		szr_vr->Add(CreateCheckBox(page_vr, wxTRANSLATE("With IR Pointer"), wxGetTranslation(motionsicknessir_desc), vconfig.bMotionSicknessIR));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, wxTRANSLATE("Motion Sickness"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	const wxString pageNames[] =
	{
		wxTRANSLATE("VR Instructions")
	};

	for (int j = 0; j < 1; ++j)
	{
		wxPanel *Page = new wxPanel(Notebook, wxID_ANY);
		Notebook->AddPage(Page, pageNames[j]);

		if (j == 0) 
		{
			//Create "Device" box, and all buttons/options within it/
			wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxHORIZONTAL, Page, wxTRANSLATE("Instructions"));

			wxStaticText* const instruction_box = new wxStaticText(Page, wxID_ANY,
				wxTRANSLATE("Dolphin VR has a lot of options and it can be confusing to set them all correctly. Here's a quick setup "
				"guide to help.\n\nDirect mode is working, but the performance on many games is poor. It is recommended to run in "
				"extended mode! If you do run in direct mode, make sure Aero is enabled. Some games are now working judder free, but "
				"many require 'dual core determinism' set to 'fake-completion', 'synchronize GPU thread' to be enabled, or dual core mode "
				"to be disabled. The mirrored window must also "
				"be enabled and manually resized to 0 pixels by 0 pixles. Testing has been limited so your mileage may vary."
				"\n\nIn the 'Config' tab, 'Framelimit' should be set to match your Rift's refresh rate (most likely 75fps). "
				"Games will run 25% too fast, but this will avoid judder. There are two options available to bring the game frame rate back down to "
				"normal speed. The first is the 'Opcode Replay Buffer', which rerenders game frames so headtracking runs at 75fps forcing "
				"the game to maintain its normal speed.  This feature works in around 75% of games, and is the preferred "
				"method.  Please leave 'Enable Idle Skipping' checked, as it is required for many games when the 'Opcode Replay Buffer' "
				"is enabled. 'Synchronous Timewarp' smudges the image to fake head-tracking at a higher rate.  This can be tried if 'Opcode Replay Buffer' fails "
				"but sometimes introduces artifacts and may judder depending on the game. As a last resort, the the Rift can be "
				"set to run at 60hz from your AMD/Nvidia control panel and still run with low persistence, but flickering can be seen "
				"in bright scenes. Different games run at differnet frame-rates, so choose the correct opcode replay/timewarp setting for the game.\n\n"
				"Under Graphics->Hacks->EFB Copies, 'Disable' and 'Remove Blank EFB Copy Box' can fix some games "
				"that display black, or large black boxes that can't be removed by the 'Hide Objects Codes'. It can also cause "
				"corruption in some games, so only enable it if it's needed.\n\n"
				"Right-clicking on each game will give you the options to adjust VR settings, hide rendered objects, and (in "
				"a limited number of games) disable culling through the use of included AR codes. Culling codes put extra strain "
				"on the emulated CPU as well as your PC.  You may need to override the Dolphin CPU clockspeed under "
				"the advanced section of the 'config' tab to maintain a full game speed. This will only help if gamecube/wii CPU is "
				"not fast enough to render the game without culling. "
				"Objects such as fake 16:9 bars can be removed from the game.  Some games already have hide objects codes, "
				"so make sure to check them if you would like the object removed.  You can find your own codes by starting "
				"with an 8-bit code and clicking the up button until the object disappears.  Then move to 16bit "
				"and continue hitting the up button until the object disappears again. "
				"Continue until you have a long enough code for it to be unique.\n\nTake time to set "
				"the VR-Hotkeys. You can create combinations for XInput by right clicking on the buttons. Remember you can also set "
				"a the gamecube/wii controller emulation to not happen during a certain button press, so setting freelook to 'Left "
				"Bumper + Right Analog Stick' and the gamecube C-Stick to '!Left Bumper + Right Analog Stick' works great. If you're "
				"changing the scale in game, use the 'global scale' instead of 'units per metre' to maintain a consistent free look step size.\n\n"
				"Enjoy and watch for new updates!"));

			instruction_box->Wrap(630);

			device_sbox->Add(instruction_box, wxEXPAND | wxALL);

			wxBoxSizer* const sPage = new wxBoxSizer(wxVERTICAL);
			sPage->Add(device_sbox, 0, wxEXPAND | wxALL, 5);
			Page->SetSizer(sPage);
		}
	}

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(Notebook, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);
	SetSizerAndFit(sMainSizer);
	SetFocus();

}

SettingCheckBox* CConfigVR::CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingCheckBox* const cb = new SettingCheckBox(parent, label, wxString(), setting, reverse, style);
	RegisterControl(cb, description);
	return cb;
}

SettingChoice* CConfigVR::CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num, const wxString choices[], long style)
{
	SettingChoice* const ch = new SettingChoice(parent, setting, wxString(), num, choices, style);
	RegisterControl(ch, description);
	return ch;
}

SettingRadioButton* CConfigVR::CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingRadioButton* const rb = new SettingRadioButton(parent, label, wxString(), setting, reverse, style);
	RegisterControl(rb, description);
	return rb;
}

SettingNumber* CConfigVR::CreateNumber(wxWindow* parent, float &setting, const wxString& description, float min, float max, float inc, long style)
{
	SettingNumber* const sn = new SettingNumber(parent, wxString(), setting, min, max, inc, style);
	RegisterControl(sn, description);
	return sn;
}

/* Use this to register descriptions for controls which have NOT been created using the Create* functions from above */
wxControl* CConfigVR::RegisterControl(wxControl* const control, const wxString& description)
{
	ctrl_descs.insert(std::pair<wxWindow*, wxString>(control, description));
	control->Bind(wxEVT_ENTER_WINDOW, &CConfigVR::Evt_EnterControl, this);
	control->Bind(wxEVT_LEAVE_WINDOW, &CConfigVR::Evt_LeaveControl, this);
	return control;
}

void CConfigVR::Evt_EnterControl(wxMouseEvent& ev)
{
	// TODO: Re-Fit the sizer if necessary!

	// Get settings control object from event
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;

	// look up description text object from the control's parent (which is the wxPanel of the current tab)
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	// look up the description of the selected control and assign it to the current description text object's label
	descr_text->SetLabel(ctrl_descs[ctrl]);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);

	ev.Skip();
}

// TODO: Don't hardcode the size of the description area via line breaks
#define DEFAULT_DESC_TEXT wxTRANSLATE("Move the mouse pointer over an option to display a detailed description.\n\n\n\n\n\n\n")
void CConfigVR::Evt_LeaveControl(wxMouseEvent& ev)
{
	// look up description text control and reset its label
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	descr_text->SetLabel(DEFAULT_DESC_TEXT);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);
	ev.Skip();
}

void CConfigVR::CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer)
{
	// Create description frame
	wxStaticBoxSizer* const desc_sizer = new wxStaticBoxSizer(wxVERTICAL, page, wxTRANSLATE("Description"));
	sizer->Add(desc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Need to call SetSizerAndFit here, since we don't want the description texts to change the dialog width
	page->SetSizerAndFit(sizer);

	// Create description text
	wxStaticText* const desc_text = new wxStaticText(page, wxID_ANY, DEFAULT_DESC_TEXT);
	desc_text->Wrap(desc_sizer->GetSize().x - 20);
	desc_sizer->Add(desc_text, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Store description text object for later lookup
	desc_texts.insert(std::pair<wxWindow*, wxStaticText*>(page, desc_text));
}

// On Keyhole Checkbox Click
void CConfigVR::OnKeyholeCheckbox(wxCommandEvent& event)
{
	if (checkbox_keyhole->IsChecked())
	{
		vconfig.bKeyhole = true;
		keyhole_width->Enable();

		checkbox_yaw->SetValue(true);
		vconfig.bStabilizeYaw = true;
	}
	else
	{
		vconfig.bKeyhole = false;
		keyhole_width->Disable();

		checkbox_keyhole_snap->SetValue(false);
		vconfig.bKeyholeSnap = false;
		keyhole_snap_size->Disable();
	}
}

// On Keyhole Checkbox Click
void CConfigVR::OnKeyholeSnapCheckbox(wxCommandEvent& event)
{
	if (checkbox_keyhole_snap->IsChecked())
	{
		keyhole_snap_size->Enable();
		vconfig.bKeyholeSnap = true;
		checkbox_yaw->SetValue(true);
		checkbox_keyhole->SetValue(true);
		keyhole_width->Enable();
		vconfig.bStabilizeYaw = true;
		vconfig.bKeyhole = true;
	}
	else
	{
		keyhole_snap_size->Disable();
		vconfig.bKeyholeSnap = false;
	}
}

void CConfigVR::OnYawCheckbox(wxCommandEvent& event)
{
	if (checkbox_yaw->IsChecked())
	{
		vconfig.bStabilizeYaw = true;
	}
	else
	{
		checkbox_keyhole->SetValue(false);
		checkbox_keyhole_snap->SetValue(false);
		vconfig.bKeyholeSnap = false;
		vconfig.bStabilizeYaw = false;
		vconfig.bKeyhole = false;
		keyhole_width->Disable();
		keyhole_snap_size->Disable();
	}
}

void CConfigVR::OnTimewarpSpinCtrl(wxCommandEvent& event)
{
	vconfig.iExtraTimewarpedFrames = event.GetInt();

	if (checkbox_pullup20_timewarp->IsChecked() || checkbox_pullup30_timewarp->IsChecked() || checkbox_pullup60_timewarp->IsChecked() || vconfig.iExtraTimewarpedFrames)
		vconfig.bSynchronousTimewarp = true;
	else
		vconfig.bSynchronousTimewarp = false;

	checkbox_pullup20->Enable(!vconfig.bSynchronousTimewarp);
	checkbox_pullup30->Enable(!vconfig.bSynchronousTimewarp);
	checkbox_pullup60->Enable(!vconfig.bSynchronousTimewarp);
	checkbox_replay_vertex_data->Enable(!vconfig.bSynchronousTimewarp);
	checkbox_replay_other_data->Enable(!vconfig.bSynchronousTimewarp);

	spin_replay_buffer->Enable(!vconfig.bSynchronousTimewarp);

	checkbox_pullup20_timewarp->Enable(!vconfig.iExtraTimewarpedFrames);
	checkbox_pullup30_timewarp->Enable(!vconfig.iExtraTimewarpedFrames);
	checkbox_pullup60_timewarp->Enable(!vconfig.iExtraTimewarpedFrames);
}

void CConfigVR::OnOpcodeSpinCtrl(wxCommandEvent& event)
{
	vconfig.iExtraVideoLoops = event.GetInt();

	if (checkbox_pullup20->IsChecked() || checkbox_pullup30->IsChecked() || checkbox_pullup60->IsChecked() || vconfig.iExtraVideoLoops)
	{
		vconfig.bOpcodeReplay = true;
		if (checkbox_replay_vertex_data->IsChecked())
		{
			vconfig.bReplayVertexData = true;
		}
		if (checkbox_replay_other_data->IsChecked())
		{
			vconfig.bReplayOtherData = true;
		}
	}
	else
	{
		vconfig.bOpcodeReplay = false;
		vconfig.bReplayVertexData = false;
		vconfig.bReplayOtherData = false;
	}

	checkbox_pullup20_timewarp->Enable(!vconfig.bOpcodeReplay);
	checkbox_pullup30_timewarp->Enable(!vconfig.bOpcodeReplay);
	checkbox_pullup60_timewarp->Enable(!vconfig.bOpcodeReplay);
	spin_timewarped_frames->Enable(!vconfig.bOpcodeReplay);
	spin_timewarp_tweak->Enable(!vconfig.bOpcodeReplay);

	checkbox_pullup20->Enable(!vconfig.iExtraVideoLoops);
	checkbox_pullup30->Enable(!vconfig.iExtraVideoLoops);
	checkbox_pullup60->Enable(!vconfig.iExtraVideoLoops);
	checkbox_replay_vertex_data->Enable(vconfig.bOpcodeReplay);
	checkbox_replay_other_data->Enable(vconfig.bOpcodeReplay);
}

// On Pullup Checkbox Click
void CConfigVR::OnPullupCheckbox(wxCommandEvent& event)
{
	if (checkbox_pullup20_timewarp->IsChecked() || checkbox_pullup30_timewarp->IsChecked() || checkbox_pullup60_timewarp->IsChecked())
		vconfig.bSynchronousTimewarp = true;
	else
		vconfig.bSynchronousTimewarp = false;

	if (checkbox_pullup20->IsChecked() || checkbox_pullup30->IsChecked() || checkbox_pullup60->IsChecked())
	{
		vconfig.bOpcodeReplay = true;
		if (checkbox_replay_vertex_data->IsChecked())
		{
			vconfig.bReplayVertexData = true;
		}
		if (checkbox_replay_other_data->IsChecked())
		{
			vconfig.bReplayOtherData = true;
		}
	}
	else
	{
		vconfig.bOpcodeReplay = false;
		vconfig.bReplayVertexData = false;
		vconfig.bReplayOtherData = false;
	}

	checkbox_pullup20->Enable(!vconfig.bSynchronousTimewarp);
	checkbox_pullup30->Enable(!vconfig.bSynchronousTimewarp);
	checkbox_pullup60->Enable(!vconfig.bSynchronousTimewarp);

	checkbox_pullup20_timewarp->Enable(!vconfig.bOpcodeReplay);
	checkbox_pullup30_timewarp->Enable(!vconfig.bOpcodeReplay);
	checkbox_pullup60_timewarp->Enable(!vconfig.bOpcodeReplay);

	checkbox_replay_vertex_data->Enable(!vconfig.bSynchronousTimewarp && vconfig.bOpcodeReplay);
	checkbox_replay_other_data->Enable(!vconfig.bSynchronousTimewarp && vconfig.bOpcodeReplay);

	spin_replay_buffer->Enable(!vconfig.bOpcodeReplay && !vconfig.bSynchronousTimewarp);
	//spin_replay_buffer_divider->Enable(!vconfig.bOpcodeReplay && !vconfig.bSynchronousTimewarp);
	spin_timewarped_frames->Enable(!vconfig.bOpcodeReplay && !vconfig.bSynchronousTimewarp);
	spin_timewarp_tweak->Enable(!vconfig.bOpcodeReplay);

	event.Skip();
}

void CConfigVR::Event_ClickSave(wxCommandEvent&)
{
	if (SConfig::GetInstance().m_strUniqueID != "")
		g_Config.GameIniSave();
}

void CConfigVR::Event_ClickReset(wxCommandEvent&)
{
	if (SConfig::GetInstance().m_strUniqueID != "")
	{
		g_Config.GameIniReset();
		Close();
	}
}

void CConfigVR::OnClose(wxCloseEvent& WXUNUSED(event))
{
	g_Config.SaveVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	EndModal(wxID_OK);
	// Save the config. Dolphin crashes too often to only save the settings on closing
	SConfig::GetInstance().SaveSettings();
}

void CConfigVR::OnOk(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

