//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "ConfigBox.h"
#include "../nJoy.h"
#include "Images/controller.xpm"

extern CONTROLLER_INFO	*joyinfo;
extern bool emulator_running;
////////////////////////

int main_stick_x = 0;

// Set PAD status
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::PadGetStatus()
{
	//
	int _numPAD = 0;

	if (!joysticks[_numPAD].enabled)
		return;
		
	// Clear pad status
	//memset(_pPADStatus, 0, sizeof(SPADStatus));


	// Get pad status
	GetJoyState(_numPAD);
	
	// Reset!
	/*
	int base = 0x80;
	_pPADStatus->stickY = base;
	_pPADStatus->stickX = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;
	_pPADStatus->button |= PAD_USE_ORIGIN;
	*/

	// Set analog controllers
	// Set Deadzones perhaps out of function
	//int deadzone = (int)(((float)(128.00/100.00)) * (float)(joysticks[_numPAD].deadzone+1));
	//int deadzone2 = (int)(((float)(-128.00/100.00)) * (float)(joysticks[_numPAD].deadzone+1));

	// Adjust range
	// The value returned by SDL_JoystickGetAxis is a signed integer (-32768 to 32768)
	// The value used for the gamecube controller is an unsigned char (0 to 255)
	int main_stick_x = (joystate[_numPAD].axis[CTL_MAIN_X] >> 8);
	int main_stick_y = -(joystate[_numPAD].axis[CTL_MAIN_Y] >> 8);
    //int sub_stick_x = (joystate[_numPAD].axis[CTL_SUB_X] >> 8);
	//int sub_stick_y = -(joystate[_numPAD].axis[CTL_SUB_Y] >> 8);


	if (joystate[_numPAD].buttons[CTL_A_BUTTON])
	{
		PanicAlert("");
		//_pPADStatus->button |= PAD_BUTTON_A;
		//_pPADStatus->analogA = 255;			// Perhaps support pressure?
		for(int i=0; i<4 ;i++)
		{
			m_bmpDot[i]->SetPosition(wxPoint(main_stick_x += 3, main_stick_y));
		}
	}

	for(int i=0; i<4 ;i++)
	{
		//m_bmpDot[i]->SetPosition(wxPoint(main_stick_x / 4, main_stick_y / 4));
	}
}

// Populate the advanced tab
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::Update()
{
	//PadGetStatus();
}


// Populate the advanced tab
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::CreateAdvancedControls(int i)
{
	m_pInStatus[i] = new wxPanel(m_Notebook, ID_INSTATUS1 + i, wxDefaultPosition, wxDefaultSize);
	m_bmpSquare[i] = new wxStaticBitmap(m_pInStatus[i], ID_STATUSBMP1 + i, CreateBitmap(),
		//wxPoint(4, 15), wxSize(70,70));
		//wxPoint(4, 20), wxDefaultSize);
		wxDefaultPosition, wxDefaultSize);

	m_bmpDot[i] = new wxStaticBitmap(m_pInStatus[i], ID_STATUSDOTBMP1 + i, CreateBitmapDot(),
		wxPoint(40, 40), wxDefaultSize);
}


wxBitmap ConfigBox::CreateBitmap() // Create box
{
	int w = 70, h = 70;
	wxBitmap bitmap(w, h);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	wxBrush LightBlueBrush(_T("#0383f0"));
	wxPen LightBluePen(_T("#80c5fd"));
	wxPen LightGrayPen(_T("#909090"));
	dc.SetPen(LightGrayPen);
	dc.SetBrush(*wxWHITE_BRUSH);

	dc.Clear();
	dc.DrawRectangle(0, 0, w, h);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}

wxBitmap ConfigBox::CreateBitmapDot() // Create dot
{
	int w = 2, h = 2;
	wxBitmap bitmap(w, h);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	wxBrush LightBlueBrush(_T("#0383f0"));
	wxPen LightBluePen(_T("#80c5fd"));
	wxPen LightGrayPen(_T("#909090"));
	dc.SetPen(LightGrayPen);
	dc.SetBrush(*wxWHITE_BRUSH);

	dc.Clear();
	dc.DrawRectangle(0, 0, w, h);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}