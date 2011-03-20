/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private/fswatcher.h
// Purpose:     File system watcher impl classes
// Author:      Bartosz Bekier
// Created:     2009-05-26
// RCS-ID:      $Id: fswatcher.h 62678 2009-11-18 09:56:52Z VZ $
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef WX_MSW_PRIVATE_FSWATCHER_H_
#define WX_MSW_PRIVATE_FSWATCHER_H_

#include "wx/filename.h"
#include "wx/vector.h"
#include "wx/msw/private.h"

// ============================================================================
// wxFSWatcherEntry implementation & helper declarations
// ============================================================================

class wxFSWatcherImplMSW;

class wxFSWatchEntryMSW : public wxFSWatchInfo
{
public:
    enum
    {
        BUFFER_SIZE = 4096   // TODO parametrize
    };

    wxFSWatchEntryMSW(const wxFSWatchInfo& winfo) :
        wxFSWatchInfo(winfo)
    {
        // get handle for this path
        m_handle = OpenDir(m_path);
        m_overlapped = (OVERLAPPED*)calloc(1, sizeof(OVERLAPPED));
        wxZeroMemory(m_buffer);
    }

    virtual ~wxFSWatchEntryMSW()
    {
        wxLogTrace(wxTRACE_FSWATCHER, "Deleting entry '%s'", m_path);

        if (m_handle != INVALID_HANDLE_VALUE)
        {
            if (!CloseHandle(m_handle))
            {
                wxLogSysError(_("Unable to close the handle for '%s'"),
                                m_path);
            }
        }
        delete m_overlapped;
    }

    bool IsOk() const
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

    HANDLE GetHandle() const
    {
        return m_handle;
    }

    void* GetBuffer()
    {
        return m_buffer;
    }

    OVERLAPPED* GetOverlapped() const
    {
        return m_overlapped;
    }

private:
    // opens dir with all flags, attributes etc. necessary to be later
    // asynchronous watched with ReadDirectoryChangesW
    static HANDLE OpenDir(const wxString& path)
    {
        HANDLE handle = CreateFile(path.t_str(),
                                   FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ |
                                   FILE_SHARE_WRITE |
                                   FILE_SHARE_DELETE,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS |
                                   FILE_FLAG_OVERLAPPED,
                                   NULL);
        if (handle == INVALID_HANDLE_VALUE)
        {
            wxLogSysError(_("Failed to open directory \"%s\" for monitoring."),
                            path);
        }

        return handle;
    }

    HANDLE m_handle;             // handle to opened directory
    char m_buffer[BUFFER_SIZE];  // buffer for fs events
    OVERLAPPED* m_overlapped;

    wxDECLARE_NO_COPY_CLASS(wxFSWatchEntryMSW);
};


// ============================================================================
// wxFSWatcherImplMSW helper classes implementations
// ============================================================================

class wxIOCPService
{
public:
    wxIOCPService() :
        m_iocp(INVALID_HANDLE_VALUE)
    {
        Init();
    }

    ~wxIOCPService()
    {
        if (m_iocp != INVALID_HANDLE_VALUE)
        {
            if (!CloseHandle(m_iocp))
            {
                wxLogSysError(_("Unable to close I/O completion port handle"));
            }
        }
        m_watches.clear();
    }

    // associates a wxFSWatchEntryMSW with completion port
    bool Add(wxSharedPtr<wxFSWatchEntryMSW> watch)
    {
        wxCHECK_MSG( m_iocp != INVALID_HANDLE_VALUE, false, "IOCP not init" );
        wxCHECK_MSG( watch->IsOk(), false, "Invalid watch" );

        // associate with IOCP
        HANDLE ret = CreateIoCompletionPort(watch->GetHandle(), m_iocp,
                                            (ULONG_PTR)watch.get(), 0);
        if (ret == NULL)
        {
            wxLogSysError(_("Unable to associate handle with "
                            "I/O completion port"));
            return false;
        }
        else if (ret != m_iocp)
        {
            wxFAIL_MSG(_("Unexpectedly new I/O completion port was created"));
            return false;
        }

        // add to watch map
        wxFSWatchEntries::value_type val(watch->GetPath(), watch);
        return m_watches.insert(val).second;
    }

    // post completion packet
    bool PostEmptyStatus()
    {
        wxCHECK_MSG( m_iocp != INVALID_HANDLE_VALUE, false, "IOCP not init" );

        int ret = PostQueuedCompletionStatus(m_iocp, 0, 0, NULL);
        if (!ret)
        {
            wxLogSysError(_("Unable to post completion status"));
        }

        return ret != 0;
    }

    // Wait for completion status to arrive.
    // This function can block forever in it's wait for completion status.
    // Use PostEmptyStatus() to wake it up (and end the worker thread)
    bool GetStatus(unsigned long* count, wxFSWatchEntryMSW** watch,
                   OVERLAPPED** overlapped)
    {
        wxCHECK_MSG( m_iocp != INVALID_HANDLE_VALUE, false, "IOCP not init" );
        wxCHECK_MSG( count != NULL, false, "Null out parameter 'count'");
        wxCHECK_MSG( watch != NULL, false, "Null out parameter 'watch'");
        wxCHECK_MSG( overlapped != NULL, false,
                     "Null out parameter 'overlapped'");

        int ret = GetQueuedCompletionStatus(m_iocp, count, (ULONG_PTR *)watch,
                                            overlapped, INFINITE);
        if (!ret)
        {
            wxLogSysError(_("Unable to dequeue completion packet"));
        }
        return ret != 0;
    }

protected:
    bool Init()
    {
        m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (m_iocp == NULL)
        {
            wxLogSysError(_("Unable to create I/O completion port"));
        }
        return m_iocp != NULL;
    }

    HANDLE m_iocp;
    wxFSWatchEntries m_watches;
};


class wxIOCPThread : public wxThread
{
public:
    wxIOCPThread(wxFSWatcherImplMSW* service, wxIOCPService* iocp);

    // finishes this thread
    bool Finish();

protected:
    // structure to hold information needed to process one native event
    // this is just a dummy holder, so it doesn't take ownership of it's data
    struct wxEventProcessingData
    {
        wxEventProcessingData(const FILE_NOTIFY_INFORMATION* ne,
                              const wxFSWatchEntryMSW* watch) :
            nativeEvent(ne), watch(watch)
        {}

        const FILE_NOTIFY_INFORMATION* nativeEvent;
        const wxFSWatchEntryMSW* watch;
    };

    virtual ExitCode Entry();

    // wait for events to occur, read them and send to interested parties
    // returns false it empty status was read, which means we whould exit
    //         true otherwise
    bool ReadEvents();

    void ProcessNativeEvents(wxVector<wxEventProcessingData>& events);

    void SendEvent(wxFileSystemWatcherEvent& evt);

    static int Native2WatcherFlags(int flags);

    static wxString FileNotifyInformationToString(
                                            const FILE_NOTIFY_INFORMATION& e);

    static wxFileName GetEventPath(const wxFSWatchEntryMSW& watch,
                                   const FILE_NOTIFY_INFORMATION& e);

    wxFSWatcherImplMSW* m_service;
    wxIOCPService* m_iocp;
};

#endif /* WX_MSW_PRIVATE_FSWATCHER_H_ */
