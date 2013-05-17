/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/utilsunx.cpp
// Purpose:     generic Unix implementation of many wx functions (for wxBase)
// Author:      Vadim Zeitlin
// Id:          $Id: utilsunx.cpp 67411 2011-04-06 17:04:12Z PC $
// Copyright:   (c) 1998 Robert Roebling, Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/utils.h"

#define USE_PUTENV (!defined(HAVE_SETENV) && defined(HAVE_PUTENV))

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/wxcrtvararg.h"
    #if USE_PUTENV
        #include "wx/module.h"
        #include "wx/hashmap.h"
    #endif
#endif

#include "wx/apptrait.h"

#include "wx/process.h"
#include "wx/thread.h"

#include "wx/cmdline.h"

#include "wx/wfstream.h"

#include "wx/private/selectdispatcher.h"
#include "wx/private/fdiodispatcher.h"
#include "wx/unix/execute.h"
#include "wx/unix/private.h"

#ifdef wxHAS_GENERIC_PROCESS_CALLBACK
#include "wx/private/fdiodispatcher.h"
#endif

#include <pwd.h>
#include <sys/wait.h>       // waitpid()

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

#define HAS_PIPE_STREAMS (wxUSE_STREAMS && wxUSE_FILE)

#if HAS_PIPE_STREAMS

// define this to let wxexec.cpp know that we know what we're doing
#define _WX_USED_BY_WXEXECUTE_
#include "../common/execcmn.cpp"

#endif // HAS_PIPE_STREAMS

#if defined(__MWERKS__) && defined(__MACH__)
    #ifndef WXWIN_OS_DESCRIPTION
        #define WXWIN_OS_DESCRIPTION "MacOS X"
    #endif
    #ifndef HAVE_NANOSLEEP
        #define HAVE_NANOSLEEP
    #endif
    #ifndef HAVE_UNAME
        #define HAVE_UNAME
    #endif

    // our configure test believes we can use sigaction() if the function is
    // available but Metrowekrs with MSL run-time does have the function but
    // doesn't have sigaction struct so finally we can't use it...
    #ifdef __MSL__
        #undef wxUSE_ON_FATAL_EXCEPTION
        #define wxUSE_ON_FATAL_EXCEPTION 0
    #endif
#endif

// not only the statfs syscall is called differently depending on platform, but
// one of its incarnations, statvfs(), takes different arguments under
// different platforms and even different versions of the same system (Solaris
// 7 and 8): if you want to test for this, don't forget that the problems only
// appear if the large files support is enabled
#ifdef HAVE_STATFS
    #ifdef __BSD__
        #include <sys/param.h>
        #include <sys/mount.h>
    #else // !__BSD__
        #include <sys/vfs.h>
    #endif // __BSD__/!__BSD__

    #define wxStatfs statfs

    #ifndef HAVE_STATFS_DECL
        // some systems lack statfs() prototype in the system headers (AIX 4)
        extern "C" int statfs(const char *path, struct statfs *buf);
    #endif
#endif // HAVE_STATFS

#ifdef HAVE_STATVFS
    #include <sys/statvfs.h>

    #define wxStatfs statvfs
#endif // HAVE_STATVFS

#if defined(HAVE_STATFS) || defined(HAVE_STATVFS)
    // WX_STATFS_T is detected by configure
    #define wxStatfs_t WX_STATFS_T
#endif

// SGI signal.h defines signal handler arguments differently depending on
// whether _LANGUAGE_C_PLUS_PLUS is set or not - do set it
#if defined(__SGI__) && !defined(_LANGUAGE_C_PLUS_PLUS)
    #define _LANGUAGE_C_PLUS_PLUS 1
#endif // SGI hack

#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>          // for O_WRONLY and friends
#include <time.h>           // nanosleep() and/or usleep()
#include <ctype.h>          // isspace()
#include <sys/time.h>       // needed for FD_SETSIZE

#ifdef HAVE_UNAME
    #include <sys/utsname.h> // for uname()
#endif // HAVE_UNAME

// Used by wxGetFreeMemory().
#ifdef __SGI__
    #include <sys/sysmp.h>
    #include <sys/sysinfo.h>   // for SAGET and MINFO structures
#endif

// ----------------------------------------------------------------------------
// conditional compilation
// ----------------------------------------------------------------------------

// many versions of Unices have this function, but it is not defined in system
// headers - please add your system here if it is the case for your OS.
// SunOS < 5.6 (i.e. Solaris < 2.6) and DG-UX are like this.
#if !defined(HAVE_USLEEP) && \
    ((defined(__SUN__) && !defined(__SunOs_5_6) && \
                         !defined(__SunOs_5_7) && !defined(__SUNPRO_CC)) || \
     defined(__osf__) || defined(__EMX__))
    extern "C"
    {
        #ifdef __EMX__
            /* I copied this from the XFree86 diffs. AV. */
            #define INCL_DOSPROCESS
            #include <os2.h>
            inline void usleep(unsigned long delay)
            {
                DosSleep(delay ? (delay/1000l) : 1l);
            }
        #else // Unix
            int usleep(unsigned int usec);
        #endif // __EMX__/Unix
    };

    #define HAVE_USLEEP 1
#endif // Unices without usleep()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// sleeping
// ----------------------------------------------------------------------------

void wxSleep(int nSecs)
{
    sleep(nSecs);
}

void wxMicroSleep(unsigned long microseconds)
{
#if defined(HAVE_NANOSLEEP)
    timespec tmReq;
    tmReq.tv_sec = (time_t)(microseconds / 1000000);
    tmReq.tv_nsec = (microseconds % 1000000) * 1000;

    // we're not interested in remaining time nor in return value
    (void)nanosleep(&tmReq, NULL);
#elif defined(HAVE_USLEEP)
    // uncomment this if you feel brave or if you are sure that your version
    // of Solaris has a safe usleep() function but please notice that usleep()
    // is known to lead to crashes in MT programs in Solaris 2.[67] and is not
    // documented as MT-Safe
    #if defined(__SUN__) && wxUSE_THREADS
        #error "usleep() cannot be used in MT programs under Solaris."
    #endif // Sun

    usleep(microseconds);
#elif defined(HAVE_SLEEP)
    // under BeOS sleep() takes seconds (what about other platforms, if any?)
    sleep(microseconds * 1000000);
#else // !sleep function
    #error "usleep() or nanosleep() function required for wxMicroSleep"
#endif // sleep function
}

void wxMilliSleep(unsigned long milliseconds)
{
    wxMicroSleep(milliseconds*1000);
}

// ----------------------------------------------------------------------------
// process management
// ----------------------------------------------------------------------------

int wxKill(long pid, wxSignal sig, wxKillError *rc, int flags)
{
    int err = kill((pid_t) (flags & wxKILL_CHILDREN) ? -pid : pid, (int)sig);
    if ( rc )
    {
        switch ( err ? errno : 0 )
        {
            case 0:
                *rc = wxKILL_OK;
                break;

            case EINVAL:
                *rc = wxKILL_BAD_SIGNAL;
                break;

            case EPERM:
                *rc = wxKILL_ACCESS_DENIED;
                break;

            case ESRCH:
                *rc = wxKILL_NO_PROCESS;
                break;

            default:
                // this goes against Unix98 docs so log it
                wxLogDebug(wxT("unexpected kill(2) return value %d"), err);

                // something else...
                *rc = wxKILL_ERROR;
        }
    }

    return err;
}

// Shutdown or reboot the PC
bool wxShutdown(int flags)
{
    flags &= ~wxSHUTDOWN_FORCE;

    wxChar level;
    switch ( flags )
    {
        case wxSHUTDOWN_POWEROFF:
            level = wxT('0');
            break;

        case wxSHUTDOWN_REBOOT:
            level = wxT('6');
            break;

        case wxSHUTDOWN_LOGOFF:
            // TODO: use dcop to log off?
            return false;

        default:
            wxFAIL_MSG( wxT("unknown wxShutdown() flag") );
            return false;
    }

    return system(wxString::Format("init %c", level).mb_str()) == 0;
}

// ----------------------------------------------------------------------------
// wxStream classes to support IO redirection in wxExecute
// ----------------------------------------------------------------------------

#if HAS_PIPE_STREAMS

bool wxPipeInputStream::CanRead() const
{
    if ( m_lasterror == wxSTREAM_EOF )
        return false;

    // check if there is any input available
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    const int fd = m_file->fd();

    fd_set readfds;

    wxFD_ZERO(&readfds);
    wxFD_SET(fd, &readfds);

    switch ( select(fd + 1, &readfds, NULL, NULL, &tv) )
    {
        case -1:
            wxLogSysError(_("Impossible to get child process input"));
            // fall through

        case 0:
            return false;

        default:
            wxFAIL_MSG(wxT("unexpected select() return value"));
            // still fall through

        case 1:
            // input available -- or maybe not, as select() returns 1 when a
            // read() will complete without delay, but it could still not read
            // anything
            return !Eof();
    }
}

size_t wxPipeOutputStream::OnSysWrite(const void *buffer, size_t size)
{
    // We need to suppress error logging here, because on writing to a pipe
    // which is full, wxFile::Write reports a system error. However, this is
    // not an extraordinary situation, and it should not be reported to the
    // user (but if really needed, the program can recognize it by checking
    // whether LastRead() == 0.) Other errors will be reported below.
    size_t ret;
    {
        wxLogNull logNo;
        ret = m_file->Write(buffer, size);
    }

    switch ( m_file->GetLastError() )
    {
       // pipe is full
#ifdef EAGAIN
       case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
       case EWOULDBLOCK:
#endif
           // do not treat it as an error
           m_file->ClearLastError();
           // fall through

       // no error
       case 0:
           break;

       // some real error
       default:
           wxLogSysError(_("Can't write to child process's stdin"));
           m_lasterror = wxSTREAM_WRITE_ERROR;
    }

    return ret;
}

#endif // HAS_PIPE_STREAMS

// ----------------------------------------------------------------------------
// wxShell
// ----------------------------------------------------------------------------

static wxString wxMakeShellCommand(const wxString& command)
{
    wxString cmd;
    if ( !command )
    {
        // just an interactive shell
        cmd = wxT("xterm");
    }
    else
    {
        // execute command in a shell
        cmd << wxT("/bin/sh -c '") << command << wxT('\'');
    }

    return cmd;
}

bool wxShell(const wxString& command)
{
    return wxExecute(wxMakeShellCommand(command), wxEXEC_SYNC) == 0;
}

bool wxShell(const wxString& command, wxArrayString& output)
{
    wxCHECK_MSG( !command.empty(), false, wxT("can't exec shell non interactively") );

    return wxExecute(wxMakeShellCommand(command), output);
}

namespace
{

// helper class for storing arguments as char** array suitable for passing to
// execvp(), whatever form they were passed to us
class ArgsArray
{
public:
    ArgsArray(const wxArrayString& args)
    {
        Init(args.size());

        for ( int i = 0; i < m_argc; i++ )
        {
            m_argv[i] = wxStrdup(args[i]);
        }
    }

#if wxUSE_UNICODE
    ArgsArray(wchar_t **wargv)
    {
        int argc = 0;
        while ( wargv[argc] )
            argc++;

        Init(argc);

        for ( int i = 0; i < m_argc; i++ )
        {
            m_argv[i] = wxSafeConvertWX2MB(wargv[i]).release();
        }
    }
#endif // wxUSE_UNICODE

    ~ArgsArray()
    {
        for ( int i = 0; i < m_argc; i++ )
        {
            free(m_argv[i]);
        }

        delete [] m_argv;
    }

    operator char**() const { return m_argv; }

private:
    void Init(int argc)
    {
        m_argc = argc;
        m_argv = new char *[m_argc + 1];
        m_argv[m_argc] = NULL;
    }

    int m_argc;
    char **m_argv;

    wxDECLARE_NO_COPY_CLASS(ArgsArray);
};

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxExecute implementations
// ----------------------------------------------------------------------------

#if defined(__DARWIN__)
bool wxMacLaunch(char **argv);
#endif

long wxExecute(const wxString& command, int flags, wxProcess *process,
        const wxExecuteEnv *env)
{
    ArgsArray argv(wxCmdLineParser::ConvertStringToArgs(command,
                                                        wxCMD_LINE_SPLIT_UNIX));

    return wxExecute(argv, flags, process, env);
}

#if wxUSE_UNICODE

long wxExecute(wchar_t **wargv, int flags, wxProcess *process,
        const wxExecuteEnv *env)
{
    ArgsArray argv(wargv);

    return wxExecute(argv, flags, process, env);
}

#endif // wxUSE_UNICODE

// wxExecute: the real worker function
long wxExecute(char **argv, int flags, wxProcess *process,
        const wxExecuteEnv *env)
{
    // for the sync execution, we return -1 to indicate failure, but for async
    // case we return 0 which is never a valid PID
    //
    // we define this as a macro, not a variable, to avoid compiler warnings
    // about "ERROR_RETURN_CODE value may be clobbered by fork()"
    #define ERROR_RETURN_CODE ((flags & wxEXEC_SYNC) ? -1 : 0)

    wxCHECK_MSG( *argv, ERROR_RETURN_CODE, wxT("can't exec empty command") );

#if wxUSE_THREADS
    // fork() doesn't mix well with POSIX threads: on many systems the program
    // deadlocks or crashes for some reason. Probably our code is buggy and
    // doesn't do something which must be done to allow this to work, but I
    // don't know what yet, so for now just warn the user (this is the least we
    // can do) about it
    wxASSERT_MSG( wxThread::IsMain(),
                    wxT("wxExecute() can be called only from the main thread") );
#endif // wxUSE_THREADS

#if defined(__WXCOCOA__) || ( defined(__WXOSX_MAC__) && wxOSX_USE_COCOA_OR_CARBON )
    // wxMacLaunch() only executes app bundles and only does it asynchronously.
    // It returns false if the target is not an app bundle, thus falling
    // through to the regular code for non app bundles.
    if ( !(flags & wxEXEC_SYNC) && wxMacLaunch(argv) )
    {
        // we don't have any PID to return so just make up something non null
        return -1;
    }
#endif // __DARWIN__


    // this struct contains all information which we use for housekeeping
    wxExecuteData execData;
    execData.flags = flags;
    execData.process = process;

    // create pipes
    if ( !execData.pipeEndProcDetect.Create() )
    {
        wxLogError( _("Failed to execute '%s'\n"), *argv );

        return ERROR_RETURN_CODE;
    }

    // pipes for inter process communication
    wxPipe pipeIn,      // stdin
           pipeOut,     // stdout
           pipeErr;     // stderr

    if ( process && process->IsRedirected() )
    {
        if ( !pipeIn.Create() || !pipeOut.Create() || !pipeErr.Create() )
        {
            wxLogError( _("Failed to execute '%s'\n"), *argv );

            return ERROR_RETURN_CODE;
        }
    }

    // fork the process
    //
    // NB: do *not* use vfork() here, it completely breaks this code for some
    //     reason under Solaris (and maybe others, although not under Linux)
    //     But on OpenVMS we do not have fork so we have to use vfork and
    //     cross our fingers that it works.
#ifdef __VMS
   pid_t pid = vfork();
#else
   pid_t pid = fork();
#endif
   if ( pid == -1 )     // error?
    {
        wxLogSysError( _("Fork failed") );

        return ERROR_RETURN_CODE;
    }
    else if ( pid == 0 )  // we're in child
    {
        // NB: we used to close all the unused descriptors of the child here
        //     but this broke some programs which relied on e.g. FD 1 being
        //     always opened so don't do it any more, after all there doesn't
        //     seem to be any real problem with keeping them opened

#if !defined(__VMS) && !defined(__EMX__)
        if ( flags & wxEXEC_MAKE_GROUP_LEADER )
        {
            // Set process group to child process' pid.  Then killing -pid
            // of the parent will kill the process and all of its children.
            setsid();
        }
#endif // !__VMS

        // redirect stdin, stdout and stderr
        if ( pipeIn.IsOk() )
        {
            if ( dup2(pipeIn[wxPipe::Read], STDIN_FILENO) == -1 ||
                 dup2(pipeOut[wxPipe::Write], STDOUT_FILENO) == -1 ||
                 dup2(pipeErr[wxPipe::Write], STDERR_FILENO) == -1 )
            {
                wxLogSysError(_("Failed to redirect child process input/output"));
            }

            pipeIn.Close();
            pipeOut.Close();
            pipeErr.Close();
        }

        // Close all (presumably accidentally) inherited file descriptors to
        // avoid descriptor leaks. This means that we don't allow inheriting
        // them purposefully but this seems like a lesser evil in wx code.
        // Ideally we'd provide some flag to indicate that none (or some?) of
        // the descriptors do not need to be closed but for now this is better
        // than never closing them at all as wx code never used FD_CLOEXEC.

        // Note that while the reading side of the end process detection pipe
        // can be safely closed, we should keep the write one opened, it will
        // be only closed when the process terminates resulting in a read
        // notification to the parent
        const int fdEndProc = execData.pipeEndProcDetect.Detach(wxPipe::Write);
        execData.pipeEndProcDetect.Close();

        // TODO: Iterating up to FD_SETSIZE is both inefficient (because it may
        //       be quite big) and incorrect (because in principle we could
        //       have more opened descriptions than this number). Unfortunately
        //       there is no good portable solution for closing all descriptors
        //       above a certain threshold but non-portable solutions exist for
        //       most platforms, see [http://stackoverflow.com/questions/899038/
        //          getting-the-highest-allocated-file-descriptor]
        for ( int fd = 0; fd < (int)FD_SETSIZE; ++fd )
        {
            if ( fd != STDIN_FILENO  &&
                 fd != STDOUT_FILENO &&
                 fd != STDERR_FILENO &&
                 fd != fdEndProc )
            {
                close(fd);
            }
        }


        // Process additional options if we have any
        if ( env )
        {
            // Change working directory if it is specified
            if ( !env->cwd.empty() )
                wxSetWorkingDirectory(env->cwd);

            // Change environment if needed.
            //
            // NB: We can't use execve() currently because we allow using
            //     non full paths to wxExecute(), i.e. we want to search for
            //     the program in PATH. However it just might be simpler/better
            //     to do the search manually and use execve() envp parameter to
            //     set up the environment of the child process explicitly
            //     instead of doing what we do below.
            if ( !env->env.empty() )
            {
                wxEnvVariableHashMap oldenv;
                wxGetEnvMap(&oldenv);

                // Remove unwanted variables
                wxEnvVariableHashMap::const_iterator it;
                for ( it = oldenv.begin(); it != oldenv.end(); ++it )
                {
                    if ( env->env.find(it->first) == env->env.end() )
                        wxUnsetEnv(it->first);
                }

                // And add the new ones (possibly replacing the old values)
                for ( it = env->env.begin(); it != env->env.end(); ++it )
                    wxSetEnv(it->first, it->second);
            }
        }

        execvp(*argv, argv);

        fprintf(stderr, "execvp(");
        for ( char **a = argv; *a; a++ )
            fprintf(stderr, "%s%s", a == argv ? "" : ", ", *a);
        fprintf(stderr, ") failed with error %d!\n", errno);

        // there is no return after successful exec()
        _exit(-1);

        // some compilers complain about missing return - of course, they
        // should know that exit() doesn't return but what else can we do if
        // they don't?
        //
        // and, sure enough, other compilers complain about unreachable code
        // after exit() call, so we can just always have return here...
#if defined(__VMS) || defined(__INTEL_COMPILER)
        return 0;
#endif
    }
    else // we're in parent
    {
        // save it for WaitForChild() use
        execData.pid = pid;
        if (execData.process)
            execData.process->SetPid(pid);  // and also in the wxProcess

        // prepare for IO redirection

#if HAS_PIPE_STREAMS
        // the input buffer bufOut is connected to stdout, this is why it is
        // called bufOut and not bufIn
        wxStreamTempInputBuffer bufOut,
                                bufErr;

        if ( process && process->IsRedirected() )
        {
            // Avoid deadlocks which could result from trying to write to the
            // child input pipe end while the child itself is writing to its
            // output end and waiting for us to read from it.
            if ( !pipeIn.MakeNonBlocking(wxPipe::Write) )
            {
                // This message is not terrible useful for the user but what
                // else can we do? Also, should we fail here or take the risk
                // to continue and deadlock? Currently we choose the latter but
                // it might not be the best idea.
                wxLogSysError(_("Failed to set up non-blocking pipe, "
                                "the program might hang."));
#if wxUSE_LOG
                wxLog::FlushActive();
#endif
            }

            wxOutputStream *inStream =
                new wxPipeOutputStream(pipeIn.Detach(wxPipe::Write));

            const int fdOut = pipeOut.Detach(wxPipe::Read);
            wxPipeInputStream *outStream = new wxPipeInputStream(fdOut);

            const int fdErr = pipeErr.Detach(wxPipe::Read);
            wxPipeInputStream *errStream = new wxPipeInputStream(fdErr);

            process->SetPipeStreams(outStream, inStream, errStream);

            bufOut.Init(outStream);
            bufErr.Init(errStream);

            execData.bufOut = &bufOut;
            execData.bufErr = &bufErr;

            execData.fdOut = fdOut;
            execData.fdErr = fdErr;
        }
#endif // HAS_PIPE_STREAMS

        if ( pipeIn.IsOk() )
        {
            pipeIn.Close();
            pipeOut.Close();
            pipeErr.Close();
        }

        // we want this function to work even if there is no wxApp so ensure
        // that we have a valid traits pointer
        wxConsoleAppTraits traitsConsole;
        wxAppTraits *traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
        if ( !traits )
            traits = &traitsConsole;

        return traits->WaitForChild(execData);
    }

#if !defined(__VMS) && !defined(__INTEL_COMPILER)
    return ERROR_RETURN_CODE;
#endif
}

#undef ERROR_RETURN_CODE

// ----------------------------------------------------------------------------
// file and directory functions
// ----------------------------------------------------------------------------

const wxChar* wxGetHomeDir( wxString *home  )
{
    *home = wxGetUserHome();
    wxString tmp;
    if ( home->empty() )
        *home = wxT("/");
#ifdef __VMS
    tmp = *home;
    if ( tmp.Last() != wxT(']'))
        if ( tmp.Last() != wxT('/')) *home << wxT('/');
#endif
    return home->c_str();
}

wxString wxGetUserHome( const wxString &user )
{
    struct passwd *who = (struct passwd *) NULL;

    if ( !user )
    {
        wxChar *ptr;

        if ((ptr = wxGetenv(wxT("HOME"))) != NULL)
        {
            return ptr;
        }

        if ((ptr = wxGetenv(wxT("USER"))) != NULL ||
             (ptr = wxGetenv(wxT("LOGNAME"))) != NULL)
        {
            who = getpwnam(wxSafeConvertWX2MB(ptr));
        }

        // make sure the user exists!
        if ( !who )
        {
            who = getpwuid(getuid());
        }
    }
    else
    {
      who = getpwnam (user.mb_str());
    }

    return wxSafeConvertMB2WX(who ? who->pw_dir : 0);
}

// ----------------------------------------------------------------------------
// network and user id routines
// ----------------------------------------------------------------------------

// private utility function which returns output of the given command, removing
// the trailing newline
static wxString wxGetCommandOutput(const wxString &cmd)
{
    FILE *f = popen(cmd.ToAscii(), "r");
    if ( !f )
    {
        wxLogSysError(wxT("Executing \"%s\" failed"), cmd.c_str());
        return wxEmptyString;
    }

    wxString s;
    char buf[256];
    while ( !feof(f) )
    {
        if ( !fgets(buf, sizeof(buf), f) )
            break;

        s += wxString::FromAscii(buf);
    }

    pclose(f);

    if ( !s.empty() && s.Last() == wxT('\n') )
        s.RemoveLast();

    return s;
}

// retrieve either the hostname or FQDN depending on platform (caller must
// check whether it's one or the other, this is why this function is for
// private use only)
static bool wxGetHostNameInternal(wxChar *buf, int sz)
{
    wxCHECK_MSG( buf, false, wxT("NULL pointer in wxGetHostNameInternal") );

    *buf = wxT('\0');

    // we're using uname() which is POSIX instead of less standard sysinfo()
#if defined(HAVE_UNAME)
    struct utsname uts;
    bool ok = uname(&uts) != -1;
    if ( ok )
    {
        wxStrlcpy(buf, wxSafeConvertMB2WX(uts.nodename), sz);
    }
#elif defined(HAVE_GETHOSTNAME)
    char cbuf[sz];
    bool ok = gethostname(cbuf, sz) != -1;
    if ( ok )
    {
        wxStrlcpy(buf, wxSafeConvertMB2WX(cbuf), sz);
    }
#else // no uname, no gethostname
    wxFAIL_MSG(wxT("don't know host name for this machine"));

    bool ok = false;
#endif // uname/gethostname

    if ( !ok )
    {
        wxLogSysError(_("Cannot get the hostname"));
    }

    return ok;
}

bool wxGetHostName(wxChar *buf, int sz)
{
    bool ok = wxGetHostNameInternal(buf, sz);

    if ( ok )
    {
        // BSD systems return the FQDN, we only want the hostname, so extract
        // it (we consider that dots are domain separators)
        wxChar *dot = wxStrchr(buf, wxT('.'));
        if ( dot )
        {
            // nuke it
            *dot = wxT('\0');
        }
    }

    return ok;
}

bool wxGetFullHostName(wxChar *buf, int sz)
{
    bool ok = wxGetHostNameInternal(buf, sz);

    if ( ok )
    {
        if ( !wxStrchr(buf, wxT('.')) )
        {
            struct hostent *host = gethostbyname(wxSafeConvertWX2MB(buf));
            if ( !host )
            {
                wxLogSysError(_("Cannot get the official hostname"));

                ok = false;
            }
            else
            {
                // the canonical name
                wxStrlcpy(buf, wxSafeConvertMB2WX(host->h_name), sz);
            }
        }
        //else: it's already a FQDN (BSD behaves this way)
    }

    return ok;
}

bool wxGetUserId(wxChar *buf, int sz)
{
    struct passwd *who;

    *buf = wxT('\0');
    if ((who = getpwuid(getuid ())) != NULL)
    {
        wxStrlcpy (buf, wxSafeConvertMB2WX(who->pw_name), sz);
        return true;
    }

    return false;
}

bool wxGetUserName(wxChar *buf, int sz)
{
#ifdef HAVE_PW_GECOS
    struct passwd *who;

    *buf = wxT('\0');
    if ((who = getpwuid (getuid ())) != NULL)
    {
       char *comma = strchr(who->pw_gecos, ',');
       if (comma)
           *comma = '\0'; // cut off non-name comment fields
       wxStrlcpy(buf, wxSafeConvertMB2WX(who->pw_gecos), sz);
       return true;
    }

    return false;
#else // !HAVE_PW_GECOS
    return wxGetUserId(buf, sz);
#endif // HAVE_PW_GECOS/!HAVE_PW_GECOS
}

bool wxIsPlatform64Bit()
{
    const wxString machine = wxGetCommandOutput(wxT("uname -m"));

    // the test for "64" is obviously not 100% reliable but seems to work fine
    // in practice
    return machine.Contains(wxT("64")) ||
                machine.Contains(wxT("alpha"));
}

#ifdef __LINUX__
wxLinuxDistributionInfo wxGetLinuxDistributionInfo()
{
    const wxString id = wxGetCommandOutput(wxT("lsb_release --id"));
    const wxString desc = wxGetCommandOutput(wxT("lsb_release --description"));
    const wxString rel = wxGetCommandOutput(wxT("lsb_release --release"));
    const wxString codename = wxGetCommandOutput(wxT("lsb_release --codename"));

    wxLinuxDistributionInfo ret;

    id.StartsWith("Distributor ID:\t", &ret.Id);
    desc.StartsWith("Description:\t", &ret.Description);
    rel.StartsWith("Release:\t", &ret.Release);
    codename.StartsWith("Codename:\t", &ret.CodeName);

    return ret;
}
#endif

// these functions are in src/osx/utilsexc_base.cpp for wxMac
#ifndef __WXMAC__

wxOperatingSystemId wxGetOsVersion(int *verMaj, int *verMin)
{
    // get OS version
    int major, minor;
    wxString release = wxGetCommandOutput(wxT("uname -r"));
    if ( release.empty() ||
         wxSscanf(release.c_str(), wxT("%d.%d"), &major, &minor) != 2 )
    {
        // failed to get version string or unrecognized format
        major =
        minor = -1;
    }

    if ( verMaj )
        *verMaj = major;
    if ( verMin )
        *verMin = minor;

    // try to understand which OS are we running
    wxString kernel = wxGetCommandOutput(wxT("uname -s"));
    if ( kernel.empty() )
        kernel = wxGetCommandOutput(wxT("uname -o"));

    if ( kernel.empty() )
        return wxOS_UNKNOWN;

    return wxPlatformInfo::GetOperatingSystemId(kernel);
}

wxString wxGetOsDescription()
{
    return wxGetCommandOutput(wxT("uname -s -r -m"));
}

#endif // !__WXMAC__

unsigned long wxGetProcessId()
{
    return (unsigned long)getpid();
}

wxMemorySize wxGetFreeMemory()
{
#if defined(__LINUX__)
    // get it from /proc/meminfo
    FILE *fp = fopen("/proc/meminfo", "r");
    if ( fp )
    {
        long memFree = -1;

        char buf[1024];
        if ( fgets(buf, WXSIZEOF(buf), fp) && fgets(buf, WXSIZEOF(buf), fp) )
        {
            // /proc/meminfo changed its format in kernel 2.6
            if ( wxPlatformInfo().CheckOSVersion(2, 6) )
            {
                unsigned long cached, buffers;
                sscanf(buf, "MemFree: %ld", &memFree);

                fgets(buf, WXSIZEOF(buf), fp);
                sscanf(buf, "Buffers: %lu", &buffers);

                fgets(buf, WXSIZEOF(buf), fp);
                sscanf(buf, "Cached: %lu", &cached);

                // add to "MemFree" also the "Buffers" and "Cached" values as
                // free(1) does as otherwise the value never makes sense: for
                // kernel 2.6 it's always almost 0
                memFree += buffers + cached;

                // values here are always expressed in kB and we want bytes
                memFree *= 1024;
            }
            else // Linux 2.4 (or < 2.6, anyhow)
            {
                long memTotal, memUsed;
                sscanf(buf, "Mem: %ld %ld %ld", &memTotal, &memUsed, &memFree);
            }
        }

        fclose(fp);

        return (wxMemorySize)memFree;
    }
#elif defined(__SGI__)
    struct rminfo realmem;
    if ( sysmp(MP_SAGET, MPSA_RMINFO, &realmem, sizeof realmem) == 0 )
        return ((wxMemorySize)realmem.physmem * sysconf(_SC_PAGESIZE));
#elif defined(_SC_AVPHYS_PAGES)
    return ((wxMemorySize)sysconf(_SC_AVPHYS_PAGES))*sysconf(_SC_PAGESIZE);
//#elif defined(__FREEBSD__) -- might use sysctl() to find it out, probably
#endif

    // can't find it out
    return -1;
}

bool wxGetDiskSpace(const wxString& path, wxDiskspaceSize_t *pTotal, wxDiskspaceSize_t *pFree)
{
#if defined(HAVE_STATFS) || defined(HAVE_STATVFS)
    // the case to "char *" is needed for AIX 4.3
    wxStatfs_t fs;
    if ( wxStatfs((char *)(const char*)path.fn_str(), &fs) != 0 )
    {
        wxLogSysError( wxT("Failed to get file system statistics") );

        return false;
    }

    // under Solaris we also have to use f_frsize field instead of f_bsize
    // which is in general a multiple of f_frsize
#ifdef HAVE_STATVFS
    wxDiskspaceSize_t blockSize = fs.f_frsize;
#else // HAVE_STATFS
    wxDiskspaceSize_t blockSize = fs.f_bsize;
#endif // HAVE_STATVFS/HAVE_STATFS

    if ( pTotal )
    {
        *pTotal = wxDiskspaceSize_t(fs.f_blocks) * blockSize;
    }

    if ( pFree )
    {
        *pFree = wxDiskspaceSize_t(fs.f_bavail) * blockSize;
    }

    return true;
#else // !HAVE_STATFS && !HAVE_STATVFS
    return false;
#endif // HAVE_STATFS
}

// ----------------------------------------------------------------------------
// env vars
// ----------------------------------------------------------------------------

#if USE_PUTENV

WX_DECLARE_STRING_HASH_MAP(char *, wxEnvVars);

static wxEnvVars gs_envVars;

class wxSetEnvModule : public wxModule
{
public:
    virtual bool OnInit() { return true; }
    virtual void OnExit()
    {
        for ( wxEnvVars::const_iterator i = gs_envVars.begin();
              i != gs_envVars.end();
              ++i )
        {
            free(i->second);
        }

        gs_envVars.clear();
    }

    DECLARE_DYNAMIC_CLASS(wxSetEnvModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxSetEnvModule, wxModule)

#endif // USE_PUTENV

bool wxGetEnv(const wxString& var, wxString *value)
{
    // wxGetenv is defined as getenv()
    char *p = wxGetenv(var);
    if ( !p )
        return false;

    if ( value )
    {
        *value = p;
    }

    return true;
}

static bool wxDoSetEnv(const wxString& variable, const char *value)
{
#if defined(HAVE_SETENV)
    if ( !value )
    {
#ifdef HAVE_UNSETENV
        // don't test unsetenv() return value: it's void on some systems (at
        // least Darwin)
        unsetenv(variable.mb_str());
        return true;
#else
        value = ""; // we can't pass NULL to setenv()
#endif
    }

    return setenv(variable.mb_str(), value, 1 /* overwrite */) == 0;
#elif defined(HAVE_PUTENV)
    wxString s = variable;
    if ( value )
        s << wxT('=') << value;

    // transform to ANSI
    const wxWX2MBbuf p = s.mb_str();

    char *buf = (char *)malloc(strlen(p) + 1);
    strcpy(buf, p);

    // store the string to free() it later
    wxEnvVars::iterator i = gs_envVars.find(variable);
    if ( i != gs_envVars.end() )
    {
        free(i->second);
        i->second = buf;
    }
    else // this variable hadn't been set before
    {
        gs_envVars[variable] = buf;
    }

    return putenv(buf) == 0;
#else // no way to set an env var
    return false;
#endif
}

bool wxSetEnv(const wxString& variable, const wxString& value)
{
    return wxDoSetEnv(variable, value.mb_str());
}

bool wxUnsetEnv(const wxString& variable)
{
    return wxDoSetEnv(variable, NULL);
}

// ----------------------------------------------------------------------------
// signal handling
// ----------------------------------------------------------------------------

#if wxUSE_ON_FATAL_EXCEPTION

#include <signal.h>

extern "C" void wxFatalSignalHandler(wxTYPE_SA_HANDLER)
{
    if ( wxTheApp )
    {
        // give the user a chance to do something special about this
        wxTheApp->OnFatalException();
    }

    abort();
}

bool wxHandleFatalExceptions(bool doit)
{
    // old sig handlers
    static bool s_savedHandlers = false;
    static struct sigaction s_handlerFPE,
                            s_handlerILL,
                            s_handlerBUS,
                            s_handlerSEGV;

    bool ok = true;
    if ( doit && !s_savedHandlers )
    {
        // install the signal handler
        struct sigaction act;

        // some systems extend it with non std fields, so zero everything
        memset(&act, 0, sizeof(act));

        act.sa_handler = wxFatalSignalHandler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;

        ok &= sigaction(SIGFPE, &act, &s_handlerFPE) == 0;
        ok &= sigaction(SIGILL, &act, &s_handlerILL) == 0;
        ok &= sigaction(SIGBUS, &act, &s_handlerBUS) == 0;
        ok &= sigaction(SIGSEGV, &act, &s_handlerSEGV) == 0;
        if ( !ok )
        {
            wxLogDebug(wxT("Failed to install our signal handler."));
        }

        s_savedHandlers = true;
    }
    else if ( s_savedHandlers )
    {
        // uninstall the signal handler
        ok &= sigaction(SIGFPE, &s_handlerFPE, NULL) == 0;
        ok &= sigaction(SIGILL, &s_handlerILL, NULL) == 0;
        ok &= sigaction(SIGBUS, &s_handlerBUS, NULL) == 0;
        ok &= sigaction(SIGSEGV, &s_handlerSEGV, NULL) == 0;
        if ( !ok )
        {
            wxLogDebug(wxT("Failed to uninstall our signal handler."));
        }

        s_savedHandlers = false;
    }
    //else: nothing to do

    return ok;
}

#endif // wxUSE_ON_FATAL_EXCEPTION

// ----------------------------------------------------------------------------
// wxExecute support
// ----------------------------------------------------------------------------

int wxAppTraits::AddProcessCallback(wxEndProcessData *data, int fd)
{
    // define a custom handler processing only the closure of the descriptor
    struct wxEndProcessFDIOHandler : public wxFDIOHandler
    {
        wxEndProcessFDIOHandler(wxEndProcessData *data, int fd)
            : m_data(data), m_fd(fd)
        {
        }

        virtual void OnReadWaiting()
        {
            wxFDIODispatcher::Get()->UnregisterFD(m_fd);
            close(m_fd);

            wxHandleProcessTermination(m_data);

            delete this;
        }

        virtual void OnWriteWaiting() { wxFAIL_MSG("unreachable"); }
        virtual void OnExceptionWaiting() { wxFAIL_MSG("unreachable"); }

        wxEndProcessData * const m_data;
        const int m_fd;
    };

    wxFDIODispatcher::Get()->RegisterFD
                             (
                                 fd,
                                 new wxEndProcessFDIOHandler(data, fd),
                                 wxFDIO_INPUT
                             );
    return fd; // unused, but return something unique for the tag
}

bool wxAppTraits::CheckForRedirectedIO(wxExecuteData& execData)
{
#if HAS_PIPE_STREAMS
    bool hasIO = false;

    if ( execData.bufOut && execData.bufOut->Update() )
        hasIO = true;

    if ( execData.bufErr && execData.bufErr->Update() )
        hasIO = true;

    return hasIO;
#else // !HAS_PIPE_STREAMS
    wxUnusedVar(execData);

    return false;
#endif // HAS_PIPE_STREAMS/!HAS_PIPE_STREAMS
}

// helper classes/functions used by WaitForChild()
namespace
{

// convenient base class for IO handlers which are registered for read
// notifications only and which also stores the FD we're reading from
//
// the derived classes still have to implement OnReadWaiting()
class wxReadFDIOHandler : public wxFDIOHandler
{
public:
    wxReadFDIOHandler(wxFDIODispatcher& disp, int fd) : m_fd(fd)
    {
        if ( fd )
            disp.RegisterFD(fd, this, wxFDIO_INPUT);
    }

    virtual void OnWriteWaiting() { wxFAIL_MSG("unreachable"); }
    virtual void OnExceptionWaiting() { wxFAIL_MSG("unreachable"); }

protected:
    const int m_fd;

    wxDECLARE_NO_COPY_CLASS(wxReadFDIOHandler);
};

// class for monitoring our end of the process detection pipe, simply sets a
// flag when input on the pipe (which must be due to EOF) is detected
class wxEndHandler : public wxReadFDIOHandler
{
public:
    wxEndHandler(wxFDIODispatcher& disp, int fd)
        : wxReadFDIOHandler(disp, fd)
    {
        m_terminated = false;
    }

    bool Terminated() const { return m_terminated; }

    virtual void OnReadWaiting() { m_terminated = true; }

private:
    bool m_terminated;

    wxDECLARE_NO_COPY_CLASS(wxEndHandler);
};

#if HAS_PIPE_STREAMS

// class for monitoring our ends of child stdout/err, should be constructed
// with the FD and stream from wxExecuteData and will do nothing if they're
// invalid
//
// unlike wxEndHandler this class registers itself with the provided dispatcher
class wxRedirectedIOHandler : public wxReadFDIOHandler
{
public:
    wxRedirectedIOHandler(wxFDIODispatcher& disp,
                          int fd,
                          wxStreamTempInputBuffer *buf)
        : wxReadFDIOHandler(disp, fd),
          m_buf(buf)
    {
    }

    virtual void OnReadWaiting()
    {
        m_buf->Update();
    }

private:
    wxStreamTempInputBuffer * const m_buf;

    wxDECLARE_NO_COPY_CLASS(wxRedirectedIOHandler);
};

#endif // HAS_PIPE_STREAMS

// helper function which calls waitpid() and analyzes the result
int DoWaitForChild(int pid, int flags = 0)
{
    wxASSERT_MSG( pid > 0, "invalid PID" );

    int status, rc;

    // loop while we're getting EINTR
    for ( ;; )
    {
        rc = waitpid(pid, &status, flags);

        if ( rc != -1 || errno != EINTR )
            break;
    }

    if ( rc == 0 )
    {
        // This can only happen if the child application closes our dummy pipe
        // that is used to monitor its lifetime; in that case, our best bet is
        // to pretend the process did terminate, because otherwise wxExecute()
        // would hang indefinitely (OnReadWaiting() won't be called again, the
        // descriptor is closed now).
        wxLogDebug("Child process (PID %d) still alive but pipe closed so "
                   "generating a close notification", pid);
    }
    else if ( rc == -1 )
    {
        wxLogLastError(wxString::Format("waitpid(%d)", pid));
    }
    else // child did terminate
    {
        wxASSERT_MSG( rc == pid, "unexpected waitpid() return value" );

        // notice that the caller expects the exit code to be signed, e.g. -1
        // instead of 255 so don't assign WEXITSTATUS() to an int
        signed char exitcode;
        if ( WIFEXITED(status) )
            exitcode = WEXITSTATUS(status);
        else if ( WIFSIGNALED(status) )
            exitcode = -WTERMSIG(status);
        else
        {
            wxLogError("Child process (PID %d) exited for unknown reason, "
                       "status = %d", pid, status);
            exitcode = -1;
        }

        return exitcode;
    }

    return -1;
}

} // anonymous namespace

int wxAppTraits::WaitForChild(wxExecuteData& execData)
{
    if ( !(execData.flags & wxEXEC_SYNC) )
    {
        // asynchronous execution: just launch the process and return,
        // endProcData will be destroyed when it terminates (currently we leak
        // it if the process doesn't terminate before we do and this should be
        // fixed but it's not a real leak so it's not really very high
        // priority)
        wxEndProcessData *endProcData = new wxEndProcessData;
        endProcData->process = execData.process;
        endProcData->pid = execData.pid;
        endProcData->tag = AddProcessCallback
                           (
                             endProcData,
                             execData.GetEndProcReadFD()
                           );
        endProcData->async = true;

        return execData.pid;
    }
    //else: synchronous execution case

#if HAS_PIPE_STREAMS && wxUSE_SOCKETS
    wxProcess * const process = execData.process;
    if ( process && process->IsRedirected() )
    {
        // we can't simply block waiting for the child to terminate as we would
        // dead lock if it writes more than the pipe buffer size (typically
        // 4KB) bytes of output -- it would then block waiting for us to read
        // the data while we'd block waiting for it to terminate
        //
        // so multiplex here waiting for any input from the child or closure of
        // the pipe used to indicate its termination
        wxSelectDispatcher disp;

        wxEndHandler endHandler(disp, execData.GetEndProcReadFD());

        wxRedirectedIOHandler outHandler(disp, execData.fdOut, execData.bufOut),
                              errHandler(disp, execData.fdErr, execData.bufErr);

        while ( !endHandler.Terminated() )
        {
            disp.Dispatch();
        }
    }
    //else: no IO redirection, just block waiting for the child to exit
#endif // HAS_PIPE_STREAMS

    return DoWaitForChild(execData.pid);
}

void wxHandleProcessTermination(wxEndProcessData *data)
{
    data->exitcode = DoWaitForChild(data->pid, WNOHANG);

    // notify user about termination if required
    if ( data->process )
    {
        data->process->OnTerminate(data->pid, data->exitcode);
    }

    if ( data->async )
    {
        // in case of asynchronous execution we don't need this data any more
        // after the child terminates
        delete data;
    }
    else // sync execution
    {
        // let wxExecute() know that the process has terminated
        data->pid = 0;
    }
}

