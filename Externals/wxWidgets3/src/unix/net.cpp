// -*- c++ -*- ///////////////////////////////////////////////////////////////
// Name:        src/unix/net.cpp
// Purpose:     Network related wxWindows classes and functions
// Author:      Karsten Ballüder
// Modified by:
// Created:     03.10.99
// Copyright:   (c) Karsten Ballüder
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/setup.h"

#if wxUSE_DIALUP_MANAGER

#ifndef  WX_PRECOMP
#   include "wx/defs.h"
#endif // !PCH

#include "wx/string.h"
#include "wx/event.h"
#include "wx/net.h"
#include "wx/timer.h"
#include "wx/filename.h"
#include "wx/utils.h"
#include "wx/log.h"
#include "wx/file.h"

#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#define __STRICT_ANSI__
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// ----------------------------------------------------------------------------
// A class which groups functions dealing with connecting to the network from a
// workstation using dial-up access to the net. There is at most one instance
// of this class in the program accessed via GetDialUpManager().
// ----------------------------------------------------------------------------

/* TODO
 *
 * 1. more configurability for Unix: i.e. how to initiate the connection, how
 *    to check for online status, &c.
 * 2. add a "long Dial(long connectionId = -1)" function which asks the user
 *    about which connection to dial (this may be done using native dialogs
 *    under NT, need generic dialogs for all others) and returns the identifier
 *    of the selected connection (it's opaque to the application) - it may be
 *    reused later to dial the same connection later (or use strings instead of
 *    longs may be?)
 * 3. add an async version of dialing functions which notify the caller about
 *    the progress (or may be even start another thread to monitor it)
 * 4. the static creation/accessor functions are not MT-safe - but is this
 *    really crucial? I think we may suppose they're always called from the
 *    main thread?
 */

class WXDLLEXPORT wxDialUpManagerImpl : public wxDialUpManager
{
public:
   wxDialUpManagerImpl()
      {
         m_IsOnline = -1; // unknown
         m_timer = NULL;
         m_CanUseIfconfig = -1; // unknown
         m_BeaconHost = WXDIALUP_MANAGER_DEFAULT_BEACONHOST;
         m_BeaconPort = 80;
      }

   /** Could the dialup manager be initialized correctly? If this function
       returns FALSE, no other functions will work neither, so it's a good idea
       to call this function and check its result before calling any other
       wxDialUpManager methods.
   */
   virtual bool IsOk() const
      { return true; }

   /** The simplest way to initiate a dial up: this function dials the given
       ISP (exact meaning of the parameter depends on the platform), returns
       TRUE on success or FALSE on failure and logs the appropriate error
       message in the latter case.
       @param nameOfISP optional parameter for dial program
       @param username unused
       @param password unused
   */
   virtual bool Dial(const wxString& nameOfISP,
                     const wxString& WXUNUSED(username),
                     const wxString& WXUNUSED(password));

   /// Hang up the currently active dial up connection.
   virtual bool HangUp();

   // returns TRUE if the computer is connected to the network: under Windows,
   // this just means that a RAS connection exists, under Unix we check that
   // the "well-known host" (as specified by SetWellKnownHost) is reachable
   virtual bool IsOnline() const
      {
         if( (! m_timer) // we are not polling, so test now:
             || m_IsOnline == -1
            )
            CheckStatus();
         return m_IsOnline != 0;
      }

   // sometimes the built-in logic for determining the online status may fail,
   // so, in general, the user should be allowed to override it. This function
   // allows to forcefully set the online status - whatever our internal
   // algorithm may think about it.
   virtual void SetOnlineStatus(bool isOnline = true)
      { m_IsOnline = isOnline; }

   // set misc wxDialUpManager options
   // --------------------------------

   // enable automatical checks for the connection status and sending of
   // wxEVT_DIALUP_CONNECTED/wxEVT_DIALUP_DISCONNECTED events. The interval
   // parameter is only for Unix where we do the check manually: under
   // Windows, the notification about the change of connection status is
   // instantenous.
   //
   // Returns FALSE if couldn't set up automatic check for online status.
   virtual bool EnableAutoCheckOnlineStatus(size_t nSeconds);

   // disable automatic check for connection status change - notice that the
   // wxEVT_DIALUP_XXX events won't be sent any more neither.
   virtual void DisableAutoCheckOnlineStatus();

   // under Unix, the value of well-known host is used to check whether we're
   // connected to the internet. It's unused under Windows, but this function
   // is always safe to call. The default value is www.yahoo.com.
   virtual void SetWellKnownHost(const wxString& hostname,
                                 int portno = 80);
   /** Sets the commands to start up the network and to hang up
       again. Used by the Unix implementations only.
   */
   virtual void SetConnectCommand(const wxString &command, const wxString &hupcmd)
      { m_ConnectCommand = command; m_HangUpCommand = hupcmd; }

private:
   /// -1: don't know, 0 = no, 1 = yes
   int m_IsOnline;

   ///  Can we use ifconfig to list active devices?
   int m_CanUseIfconfig;
   /// The path to ifconfig
   wxString m_IfconfigPath;

   /// beacon host:
   wxString m_BeaconHost;
   /// beacon host portnumber for connect:
   int m_BeaconPort;

   /// command to connect to network
   wxString m_ConnectCommand;
   /// command to hang up
   wxString m_HangUpCommand;
   /// name of ISP
   wxString m_ISPname;
   /// a timer for regular testing
   class AutoCheckTimer *m_timer;

   friend class AutoCheckTimer;
   /// determine status
   void CheckStatus(void) const;

   /// real status check
   void CheckStatusInternal(void);
};


class AutoCheckTimer : public wxTimer
{
public:
   AutoCheckTimer(wxDialUpManagerImpl *dupman)
      {
         m_dupman = dupman;
         m_started = false;
      }

   virtual bool Start( int millisecs = -1 )
      { m_started = true; return wxTimer::Start(millisecs, false); }

   virtual void Notify()
      { wxLogTrace("Checking dial up network status."); m_dupman->CheckStatus(); }

   virtual void Stop()
      { if ( m_started ) wxTimer::Stop(); }
public:
   bool m_started;
   wxDialUpManagerImpl *m_dupman;
};

bool
wxDialUpManagerImpl::Dial(const wxString &isp,
                          const wxString & WXUNUSED(username),
                          const wxString & WXUNUSED(password))
{
   if(m_IsOnline == 1)
      return false;
   m_IsOnline = -1;
   m_ISPname = isp;
   wxString cmd;
   if(m_ConnectCommand.Find("%s"))
      cmd.Printf(m_ConnectCommand,m_ISPname.c_str());
   else
      cmd = m_ConnectCommand;
   return wxExecute(cmd, /* sync */ TRUE) == 0;
}

bool
wxDialUpManagerImpl::HangUp(void)
{
   if(m_IsOnline == 0)
      return false;
   m_IsOnline = -1;
   wxString cmd;
   if(m_HangUpCommand.Find("%s"))
      cmd.Printf(m_HangUpCommand,m_ISPname.c_str());
   else
      cmd = m_HangUpCommand;
   return wxExecute(cmd, /* sync */ TRUE) == 0;
}


bool
wxDialUpManagerImpl::EnableAutoCheckOnlineStatus(size_t nSeconds)
{
   wxASSERT(m_timer == NULL);
   m_timer = new AutoCheckTimer(this);
   bool rc = m_timer->Start(nSeconds*1000);
   if(! rc)
   {
      wxDELETE(m_timer);
   }
   return rc;
}

void
wxDialUpManagerImpl::DisableAutoCheckOnlineStatus()
{
   wxASSERT(m_timer != NULL);
   m_timer->Stop();
   wxDELETE(m_timer);
}


void
wxDialUpManagerImpl::SetWellKnownHost(const wxString& hostname, int portno)
{
   /// does hostname contain a port number?
   wxString port = hostname.After(':');
   if(port.empty())
   {
      m_BeaconHost = hostname;
      m_BeaconPort = portno;
   }
   else
   {
      m_BeaconHost = hostname.Before(':');
      m_BeaconPort = atoi(port);
   }
}


void
wxDialUpManagerImpl::CheckStatus(void) const
{
   // This function calls the CheckStatusInternal() helper function
   // which is OS - specific and then sends the events.

   int oldIsOnline = m_IsOnline;
   ( /* non-const */ (wxDialUpManagerImpl *)this)->CheckStatusInternal();

   // now send the events as appropriate:
   if(m_IsOnline != oldIsOnline)
   {
      if(m_IsOnline)
         ; // send ev
      else
         ; // send ev
   }
}

/*
  We have three methods that we can use:

  1. test via /sbin/ifconfig and grep for "sl", "ppp", "pl"
     --> should be fast enough for regular polling
  2. test if we can reach the well known beacon host
     --> too slow for polling
  3. check /proc/net/dev on linux??
     This method should be preferred, if possible. Need to do more
     testing.

*/

void
wxDialUpManagerImpl::CheckStatusInternal(void)
{
   m_IsOnline = -1;

   // First time check for ifconfig location. We only use the variant
   // which does not take arguments, a la GNU.
   if(m_CanUseIfconfig == -1) // unknown
   {
      if(wxFileExists("/sbin/ifconfig"))
         m_IfconfigPath = "/sbin/ifconfig";
      else if(wxFileExists("/usr/sbin/ifconfig"))
         m_IfconfigPath = "/usr/sbin/ifconfig";
   }

   wxLogNull ln; // suppress all error messages
   // Let's try the ifconfig method first, should be fastest:
   if(m_CanUseIfconfig != 0) // unknown or yes
   {
      wxASSERT( !m_IfconfigPath.empty() );

      wxString tmpfile = wxFileName::CreateTempFileName("_wxdialuptest");
      wxString cmd = "/bin/sh -c \'";
      cmd << m_IfconfigPath << " >" << tmpfile <<  '\'';
      /* I tried to add an option to wxExecute() to not close stdout,
         so we could let ifconfig write directly to the tmpfile, but
         this does not work. That should be faster, as it doesn't call
         the shell first. I have no idea why. :-(  (KB) */
#if 0
      // temporarily redirect stdout/stderr:
      int
         new_stdout = dup(STDOUT_FILENO),
         new_stderr = dup(STDERR_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);

      int
         // new stdout:
         output_fd = open(tmpfile, O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR),
         // new stderr:
         null_fd = open("/dev/null", O_CREAT, S_IRUSR|S_IWUSR);
      // verify well behaved unix behaviour:
      wxASSERT(output_fd == STDOUT_FILENO);
      wxASSERT(null_fd == STDERR_FILENO);
      int rc = wxExecute(m_IfconfigPath,TRUE /* sync */,NULL ,wxEXECUTE_DONT_CLOSE_FDS);
      close(null_fd); close(output_fd);
      // restore old stdout, stderr:
      int test;
      test = dup(new_stdout); close(new_stdout); wxASSERT(test == STDOUT_FILENO);
      test = dup(new_stderr); close(new_stderr); wxASSERT(test == STDERR_FILENO);
      if(rc == 0)
#endif
      if(wxExecute(cmd,TRUE /* sync */) == 0)
      {
         m_CanUseIfconfig = 1;
         wxFile file;
         if( file.Open(tmpfile) )
         {
            char *output = new char [file.Length()+1];
            output[file.Length()] = '\0';
            if(file.Read(output,file.Length()) == file.Length())
            {
               if(strstr(output,"ppp")   // ppp
                  || strstr(output,"sl") // slip
                  || strstr(output,"pl") // plip
                  )
                  m_IsOnline = 1;
               else
                  m_IsOnline = 0;
            }
            file.Close();
            delete [] output;
         }
         // else m_IsOnline remains -1 as we don't know for sure
      }
      else // could not run ifconfig correctly
         m_CanUseIfconfig = 0; // don't try again
      (void) wxRemoveFile(tmpfile);
      if(m_IsOnline != -1) // we are done
         return;
   }

   // second method: try to connect to well known host:
   // This can be used under Win 9x, too!
   struct hostent     *hp;
   struct sockaddr_in  serv_addr;
   int sockfd;

   m_IsOnline = 0; // assume false
   if((hp = gethostbyname(m_BeaconHost)) == NULL)
      return; // no DNS no net

   serv_addr.sin_family = hp->h_addrtype;
   memcpy(&serv_addr.sin_addr,hp->h_addr, hp->h_length);
   serv_addr.sin_port = htons(m_BeaconPort);
   if( ( sockfd = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
   {
      //  sys_error("cannot create socket for gw");
      return;
   }
   if( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
   {
      //sys_error("cannot connect to server");
      return;
   }
   //connected!
   close(sockfd);
}


/* static */
wxDialUpManager *
wxDialUpManager::wxDialUpManager::Create(void)
{
   return new wxDialUpManagerImpl;
}

#endif // wxUSE_DIALUP_MANAGER
