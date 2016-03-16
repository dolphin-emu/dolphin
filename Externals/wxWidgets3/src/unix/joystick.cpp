/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/joystick.cpp
// Purpose:     wxJoystick class
// Author:      Ported to Linux by Guilhem Lavaux
// Modified by:
// Created:     05/23/98
// Copyright:   (c) Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_JOYSTICK

#include "wx/joystick.h"

#ifndef WX_PRECOMP
    #include "wx/event.h"
    #include "wx/window.h"
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/thread.h"

#include <linux/joystick.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

#include "wx/unix/private.h"

enum {
    wxJS_AXIS_X = 0,
    wxJS_AXIS_Y,
    wxJS_AXIS_Z,
    wxJS_AXIS_RUDDER,
    wxJS_AXIS_U,
    wxJS_AXIS_V,

    wxJS_AXIS_MAX = 32767,
    wxJS_AXIS_MIN = -32767,
    wxJS_MAX_AXES = 15,
    wxJS_MAX_BUTTONS = sizeof(int) * 8
};


wxIMPLEMENT_DYNAMIC_CLASS(wxJoystick, wxObject);


////////////////////////////////////////////////////////////////////////////
// Background thread for reading the joystick device
////////////////////////////////////////////////////////////////////////////

class wxJoystickThread : public wxThread
{
public:
    wxJoystickThread(int device, int joystick);
    void* Entry() wxOVERRIDE;

private:
    void      SendEvent(wxEventType type, long ts, int change = 0);
    int       m_device;
    int       m_joystick;
    wxPoint   m_lastposition;
    int       m_axe[wxJS_MAX_AXES];
    int       m_buttons;
    wxWindow* m_catchwin;
    int       m_polling;
    int       m_threshold;

    friend class wxJoystick;
};


wxJoystickThread::wxJoystickThread(int device, int joystick)
    : m_device(device),
      m_joystick(joystick),
      m_lastposition(wxDefaultPosition),
      m_buttons(0),
      m_catchwin(NULL),
      m_polling(0),
      m_threshold(0)
{
    memset(m_axe, 0, sizeof(m_axe));
}

void wxJoystickThread::SendEvent(wxEventType type, long ts, int change)
{
    wxJoystickEvent jwx_event(type, m_buttons, m_joystick, change);

    jwx_event.SetTimestamp(ts);
    jwx_event.SetPosition(m_lastposition);
    jwx_event.SetZPosition(m_axe[wxJS_AXIS_Z]);
    jwx_event.SetEventObject(m_catchwin);

    if (m_catchwin)
        m_catchwin->GetEventHandler()->AddPendingEvent(jwx_event);
}

void* wxJoystickThread::Entry()
{
    struct js_event j_evt;
    fd_set read_fds;
    struct timeval time_out = {0, 0};

    wxFD_ZERO(&read_fds);
    while (true)
    {
        if (TestDestroy())
            break;

        // We use select when either polling or 'blocking' as even in the
        // blocking case we need to check TestDestroy periodically
        if (m_polling)
            time_out.tv_usec = m_polling * 1000;
        else
            time_out.tv_usec = 10 * 1000; // check at least every 10 msec in blocking case

        wxFD_SET(m_device, &read_fds);
        select(m_device+1, &read_fds, NULL, NULL, &time_out);
        if (wxFD_ISSET(m_device, &read_fds))
        {
            memset(&j_evt, 0, sizeof(j_evt));
            if ( read(m_device, &j_evt, sizeof(j_evt)) == -1 )
            {
                // We can hardly do anything other than ignoring the error and
                // hope that we read the next event successfully.
                continue;
            }

            //printf("time: %d\t value: %d\t type: %d\t number: %d\n",
            //       j_evt.time, j_evt.value, j_evt.type, j_evt.number);

            if ((j_evt.type & JS_EVENT_AXIS) && (j_evt.number < wxJS_MAX_AXES))
            {
                // Ignore invalid axis.
                if ( j_evt.number >= wxJS_MAX_AXES )
                {
                    wxLogDebug(wxS("Invalid axis index %d in joystick message."),
                               j_evt.number);
                    continue;
                }

                if (   (m_axe[j_evt.number] + m_threshold < j_evt.value)
                    || (m_axe[j_evt.number] - m_threshold > j_evt.value) )
            {
                m_axe[j_evt.number] = j_evt.value;

                switch (j_evt.number)
                {
                    case wxJS_AXIS_X:
                        m_lastposition.x = j_evt.value;
                        SendEvent(wxEVT_JOY_MOVE, j_evt.time);
                        break;
                    case wxJS_AXIS_Y:
                        m_lastposition.y = j_evt.value;
                        SendEvent(wxEVT_JOY_MOVE, j_evt.time);
                        break;
                    case wxJS_AXIS_Z:
                        SendEvent(wxEVT_JOY_ZMOVE, j_evt.time);
                        break;
                    default:
                        SendEvent(wxEVT_JOY_MOVE, j_evt.time);
                        // TODO: There should be a way to indicate that the event
                        //       is for some other axes.
                        break;
                }
            }
            }

            if ( (j_evt.type & JS_EVENT_BUTTON) && (j_evt.number < wxJS_MAX_BUTTONS) )
            {
                if (j_evt.value)
                {
                    m_buttons |= (1 << j_evt.number);
                    SendEvent(wxEVT_JOY_BUTTON_DOWN, j_evt.time, j_evt.number);
                }
                else
                {
                    m_buttons &= ~(1 << j_evt.number);
                    SendEvent(wxEVT_JOY_BUTTON_UP, j_evt.time, j_evt.number);
                }
            }
        }
    }

    return NULL;
}


////////////////////////////////////////////////////////////////////////////

wxJoystick::wxJoystick(int joystick)
    : m_device(-1),
      m_joystick(joystick),
      m_thread(NULL)
{
    wxString dev_name;

     // old /dev structure
    dev_name.Printf( wxT("/dev/js%d"), joystick);
    m_device = open(dev_name.fn_str(), O_RDONLY);

    // new /dev structure with "input" subdirectory
    if (m_device == -1)
    {
        dev_name.Printf( wxT("/dev/input/js%d"), joystick);
        m_device = open(dev_name.fn_str(), O_RDONLY);
    }

    if (m_device != -1)
    {
        m_thread = new wxJoystickThread(m_device, m_joystick);
        m_thread->Create();
        m_thread->Run();
    }
}


wxJoystick::~wxJoystick()
{
    ReleaseCapture();
    if (m_thread)
        m_thread->Delete();  // It's detached so it will delete itself
    if (m_device != -1)
        close(m_device);
}


////////////////////////////////////////////////////////////////////////////
// State
////////////////////////////////////////////////////////////////////////////

wxPoint wxJoystick::GetPosition() const
{
    wxPoint pos(wxDefaultPosition);
    if (m_thread) pos = m_thread->m_lastposition;
    return pos;
}

int wxJoystick::GetPosition(unsigned axis) const
{
    if (m_thread && (axis < wxJS_MAX_AXES))
        return m_thread->m_axe[axis];
    return 0;
}

int wxJoystick::GetZPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_Z];
    return 0;
}

int wxJoystick::GetButtonState() const
{
    if (m_thread)
        return m_thread->m_buttons;
    return 0;
}

bool wxJoystick::GetButtonState(unsigned id) const
{
    if (m_thread && (id < wxJS_MAX_BUTTONS))
        return (m_thread->m_buttons & (1 << id)) != 0;
    return false;
}

int wxJoystick::GetPOVPosition() const
{
    return -1;
}

int wxJoystick::GetPOVCTSPosition() const
{
    return -1;
}

int wxJoystick::GetRudderPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_RUDDER];
    return 0;
}

int wxJoystick::GetUPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_U];
    return 0;
}

int wxJoystick::GetVPosition() const
{
    if (m_thread)
        return m_thread->m_axe[wxJS_AXIS_V];
    return 0;
}

int wxJoystick::GetMovementThreshold() const
{
    if (m_thread)
        return m_thread->m_threshold;
    return 0;
}

void wxJoystick::SetMovementThreshold(int threshold)
{
    if (m_thread)
        m_thread->m_threshold = threshold;
}

////////////////////////////////////////////////////////////////////////////
// Capabilities
////////////////////////////////////////////////////////////////////////////

bool wxJoystick::IsOk() const
{
    return (m_device != -1);
}

int wxJoystick::GetNumberJoysticks()
{
    wxString dev_name;
    int fd, j;

    for (j=0; j<4; j++) {
        dev_name.Printf(wxT("/dev/js%d"), j);
        fd = open(dev_name.fn_str(), O_RDONLY);
        if (fd == -1)
            break;
        close(fd);
    }

    if (j == 0) {
        for (j=0; j<4; j++) {
            dev_name.Printf(wxT("/dev/input/js%d"), j);
            fd = open(dev_name.fn_str(), O_RDONLY);
            if (fd == -1)
                return j;
            close(fd);
        }
    }

    return j;
}

int wxJoystick::GetManufacturerId() const
{
    return 0;
}

int wxJoystick::GetProductId() const
{
    return 0;
}

wxString wxJoystick::GetProductName() const
{
    char name[128];

    if (ioctl(m_device, JSIOCGNAME(sizeof(name)), name) < 0)
        strcpy(name, "Unknown");
    return wxString(name, wxConvLibc);
}

int wxJoystick::GetXMin() const
{
    return wxJS_AXIS_MIN;
}

int wxJoystick::GetYMin() const
{
    return wxJS_AXIS_MIN;
}

int wxJoystick::GetZMin() const
{
    return wxJS_AXIS_MIN;
}

int wxJoystick::GetXMax() const
{
    return wxJS_AXIS_MAX;
}

int wxJoystick::GetYMax() const
{
    return wxJS_AXIS_MAX;
}

int wxJoystick::GetZMax() const
{
    return wxJS_AXIS_MAX;
}

int wxJoystick::GetNumberButtons() const
{
    char nb=0;

    if (m_device != -1)
        ioctl(m_device, JSIOCGBUTTONS, &nb);

    if ((int)nb > wxJS_MAX_BUTTONS)
        nb = wxJS_MAX_BUTTONS;

    return nb;
}

int wxJoystick::GetNumberAxes() const
{
    char nb=0;

    if (m_device != -1)
        ioctl(m_device, JSIOCGAXES, &nb);

    if ((int)nb > wxJS_MAX_AXES)
        nb = wxJS_MAX_AXES;

    return nb;
}

int wxJoystick::GetMaxButtons() const
{
    return wxJS_MAX_BUTTONS; // internal
}

int wxJoystick::GetMaxAxes() const
{
    return wxJS_MAX_AXES; // internal
}

int wxJoystick::GetPollingMin() const
{
    return 10;
}

int wxJoystick::GetPollingMax() const
{
    return 1000;
}

int wxJoystick::GetRudderMin() const
{
    return wxJS_AXIS_MIN;
}

int wxJoystick::GetRudderMax() const
{
    return wxJS_AXIS_MAX;
}

int wxJoystick::GetUMin() const
{
    return wxJS_AXIS_MIN;
}

int wxJoystick::GetUMax() const
{
    return wxJS_AXIS_MAX;
}

int wxJoystick::GetVMin() const
{
    return wxJS_AXIS_MIN;
}

int wxJoystick::GetVMax() const
{
    return wxJS_AXIS_MAX;
}

bool wxJoystick::HasRudder() const
{
    return GetNumberAxes() >= wxJS_AXIS_RUDDER;
}

bool wxJoystick::HasZ() const
{
    return GetNumberAxes() >= wxJS_AXIS_Z;
}

bool wxJoystick::HasU() const
{
    return GetNumberAxes() >= wxJS_AXIS_U;
}

bool wxJoystick::HasV() const
{
    return GetNumberAxes() >= wxJS_AXIS_V;
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

////////////////////////////////////////////////////////////////////////////
// Operations
////////////////////////////////////////////////////////////////////////////

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
#endif  // wxUSE_JOYSTICK
