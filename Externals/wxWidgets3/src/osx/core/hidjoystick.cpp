/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/joystick.cpp
// Purpose:     wxJoystick class
// Author:      Ryan Norton
// Modified by:
// Created:     2/13/2005
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//===========================================================================
//  DECLARATIONS
//===========================================================================

//---------------------------------------------------------------------------
// Pre-compiled header stuff
//---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

//---------------------------------------------------------------------------
// Guard
//---------------------------------------------------------------------------

//we only support HID on OSX (DARWIN), since it requires DARWIN...
#if wxUSE_JOYSTICK && wxUSE_THREADS

//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/event.h"   //joystick wxEvents
    #include "wx/window.h"  //for wxWindow to "capture" joystick
#endif

#include "wx/joystick.h"    //...
#include "wx/thread.h"      //wxThread for polling thread/ wxCriticalSection

//private headers
#include "wx/osx/core/hid.h" //private mac hid stuff

//mac headers
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>

//---------------------------------------------------------------------------
// Definitions/Enumerations
//---------------------------------------------------------------------------

#define wxJS_MAX_AXES       10 /*max number of axes*/
#define wxJS_MAX_BUTTONS    40 /*max number of buttons*/

enum
{
    //These are positions within the cookie array
    //in wxHIDJoystick that the cookies that store the axis' are
    wxJS_AXIS_X = 40,
    wxJS_AXIS_Y,
    wxJS_AXIS_Z,
    wxJS_AXIS_RUDDER,
    wxJS_AXIS_U,
    wxJS_AXIS_V,
};

//---------------------------------------------------------------------------
// wxHIDJoystick
//---------------------------------------------------------------------------
class wxHIDJoystick : public wxHIDDevice
{
public:
    wxHIDJoystick();
    virtual ~wxHIDJoystick();

    bool Create(int nWhich);
    virtual void BuildCookies(CFArrayRef Array) wxOVERRIDE;
    void MakeCookies(CFArrayRef Array);
    IOHIDElementCookie* GetCookies();
    IOHIDQueueInterface** GetQueue();

    int  m_nXMax, m_nYMax, m_nZMax, m_nRudderMax, m_nUMax, m_nVMax,
         m_nXMin, m_nYMin, m_nZMin, m_nRudderMin, m_nUMin, m_nVMin;

    friend class wxJoystick;
};

//---------------------------------------------------------------------------
// wxJoystickThread
//---------------------------------------------------------------------------
class wxJoystickThread : public wxThread
{
public:
    wxJoystickThread(wxHIDJoystick* hid, int joystick);
    void* Entry() wxOVERRIDE;
    static void HIDCallback(void* target, IOReturn res, void* context, void* sender);

private:
    wxHIDJoystick*       m_hid;
    int       m_joystick;
    wxPoint   m_lastposition;
    int       m_axe[wxJS_MAX_AXES];
    int       m_buttons;
    wxWindow* m_catchwin;
    int       m_polling;

    friend class wxJoystick;
};

//===========================================================================
//  IMPLEMENTATION
//===========================================================================

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// wxGetIntFromCFDictionary
//
// Helper function that gets a integer from a dictionary key
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
void wxGetIntFromCFDictionary(CFTypeRef cfDict, CFStringRef key, int* pOut)
{
        CFNumberGetValue(
          (CFNumberRef) CFDictionaryGetValue((CFDictionaryRef) cfDict,
                                              key),
                        kCFNumberIntType, pOut);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxJoystick
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

wxIMPLEMENT_DYNAMIC_CLASS(wxJoystick, wxObject);

//---------------------------------------------------------------------------
// wxJoystick Constructor
//
// 1) Initializes member variables
// 2) Attempts to create the native HID joystick implementation - if none
//    could be found (no joysticks, etc.) then it sets it to NULL
//---------------------------------------------------------------------------
wxJoystick::wxJoystick(int joystick)
    : m_joystick(joystick),
      m_thread(NULL)
{
    m_hid = new wxHIDJoystick();

    if (m_hid->Create(m_joystick+1)) //wxHIDDevice is 1-based while this is 0
    {
        m_thread = new wxJoystickThread(m_hid, m_joystick);
        m_thread->Create();
        m_thread->Run();
    }
    else
    {
        wxDELETE(m_hid);
    }
}

//---------------------------------------------------------------------------
// wxJoystick Destructor
//
// Releases the capture of the thread, deletes it, and deletes
// the native implementation.
//---------------------------------------------------------------------------
wxJoystick::~wxJoystick()
{
    ReleaseCapture();
    if (m_thread)
        m_thread->Delete();  // It's detached so it will delete itself

    delete m_hid;
}

//---------------------------------------------------------------------------
// wxJoystick::Get[XXX]Position
//
// Returns the value of an axis that was polled from the thread. In the
// case of GetPosition returns the X and Y values in a wxPoint
//---------------------------------------------------------------------------
wxPoint wxJoystick::GetPosition() const
{
    wxPoint pos(wxDefaultPosition);
    if (m_thread) pos = m_thread->m_lastposition;
    return pos;
}
int wxJoystick::GetPosition(unsigned int axis) const
{
    wxCHECK_MSG(axis < (unsigned)GetNumberAxes(), 0, "Invalid joystick axis");
    if (m_thread)
        return m_thread->m_axe[axis];
    return 0;

}
int wxJoystick::GetZPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_Z-wxJS_AXIS_X];
    return 0;
}
int wxJoystick::GetRudderPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_RUDDER-wxJS_AXIS_X];
    return 0;
}
int wxJoystick::GetUPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_U-wxJS_AXIS_X];
    return 0;
}
int wxJoystick::GetVPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_V-wxJS_AXIS_X];
    return 0;
}

//---------------------------------------------------------------------------
// wxJoystick::GetButtonState
//
// Returns the state of the buttons in a bitmask as dictated by the
// wx manual (the real work takes place in the thread, as always)
//---------------------------------------------------------------------------
int wxJoystick::GetButtonState() const
{
    if (m_thread)
        return m_thread->m_buttons;
    return 0;
}

bool wxJoystick::GetButtonState(unsigned int id) const
{
    if (id > sizeof(int) * 8)
        return false;

    return (GetButtonState() & (1 << id)) != 0;
}

//---------------------------------------------------------------------------
// wxJoystick::IsOk
//
// Returns whether the joystick initialized successfully - in this case
// if the native implementation doesn't exist (in constructor)
//---------------------------------------------------------------------------
bool wxJoystick::IsOk() const
{
    return m_hid != NULL;
}

//---------------------------------------------------------------------------
// wxJoystick::Get[XXX](Id/Name)
//
// Simple accessors to the native HID implementation
//---------------------------------------------------------------------------
int wxJoystick::GetManufacturerId() const
{
    return m_hid->m_nManufacturerId;
}

int wxJoystick::GetProductId() const
{
    return m_hid->m_nProductId;
}

wxString wxJoystick::GetProductName() const
{
    return m_hid->m_szProductName;
}

//---------------------------------------------------------------------------
// wxJoystick::GetNumberButtons
// wxJoystick::GetNumberAxes
//
// Queries the joystick for an active number of buttons/axes.
//
// In the native HID implementation, the cookies:
// 0-40     are the buttons of the joystick
// 40-50    are the axes of the joystick
//
// These just query the native HID implementation as above.
//---------------------------------------------------------------------------
int wxJoystick::GetNumberButtons() const
{
    int nCount = 0;

    for(int nIndex = 0; nIndex < 40; ++nIndex)
    {
        if(m_hid->HasElement(nIndex))
            ++nCount;
    }

    return nCount;
}
int wxJoystick::GetNumberAxes() const
{
    int nCount = 0;

    for(int nIndex = 40; nIndex < 50; ++nIndex)
    {
        if(m_hid->HasElement(nIndex))
            ++nCount;
    }

    return nCount;
}

//---------------------------------------------------------------------------
// wxJoystick::GetNumberJoysticks
//
// Gets the number of joysticks on the system. In HID that
// is all devices with the kHIDUsage_GD_Joystick or kHIDUsage_GD_GamePad
// identifiers.
//---------------------------------------------------------------------------
int wxJoystick::GetNumberJoysticks()
{
    return
        wxHIDDevice::GetCount(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick) +
        wxHIDDevice::GetCount(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);
}

//---------------------------------------------------------------------------
// wxJoystick::SetCapture
//
// Stops sending events from the thread to the window set in
// SetCapture and stops polling the joystick
//---------------------------------------------------------------------------
bool wxJoystick::SetCapture(wxWindow* win, int pollingFreq)
{
    if (m_thread)
    {
        m_thread->m_catchwin = win;
        m_thread->m_polling = pollingFreq;
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
// wxJoystick::ReleaseCapture
//
// Stops sending events from the thread to the window set in
// SetCapture and stops polling the joystick
//---------------------------------------------------------------------------
bool wxJoystick::ReleaseCapture()
{
    if (m_thread)
    {
        m_thread->m_catchwin = NULL;
        m_thread->m_polling = 0;
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
// wxJoystick::Get[XXX]
//
// Gets the minimum and maximum values for each axis, returning 0 if the
// axis doesn't exist.
//---------------------------------------------------------------------------
int wxJoystick::GetXMin() const
{
    return m_hid->m_nXMin;
}

int wxJoystick::GetYMin() const
{
    return m_hid->m_nYMin;
}

int wxJoystick::GetZMin() const
{
    return m_hid->m_nZMin;
}

int wxJoystick::GetRudderMin() const
{
    return m_hid->m_nRudderMin;
}

int wxJoystick::GetUMin() const
{
    return m_hid->m_nUMin;
}

int wxJoystick::GetVMin() const
{
    return m_hid->m_nVMin;
}

int wxJoystick::GetXMax() const
{
    return m_hid->m_nXMax;
}

int wxJoystick::GetYMax() const
{
    return m_hid->m_nYMax;
}

int wxJoystick::GetZMax() const
{
    return m_hid->m_nZMax;
}

int wxJoystick::GetRudderMax() const
{
    return m_hid->m_nRudderMax;
}

int wxJoystick::GetUMax() const
{
    return m_hid->m_nUMax;
}

int wxJoystick::GetVMax() const
{
    return m_hid->m_nVMax;
}

//---------------------------------------------------------------------------
// wxJoystick::Get[XXX]
//
// Min/Max values for buttons, axes, etc.. Polling in this case is just
// what the linux port has.
//---------------------------------------------------------------------------
int wxJoystick::GetMaxButtons() const
{
    return wxJS_MAX_BUTTONS;
}

int wxJoystick::GetMaxAxes() const
{
    return wxJS_MAX_AXES;
}

int wxJoystick::GetPollingMin() const
{
    return 10;
}

int wxJoystick::GetPollingMax() const
{
    return 1000;
}

//---------------------------------------------------------------------------
// wxJoystick::Has[XXX]
//
// Just queries the native hid implementation if the cookie was found
// when enumerating the cookies of the joystick device
//---------------------------------------------------------------------------
bool wxJoystick::HasZ() const
{
    return m_hid->HasElement(wxJS_AXIS_Z);
}

bool wxJoystick::HasRudder() const
{
    return m_hid->HasElement(wxJS_AXIS_RUDDER);
}

bool wxJoystick::HasU() const
{
    return m_hid->HasElement(wxJS_AXIS_U);
}

bool wxJoystick::HasV() const
{
    return m_hid->HasElement(wxJS_AXIS_V);
}

//---------------------------------------------------------------------------
// UNSUPPORTED
//---------------------------------------------------------------------------
int wxJoystick::GetPOVPosition() const
{
    return -1;
}

int wxJoystick::GetPOVCTSPosition() const
{
    return -1;
}

int wxJoystick::GetMovementThreshold() const
{
    return 0;
}

void wxJoystick::SetMovementThreshold(int WXUNUSED(threshold))
{
}

bool wxJoystick::HasPOV() const
{
    return false;
}

bool wxJoystick::HasPOV4Dir() const
{
    return false;
}

bool wxJoystick::HasPOVCTS() const
{
    return false;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxHIDJoystick
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
// wxHIDJoystick ctor
//
// Initializes the min/max members
//---------------------------------------------------------------------------
wxHIDJoystick::wxHIDJoystick() :
 m_nXMax(0), m_nYMax(0), m_nZMax(0), m_nRudderMax(0), m_nUMax(0), m_nVMax(0),
 m_nXMin(0), m_nYMin(0), m_nZMin(0), m_nRudderMin(0), m_nUMin(0), m_nVMin(0)
{
}

//---------------------------------------------------------------------------
// wxHIDJoystick dtor
//
// Nothing...
//---------------------------------------------------------------------------
wxHIDJoystick::~wxHIDJoystick()
{
}

//---------------------------------------------------------------------------
// wxHIDJoystick::Create
//
// Creates the native HID device (joysticks are of either
// kHIDUsage_GD_Joystick or kHIDUsage_GD_GamePad)
//---------------------------------------------------------------------------
bool wxHIDJoystick::Create(int nWhich)
{
    int nJoysticks = GetCount(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);

    if (nWhich <= nJoysticks)
        return wxHIDDevice::Create(kHIDPage_GenericDesktop,
                                   kHIDUsage_GD_Joystick,
                                   nWhich);
    else
        nWhich -= nJoysticks;

    int nGamePads = GetCount(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);

    if (nWhich <= nGamePads)
        return wxHIDDevice::Create(kHIDPage_GenericDesktop,
                                   kHIDUsage_GD_GamePad,
                                   nWhich);
    else
    return false;
}

//---------------------------------------------------------------------------
// wxHIDJoystick::BuildCookies
// wxHIDJoystick::MakeCookies
//
// Sets up the cookies for the HID device (called from Create) - as
// mentioned 0-40 are the buttons and 40-50 are the axes.
//
// MakeCookies is just a recursive function for each array within
// BuildCookies.
//---------------------------------------------------------------------------
void wxHIDJoystick::BuildCookies(CFArrayRef Array)
{
    InitCookies(50, true);

    //
    // I wasted two hours of my life on this line :(
    // accidentally removed it during some source cleaning...
    //
    MakeCookies(Array);

    //paranoid debugging stuff
#if 0
    for(int i = 0; i < 50; ++i)
        wxPrintf(wxT("\nVAL #%i:[%i]"), i, m_pCookies[i]);
#endif
}//end buildcookies

void wxHIDJoystick::MakeCookies(CFArrayRef Array)
{
    int i, nUsage, nPage;

    for (i = 0; i < CFArrayGetCount(Array); ++i)
    {
        const void* ref = CFDictionaryGetValue(
                (CFDictionaryRef)CFArrayGetValueAtIndex(Array, i),
                CFSTR(kIOHIDElementKey)
                                              );

        if (ref != NULL)
        {
            MakeCookies((CFArrayRef) ref);
    }
        else
        {
            CFNumberGetValue(
                (CFNumberRef)
                    CFDictionaryGetValue(
                        (CFDictionaryRef) CFArrayGetValueAtIndex(Array, i),
                        CFSTR(kIOHIDElementUsageKey)
                                        ),
                kCFNumberIntType,
                &nUsage    );

            CFNumberGetValue(
                (CFNumberRef)
                    CFDictionaryGetValue(
                        (CFDictionaryRef) CFArrayGetValueAtIndex(Array, i),
                        CFSTR(kIOHIDElementUsagePageKey)
                                        ),
                kCFNumberIntType,
                &nPage     );

#if 0
            wxLogSysError(wxT("[%i][%i]"), nUsage, nPage);
#endif
            if (nPage == kHIDPage_Button && nUsage <= 40)
                AddCookieInQueue(CFArrayGetValueAtIndex(Array, i), nUsage-1 );
            else if (nPage == kHIDPage_GenericDesktop)
            {
                //axis...
                switch(nUsage)
                {
                    case kHIDUsage_GD_X:
                        AddCookieInQueue(CFArrayGetValueAtIndex(Array, i), wxJS_AXIS_X);
                        wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                                 CFSTR(kIOHIDElementMaxKey),
                                                 &m_nXMax);
                        wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                                 CFSTR(kIOHIDElementMinKey),
                                                 &m_nXMin);
                        break;
                    case kHIDUsage_GD_Y:
                        AddCookieInQueue(CFArrayGetValueAtIndex(Array, i), wxJS_AXIS_Y);
                        wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                                 CFSTR(kIOHIDElementMaxKey),
                                                 &m_nYMax);
                        wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                                 CFSTR(kIOHIDElementMinKey),
                                                 &m_nYMin);
                        break;
                    case kHIDUsage_GD_Z:
                        AddCookieInQueue(CFArrayGetValueAtIndex(Array, i), wxJS_AXIS_Z);
                        wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                                 CFSTR(kIOHIDElementMaxKey),
                                                 &m_nZMax);
                        wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                                 CFSTR(kIOHIDElementMinKey),
                                                 &m_nZMin);
                        break;
                    default:
                        break;
                }
            }
            else if (nPage == kHIDPage_Simulation && nUsage == kHIDUsage_Sim_Rudder)
            {
                //rudder...
                AddCookieInQueue(CFArrayGetValueAtIndex(Array, i), wxJS_AXIS_RUDDER );
                wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                         CFSTR(kIOHIDElementMaxKey),
                                         &m_nRudderMax);
                wxGetIntFromCFDictionary(CFArrayGetValueAtIndex(Array, i),
                                         CFSTR(kIOHIDElementMinKey),
                                         &m_nRudderMin);
            }
        }
    }
}

//---------------------------------------------------------------------------
// wxHIDJoystick::Get[XXX]
//
// Simple accessors so that the HID callback and the thread procedure
// can access members from wxHIDDevice (our parent here).
//---------------------------------------------------------------------------
IOHIDElementCookie* wxHIDJoystick::GetCookies()
{   return m_pCookies;  }
IOHIDQueueInterface** wxHIDJoystick::GetQueue()
{   return m_ppQueue;   }

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxJoystickThread
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
// wxJoystickThread Constructor
//
// Just initializes members
//---------------------------------------------------------------------------
wxJoystickThread::wxJoystickThread(wxHIDJoystick* hid, int joystick)
    : m_hid(hid),
      m_joystick(joystick),
      m_lastposition(127,127),
      m_buttons(0),
      m_catchwin(NULL),
      m_polling(0)
{
    memset(m_axe, 0, sizeof(int) * wxJS_MAX_AXES);
}

//---------------------------------------------------------------------------
// wxJoystickThread::Entry
//
// Thread procedure
//
// Runs a CFRunLoop for polling. Basically, it sets the HID queue to
// call wxJoystickThread::HIDCallback in the context of this thread
// when something changes on the device. It polls as long as the user
// wants, or a certain amount if the user wants to "block". Note that
// we don't actually block here since this is in a secondary thread.
//---------------------------------------------------------------------------
void* wxJoystickThread::Entry()
{
    CFRunLoopSourceRef pRLSource = NULL;

    if ((*m_hid->GetQueue())->createAsyncEventSource(
                    m_hid->GetQueue(), &pRLSource) != kIOReturnSuccess )
    {
        wxLogSysError(wxT("Couldn't create async event source"));
        return NULL;
    }

    wxASSERT(pRLSource != NULL);

    //attach runloop source to main run loop in thread
    CFRunLoopRef pRL = CFRunLoopGetCurrent();
    CFRunLoopAddSource(pRL, pRLSource, kCFRunLoopDefaultMode);
    wxASSERT( CFRunLoopContainsSource(pRL, pRLSource, kCFRunLoopDefaultMode) );


    if( (*m_hid->GetQueue())->setEventCallout(m_hid->GetQueue(),
          wxJoystickThread::HIDCallback, this, this) != kIOReturnSuccess )
    {
        wxLogSysError(wxT("Could not set event callout for queue"));
        return NULL;
    }

    if( (*m_hid->GetQueue())->start(m_hid->GetQueue()) != kIOReturnSuccess )
    {
        wxLogSysError(wxT("Could not start queue"));
        return NULL;
    }

    double dTime;

    while(true)
    {
        if (TestDestroy())
            break;

        if (m_polling)
            dTime = 0.0001 * m_polling;
        else
            dTime = 0.0001 * 10;  // check at least every 10 msec in "blocking" case

        //true just "handles and returns" - false forces it to stay the time
        //amount
#if 1
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, dTime, true);
#else
        IOReturn ret = NULL;
        HIDCallback(this, ret, this, this);
        Sleep(3000);
#endif
    }

    wxASSERT( CFRunLoopContainsSource(pRL, pRLSource, kCFRunLoopDefaultMode) );

    CFRunLoopRemoveSource(pRL, pRLSource, kCFRunLoopDefaultMode);
    CFRelease(pRLSource);

    return NULL;
}

//---------------------------------------------------------------------------
// wxJoystickThread::HIDCallback (static)
//
// Callback for the native HID device when it receives input.
//
// This is where the REAL dirty work gets done.
//
// 1) Loops through each event the queue has received
// 2) First, checks if the thread that is running the loop for
//    the polling has ended - if so it breaks out
// 3) Next, it checks if there was an error getting this event from
//    the HID queue, if there was, it logs an error and returns
// 4) Now it does the real dirty work by getting the button states
//    from cookies 0-40 and axes positions/states from cookies 40-50
//    in the native HID device by quering cookie values.
// 5) Sends the event to the polling window (if any)
// 6) Gets the next event and goes back to (1)
//---------------------------------------------------------------------------
/*static*/ void wxJoystickThread::HIDCallback(void* WXUNUSED(target),
                                              IOReturn WXUNUSED(res),
                                              void* context,
                                              void* WXUNUSED(sender))
{
    IOHIDEventStruct hidevent;
    AbsoluteTime bogustime = {0,0};
    IOReturn ret;
    wxJoystickThread* pThis = (wxJoystickThread*) context;
    wxHIDJoystick* m_hid = pThis->m_hid;

    //Get the "first" event from the queue
    //bogustime tells it we don't care at what time to start
    //where it gets the next from
    ret = (*m_hid->GetQueue())->getNextEvent(m_hid->GetQueue(),
                    &hidevent, bogustime, 0);

    while (ret != kIOReturnUnderrun)
    {
        if (pThis->TestDestroy())
            break;

        if(ret != kIOReturnSuccess)
        {
            wxLogSysError(wxString::Format(wxT("wxJoystick Error:[%i]"), ret));
            return;
        }

        wxJoystickEvent wxevent;

        //Find the cookie that changed
        int nIndex = 0;
        IOHIDElementCookie* pCookies = m_hid->GetCookies();
        while(nIndex < 50)
        {
            if(hidevent.elementCookie == pCookies[nIndex])
                break;

            ++nIndex;
        }

        //debugging stuff
#if 0
        if(nIndex == 50)
        {
            wxLogSysError(wxString::Format(wxT("wxJoystick Out Of Bounds Error")));
            break;
        }
#endif

        //is the cookie a button?
        if (nIndex < 40)
        {
            if (hidevent.value)
            {
                pThis->m_buttons |= (1 << nIndex);
                wxevent.SetEventType(wxEVT_JOY_BUTTON_DOWN);
            }
            else
            {
                pThis->m_buttons &= ~(1 << nIndex);
                wxevent.SetEventType(wxEVT_JOY_BUTTON_UP);
            }

            wxevent.SetButtonChange(nIndex+1);
        }
        else if (nIndex == wxJS_AXIS_X)
        {
            pThis->m_lastposition.x = hidevent.value;
            wxevent.SetEventType(wxEVT_JOY_MOVE);
            pThis->m_axe[0] = hidevent.value;
        }
        else if (nIndex == wxJS_AXIS_Y)
        {
            pThis->m_lastposition.y = hidevent.value;
            wxevent.SetEventType(wxEVT_JOY_MOVE);
            pThis->m_axe[1] = hidevent.value;
        }
        else if (nIndex == wxJS_AXIS_Z)
        {
            wxevent.SetEventType(wxEVT_JOY_ZMOVE);
            pThis->m_axe[2] = hidevent.value;
        }
        else
            wxevent.SetEventType(wxEVT_JOY_MOVE);

        Nanoseconds timestamp = AbsoluteToNanoseconds(hidevent.timestamp);

        wxULongLong llTime(timestamp.hi, timestamp.lo);

        llTime /= 1000000;

        wxevent.SetTimestamp(llTime.GetValue());
        wxevent.SetJoystick(pThis->m_joystick);
        wxevent.SetButtonState(pThis->m_buttons);
        wxevent.SetPosition(pThis->m_lastposition);
        wxevent.SetZPosition(pThis->m_axe[2]);
        wxevent.SetEventObject(pThis->m_catchwin);

        if (pThis->m_catchwin)
            pThis->m_catchwin->GetEventHandler()->AddPendingEvent(wxevent);

        ret = (*m_hid->GetQueue())->getNextEvent(m_hid->GetQueue(),
                    &hidevent, bogustime, 0);
    }
}

#endif // wxUSE_JOYSTICK
