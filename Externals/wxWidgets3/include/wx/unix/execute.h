/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/execute.h
// Purpose:     private details of wxExecute() implementation
// Author:      Vadim Zeitlin
// Id:          $Id: execute.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1998 Robert Roebling, Julian Smart, Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_EXECUTE_H
#define _WX_UNIX_EXECUTE_H

#include "wx/unix/pipe.h"

class WXDLLIMPEXP_FWD_BASE wxProcess;
class wxStreamTempInputBuffer;

struct wxEndProcessData
{
    wxEndProcessData()
    {
        pid =
        tag =
        exitcode = -1;
        process = NULL;
        async = false;
    }

    int pid;                // pid of the process
    int tag;                // port dependent value
    wxProcess *process;     // if !NULL: notified on process termination
    int exitcode;           // the exit code
    bool async;             // if true, delete us on process termination
};

// struct in which information is passed from wxExecute() to wxAppTraits
// methods
struct wxExecuteData
{
    wxExecuteData()
    {
        flags =
        pid = 0;

        process = NULL;

#if wxUSE_STREAMS
        bufOut =
        bufErr = NULL;

        fdOut =
        fdErr = wxPipe::INVALID_FD;
#endif // wxUSE_STREAMS
    }

    // get the FD corresponding to the read end of the process end detection
    // pipe and close the write one
    int GetEndProcReadFD()
    {
        const int fd = pipeEndProcDetect.Detach(wxPipe::Read);
        pipeEndProcDetect.Close();
        return fd;
    }


    // wxExecute() flags
    int flags;

    // the pid of the child process
    int pid;

    // the associated process object or NULL
    wxProcess *process;

    // pipe used for end process detection
    wxPipe pipeEndProcDetect;

#if wxUSE_STREAMS
    // the input buffer bufOut is connected to stdout, this is why it is
    // called bufOut and not bufIn
    wxStreamTempInputBuffer *bufOut,
                            *bufErr;

    // the corresponding FDs, -1 if not redirected
    int fdOut,
        fdErr;
#endif // wxUSE_STREAMS
};

// this function is called when the process terminates from port specific
// callback function and is common to all ports (src/unix/utilsunx.cpp)
extern WXDLLIMPEXP_BASE void wxHandleProcessTermination(wxEndProcessData *proc_data);

#endif // _WX_UNIX_EXECUTE_H
