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

#include <vector>
#include <string>

#include "Common.h"
#include "CommonPaths.h"

#if defined HAVE_X11 && HAVE_X11
#include <X11/Xlib.h>
#endif

#include "CPUDetect.h"
#include "IniFile.h"
#include "FileUtil.h"
#include "Setup.h"

#include "Host.h" // Core
#include "HW/Wiimote.h"

#include "Globals.h" // Local
#include "Main.h"
#include "ConfigManager.h"
#include "Debugger/CodeWindow.h"
#include "Debugger/JitWindow.h"
#include "BootManager.h"
#include "Frame.h"

#include "VideoBackendBase.h"

#include <wx/intl.h>

// ------------
//  Main window

IMPLEMENT_APP(DolphinApp)

BEGIN_EVENT_TABLE(DolphinApp, wxApp)
	EVT_TIMER(wxID_ANY, DolphinApp::AfterInit)
END_EVENT_TABLE()

#include <wx/stdpaths.h>
bool wxMsgAlert(const char*, const char*, bool, int);
std::string wxStringTranslator(const char *);

CFrame* main_frame = NULL;

#ifdef WIN32

#ifdef _M_IX86
#define CREG(x) ContextRecord->E##x
#elif defined _M_X64
#define CREG(x) ContextRecord->R##x
#endif

// bind some crashing cases to buttons or something			done (good enough)
// implement streams										done
// implement compression									done
// filter dumping											TODO
// progress of dumping / compression						TODO
// use stack guarantee for each thread						done (by hacking up std::thread impl)

#pragma optimize("",off)
u64 stack_overflow(int x, int y)
{
	u32 filler[0x10000] = { 0xdeadbeef };
	return stack_overflow(x, x + y);
}


#include <imagehlp.h>
#pragma comment(lib, "DbgHelp.lib")

bool WriteDump(LPTSTR path,
	PMINIDUMP_EXCEPTION_INFORMATION exception_context,
	PMINIDUMP_USER_STREAM_INFORMATION user_streams)
{
	auto success = FALSE;
	auto dump_file_handle = CreateFile(
		path,
		GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		nullptr
		);

	if (dump_file_handle != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_TYPE type = (MINIDUMP_TYPE)(
			MiniDumpNormal |
			/*
			MiniDumpWithFullMemory |
			MiniDumpIgnoreInaccessibleMemory |
			//*/
			// so we can see files / devices in use
			MiniDumpWithHandleData |
			// times, entry point, affinity
			MiniDumpWithThreadInfo
			);
		success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
			dump_file_handle, type, exception_context, user_streams, nullptr);

		CloseHandle(dump_file_handle);
	}

	return success == TRUE;
}

#include "../../../Externals/zlib/zlib.h"

#define CHUNK 16384

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int def(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

// Compresses the input path to a file with a unique name in a temp dir
bool CompressDump(std::string const &path, std::wstring &compressed_path)
{
	File::IOFile dump(path, "rb");

	TCHAR temp_dir[MAX_PATH];
	TCHAR temp_path[MAX_PATH];
	GetTempPath(MAX_PATH, temp_dir);
	GetTempFileName(temp_dir, TEXT("dol"), 0, temp_path);
	compressed_path = temp_path;
	char *compressed_path_c = new char[compressed_path.size() * 2 + 2];
	wcstombs(compressed_path_c, temp_path, MAX_PATH);
	File::IOFile dump_compressed(compressed_path_c, "wb");
	delete [] compressed_path_c;

	auto result = def(dump.GetHandle(), dump_compressed.GetHandle(), Z_BEST_COMPRESSION);

	return result == Z_OK;
}

#include <algorithm>
class DumpStreams
{
	MINIDUMP_USER_STREAM_INFORMATION si;
	std::vector<MINIDUMP_USER_STREAM> v_us;
	std::vector<void *> caches;

	enum BufferType
	{
		NamedFile = LastReservedStream + 1
	};

	void AddStream(BufferType const type, void const *buffer, u64 const size)
	{
		MINIDUMP_USER_STREAM s;

		s.Type = type;
		s.Buffer = (PVOID)buffer;
		s.BufferSize = size;

		v_us.push_back(s);
	}

	void const *AddToCaches(
		void const *buffer, u64 const size,
		void const *extra_buffer = nullptr, u64 const extra_size = 0)
	{
		u8 *cache = new u8[extra_size + size];

		if (extra_buffer)
			std::copy((u8 const *)extra_buffer, (u8 const *)extra_buffer + extra_size, cache);

		std::copy((u8 const *)buffer, (u8 const *)buffer + size, cache + extra_size);

		caches.push_back(cache);

		return cache;
	}

	void UpdateStreamInfo()
	{
		si.UserStreamCount = v_us.size();
		si.UserStreamArray = v_us.data();
	}

public:

	~DumpStreams()
	{
		for (auto it = caches.begin(); it != caches.end(); it++)
			delete [] *it;
	}

	void AddFileStream(std::string const &name, void const *buffer, u64 const size)
	{
		auto const name_size = name.size() + 1;
		AddStream(NamedFile,
			AddToCaches(buffer, size, name.c_str(), name_size),
			name_size + size);
	}

	void AddFileStream(std::string const &name, std::string const &path)
	{
		auto const name_size = name.size() + 1;
		File::IOFile file(path, "rb");

		u64 file_size = file.GetSize();
		u8 *cache = new u8[name_size + file_size];
		std::copy(name.begin(), name.end(), cache);
		file.ReadBytes(cache + name_size, file_size);

		caches.push_back(cache);

		AddStream(NamedFile,
			cache,
			name_size + file_size);
	}

	PMINIDUMP_USER_STREAM_INFORMATION GetStreams()
	{
		UpdateStreamInfo();

		return &si;
	}
};

#ifdef _M_IX86
#define DOLPHIN_ARCH_STR "x86"
#elif defined _M_X64
#define DOLPHIN_ARCH_STR "x64"
#endif

#include <sstream>
#include "scmrev.h"
#include "FileSearch.h"
#include "../../../Plugins/Plugin_VideoDX9/Src/D3DBase.h"

void CreateCompressedDump(PMINIDUMP_EXCEPTION_INFORMATION ei, std::wstring &output_path)
{
	DumpStreams ds;
	std::string temp_str;

	// Add information about the dolphin build

	ds.AddFileStream("dolphin.arch", DOLPHIN_ARCH_STR, sizeof(DOLPHIN_ARCH_STR));
	ds.AddFileStream("dolphin.desc", SCM_DESC_STR, sizeof(SCM_DESC_STR));
	ds.AddFileStream("dolphin.branch", SCM_BRANCH_STR, sizeof(SCM_BRANCH_STR));
	ds.AddFileStream("dolphin.revision", SCM_REV_STR, sizeof(SCM_REV_STR));

	// TODO add module / offset

	// Add information about the hardware

	temp_str = cpu_info.Summarize();
	ds.AddFileStream("info.cpu", temp_str.c_str(), temp_str.size());

	int num_adapters = DX9::D3D::GetNumAdapters();
	for (int i = 0; i < num_adapters; i++)
	{
		auto adapter = DX9::D3D::GetAdapter(i);
		std::stringstream adapter_num;
		adapter_num << i;
		ds.AddFileStream("info.gpu" + adapter_num.str(), &adapter.ident, sizeof(adapter.ident));
	}

	// Add game id

	temp_str = SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID();
	ds.AddFileStream("info.game_id", temp_str.c_str(), temp_str.size());

	// Add all ini files in User/Config/

	CFileSearch::XStringVector Directories;
	Directories.push_back(File::GetUserPath(D_CONFIG_IDX));
	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.ini");
	CFileSearch FileSearch(Extensions, Directories);
	for (auto it = FileSearch.GetFileNames().cbegin(); it != FileSearch.GetFileNames().cend(); it++)
	{
		auto const basename_start = it->rfind(DIR_SEP_CHR) + 1;
		std::string const basename = it->substr(basename_start, it->size() - basename_start - strlen(".ini"));
		ds.AddFileStream("config." + basename, *it);
	}

	// Add current game's ini

	auto const gameini_path = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni;
	auto const basename_start = gameini_path.rfind(DIR_SEP_CHR) + 1;
	std::string const basename = gameini_path.substr(basename_start, gameini_path.size() - basename_start - strlen(".ini"));
	ds.AddFileStream("config." + basename, gameini_path);


	if (!WriteDump(L"mini.dmp", ei, ds.GetStreams()))
	{
		MessageBox(nullptr, L"failed to write the dump file", L"error", 0);
	}
	
	if (!CompressDump("mini.dmp", output_path))
	{
		MessageBox(nullptr, L"failed to compress the dump file", L"error", 0);
	}
}

#include <Bits.h>
#include <functional>

struct BITSWrapper
{
	IBackgroundCopyManager *manager;
	HRESULT hr;
	bool is_valid;

	static std::wstring const dolphin_job_name;

	BITSWrapper()
	{
		is_valid = false;

		hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (SUCCEEDED(hr))
		{
			hr = CoCreateInstance(
				__uuidof(BackgroundCopyManager), nullptr, CLSCTX_LOCAL_SERVER,
				__uuidof(IBackgroundCopyManager), (LPVOID *)&manager);
			if (SUCCEEDED(hr))
			{
				is_valid = true;
			}
		}
	}

	~BITSWrapper()
	{
		if (is_valid)
		{
			manager->Release();
		}
		CoUninitialize();
	}

	// Enumerates all of the current user's BITS jobs.
	void for_each_job(
		std::wstring const &job_name,
		std::function<void(IBackgroundCopyJob *)> f)
	{
		if (is_valid)
		{
			IEnumBackgroundCopyJobs *pJobs = nullptr;

			hr = manager->EnumJobs(0, &pJobs);
			if (SUCCEEDED(hr))
			{
				ULONG job_count;
				pJobs->GetCount(&job_count);

				for (ULONG i = 0; i < job_count; ++i)
				{
					IBackgroundCopyJob *pJob = nullptr;
					hr = pJobs->Next(1, &pJob, nullptr);
					if (hr == S_OK)
					{
						wchar_t *name = nullptr;
						hr = pJob->GetDisplayName(&name);
						if (SUCCEEDED(hr))
						{
							if (job_name.empty() || name == job_name)
							{
								f(pJob);
							}
							CoTaskMemFree(name);
						}

						pJob->Release();
					}
					else
					{
						// Bail out on error
						break;
					}
				}

				pJobs->Release();
			}
		}
	}

	void for_each_file(
		IBackgroundCopyJob *pJob,
		std::function<void(IBackgroundCopyFile *)> f)
	{
		if (is_valid)
		{
			IEnumBackgroundCopyFiles *pFiles = nullptr;

			hr = pJob->EnumFiles(&pFiles);
			if (SUCCEEDED(hr))
			{
				ULONG count;
				pFiles->GetCount(&count);

				for (ULONG i = 0; i < count; ++i)
				{
					IBackgroundCopyFile *pFile = nullptr;
					hr = pFiles->Next(1, &pFile, nullptr);
					if (hr == S_OK)
					{
						f(pFile);

						pFile->Release();
					}
					else
					{
						// Bail out on error
						break;
					}
				}

				pFiles->Release();
			}
		}
	}

	void DeleteTempFile(IBackgroundCopyJob *pJob)
	{
		for_each_file(pJob, [&](IBackgroundCopyFile *pFile)
		{
			TCHAR *temp_path = nullptr;
			hr = pFile->GetLocalName(&temp_path);
			if (SUCCEEDED(hr))
			{
				DeleteFile(temp_path);
				CoTaskMemFree(temp_path);
			}
		});
	}

	// Marks any completed dolphin transfers as being completed.
	void TryPurge()
	{
		for_each_job(dolphin_job_name, [&](IBackgroundCopyJob *pJob)
		{
			BG_JOB_STATE state;
			hr = pJob->GetState(&state);

			if (hr == S_OK)
			{
				switch (state)
				{
				case BG_JOB_STATE_TRANSFERRED:
					pJob->Complete();
					DeleteTempFile(pJob);
					break;

				case BG_JOB_STATE_ERROR:
					// Inspecting the error context is just for debugging,
					// we'll decide what to do with the faulty job based on
					// it's age (see below)
					IBackgroundCopyError *pError = nullptr;
					pJob->GetError(&pError);
					if (SUCCEEDED(hr))
					{
						BG_ERROR_CONTEXT context;
						HRESULT code;
						pError->GetError(&context, &code);
						TCHAR *description = nullptr;
						hr = pError->GetErrorDescription(LANGIDFROMLCID(GetThreadLocale()), &description);
						if (SUCCEEDED(hr))
						{
							CoTaskMemFree(description);
						}
						pError->Release();
					}

					// If job was created less than 30 days ago and might be recoverable,
					// try resuming it else, cancel it. 

					BG_JOB_TIMES times;
					hr = pJob->GetTimes(&times);
					if (SUCCEEDED(hr))
					{
						FILETIME now_filetime;
						GetSystemTimeAsFileTime(&now_filetime);
						u64 const now = (u64)now_filetime.dwHighDateTime << 32 | now_filetime.dwLowDateTime;
						u64 const job = (u64)times.CreationTime.dwHighDateTime << 32 | times.CreationTime.dwLowDateTime;
						u64 const thirty_days = 30 * 24 * 60 * 60 * 10000000;

						if (now - job < thirty_days)
						{
							pJob->Resume();
						}
						else
						{
							pJob->Cancel();
							DeleteTempFile(pJob);
						}
					}
					break;
				}
			}
		});
	}

	void Upload(std::wstring const &local, std::wstring const &remote)
	{
		if (is_valid)
		{
			GUID JobId;
			IBackgroundCopyJob *pJob = nullptr;

			hr = manager->CreateJob(dolphin_job_name.c_str(), BG_JOB_TYPE_UPLOAD, &JobId, &pJob);
			if (SUCCEEDED(hr))
			{
				hr = pJob->AddFile(remote.c_str(), local.c_str());
				if (SUCCEEDED(hr))
				{
					hr = pJob->Resume();
				}
			}

			pJob->Release();
		}
	}
};
std::wstring const BITSWrapper::dolphin_job_name = L"dolphin-emu crash report";

#include <SFML/Network.hpp>
int __stdcall monitor_thread(PMINIDUMP_EXCEPTION_INFORMATION ei)
{
	// doesn't appear to be needed
	//SuspendThreads();

	sf::Http Http("goliath", 8000);

	sf::Http::Request Request;
	Request.SetMethod(sf::Http::Request::Get);
	Request.SetURI("/dumps/upload/small");
	Request.SetField("X-Dolphin-Version", SCM_REV_STR);
	Request.SetField("X-Dolphin-Architecture", DOLPHIN_ARCH_STR);
	Request.SetField("X-Crash-Module", "module"); // TODO
	Request.SetField("X-Crash-EIP-Offset", "offset");

	// Send it and get the response returned by the server
	// TODO set timeout?
	sf::Http::Response Response = Http.SendRequest(Request);

	//if (Response.GetStatus() == Response.Ok)
	{
		auto issue_link = Response.GetField("X-Issue-Link");
		auto possible_fix = Response.GetField("X-Possible-Fix");
		auto upload_token = Response.GetField("X-Upload-Token");

		if (upload_token.empty())
		{
			// Sent report ok, but we weren't asked for dump
			//return true;
		}

		// Send dump - mini.dmp.zl to /dumps/upload/full/<token>
		std::wstring dump_file;
		CreateCompressedDump(ei, dump_file);
		BITSWrapper bits;
		bits.TryPurge(); // TODO this should be called at dolphin startup instead
		bits.Upload(dump_file,
			L"http://goliath:8000/dumps/upload/full/d15ea5e/mini.dmp.zl");

		//return true;
	}
	//else
	{
		// Failed to send dump
		//return false;
	}

	// Here it's mostly safe to display some info to the user / upload dump file

	// by default, std::terminate() has some funky behavior on msvc which
	// allows you to break in with a debugger or shows the typical
	// "the application has stopped working" dialog. If we want to show any gui,
	// we'll do it ourselves. This is why we call TerminateProcess directly.

	HANDLE this_process = GetCurrentProcess();
	TerminateProcess(this_process, 0xc0ffee);
	WaitForSingleObject(this_process, INFINITE);

	return 0;
}

LONG NTAPI LastChanceExceptionFilter(PEXCEPTION_POINTERS e)
{
	switch (e->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_STACK_OVERFLOW:
		{
		MINIDUMP_EXCEPTION_INFORMATION ei = { GetCurrentThreadId(), e, TRUE };

		std::thread monitor(monitor_thread, &ei);
		monitor.join();
		}
		return EXCEPTION_CONTINUE_SEARCH;

	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}
#pragma optimize("",on)
#endif

bool DolphinApp::Initialize(int& c, wxChar **v)
{
#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif 
	return wxApp::Initialize(c, v);
}

// The `main program' equivalent that creates the main window and return the main frame 

bool DolphinApp::OnInit()
{
	InitLanguageSupport();

	// Declarations and definitions
	bool UseDebugger = false;
	bool UseLogger = false;
	bool selectVideoBackend = false;
	bool selectAudioEmulation = false;

	wxString videoBackendName;
	wxString audioEmulationName;

#if wxUSE_CMDLINE_PARSER // Parse command lines
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{
			wxCMD_LINE_SWITCH, "h", "help",
			_("Show this help message"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
		},
		{
			wxCMD_LINE_SWITCH, "d", "debugger",
			_("Opens the debugger"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, "l", "logger",
			_("Opens the logger"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "e", "exec",
			_("Loads the specified file (DOL,ELF,GCM,ISO,WAD)"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, "b", "batch",
			_("Exit Dolphin with emulator"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "V", "video_backend",
			_("Specify a video backend"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "A", "audio_emulation",
			_("Low level (LLE) or high level (HLE) audio"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_NONE, NULL, NULL, NULL, wxCMD_LINE_VAL_NONE, 0
		}
	};

	// Gets the command line parameters
	wxCmdLineParser parser(cmdLineDesc, argc, argv);
	if (parser.Parse() != 0)
	{
		return false;
	} 

	UseDebugger = parser.Found(wxT("debugger"));
	UseLogger = parser.Found(wxT("logger"));
	LoadFile = parser.Found(wxT("exec"), &FileToLoad);
	BatchMode = parser.Found(wxT("batch"));
	selectVideoBackend = parser.Found(wxT("video_backend"),
		&videoBackendName);
	// TODO:  This currently has no effect.  Implement or delete.
	selectAudioEmulation = parser.Found(wxT("audio_emulation"),
		&audioEmulationName);
#endif // wxUSE_CMDLINE_PARSER

#if defined _DEBUG && defined _WIN32
	int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
	_CrtSetDbgFlag(tmpflag);
#endif

	// Register message box and translation handlers
	RegisterMsgAlertHandler(&wxMsgAlert);
	RegisterStringTranslator(&wxStringTranslator);

#ifdef _WIN32
	AddVectoredExceptionHandler(TRUE, LastChanceExceptionFilter);
#elif wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
#endif

	// TODO: if First Boot
	if (!cpu_info.bSSE2) 
	{
		PanicAlertT("Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
				"Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
				"Sayonara!\n");
		return false;
	}

#ifdef _WIN32
	if (!wxSetWorkingDirectory(wxString(File::GetExeDirectory().c_str(), *wxConvCurrent)))
	{
		INFO_LOG(CONSOLE, "set working directory failed");
	}
#else
	//create all necessary directories in user directory
	//TODO : detect the revision and upgrade where necessary
	File::CopyDir(std::string(SHARED_USER_DIR GAMECONFIG_DIR DIR_SEP),
			File::GetUserPath(D_GAMECONFIG_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR MAPS_DIR DIR_SEP),
			File::GetUserPath(D_MAPS_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR SHADERS_DIR DIR_SEP),
			File::GetUserPath(D_SHADERS_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR WII_USER_DIR DIR_SEP),
			File::GetUserPath(D_WIIUSER_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR OPENCL_DIR DIR_SEP),
			File::GetUserPath(D_OPENCL_IDX));
#endif
	File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
	File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
	File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));

	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	WiimoteReal::LoadSettings();

	if (selectVideoBackend && videoBackendName != wxEmptyString)
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend =
			std::string(videoBackendName.mb_str());

	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);

	// Enable the PNG image handler for screenshots
	wxImage::AddHandler(new wxPNGHandler);

	SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);

	int x = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX;
	int y = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY;
	int w = SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth;
	int h = SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight;

	// The following is not needed with X11, where window managers
	// do not allow windows to be created off the desktop.
#ifdef _WIN32
	// Out of desktop check
	HWND hDesktop = GetDesktopWindow();
	RECT rc;
	GetWindowRect(hDesktop, &rc);
	if (rc.right < x + w || rc.bottom < y + h)
		x = y = wxDefaultCoord;
#elif defined __APPLE__
	if (y < 1)
		y = wxDefaultCoord;
#endif

	main_frame = new CFrame((wxFrame*)NULL, wxID_ANY,
				wxString::FromAscii(scm_rev_str),
				wxPoint(x, y), wxSize(w, h),
				UseDebugger, BatchMode, UseLogger);
	SetTopWindow(main_frame);
	main_frame->SetMinSize(wxSize(400, 300));

	// Postpone final actions until event handler is running.
	// Updating the game list makes use of wxProgressDialog which may
	// only be run after OnInit() when the event handler is running.
	m_afterinit = new wxTimer(this, wxID_ANY);
	m_afterinit->Start(1, wxTIMER_ONE_SHOT);

	return true;
}

void DolphinApp::MacOpenFile(const wxString &fileName)
{
	FileToLoad = fileName;
	LoadFile = true;

	if (m_afterinit == NULL)
		main_frame->BootGame(std::string(FileToLoad.mb_str()));
}

void DolphinApp::AfterInit(wxTimerEvent& WXUNUSED(event))
{
	delete m_afterinit;
	m_afterinit = NULL;

	if (!BatchMode)
		main_frame->UpdateGameList();

	// First check if we have an exec command line.
	if (LoadFile && FileToLoad != wxEmptyString)
	{
		main_frame->BootGame(std::string(FileToLoad.mb_str()));
	}
	// If we have selected Automatic Start, start the default ISO,
	// or if no default ISO exists, start the last loaded ISO
	else if (main_frame->g_pCodeWindow)
	{
		if (main_frame->g_pCodeWindow->AutomaticStart())
		{
			if(!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.empty()
					&& File::Exists(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM))
			{
				main_frame->BootGame(SConfig::GetInstance().m_LocalCoreStartupParameter.
						m_strDefaultGCM);
			}
			else if(!SConfig::GetInstance().m_LastFilename.empty()
					&& File::Exists(SConfig::GetInstance().m_LastFilename))
			{
				main_frame->BootGame(SConfig::GetInstance().m_LastFilename);
			}	
		}
	}
}

void DolphinApp::InitLanguageSupport()
{
	unsigned int language = 0;

	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	ini.Get("Interface", "Language", &language, wxLANGUAGE_DEFAULT);

	// Load language if possible, fall back to system default otherwise
	if(wxLocale::IsAvailable(language))
	{
		m_locale = new wxLocale(language);

#ifdef _WIN32
		m_locale->AddCatalogLookupPathPrefix(wxT("Languages"));
#endif

		m_locale->AddCatalog(wxT("dolphin-emu"));

		if(!m_locale->IsOk())
		{
			PanicAlertT("Error loading selected language. Falling back to system default.");
			delete m_locale;
			m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
		}
	}
	else
	{
		PanicAlertT("The selected language is not supported by your system. Falling back to system default.");
		m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}
}

int DolphinApp::OnExit()
{
	WiimoteReal::Shutdown();
#ifdef _WIN32
	if (SConfig::GetInstance().m_WiiAutoUnpair)
		WiimoteReal::UnPair();
#endif
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();

	delete m_locale;

	return wxApp::OnExit();
}

void DolphinApp::OnFatalException()
{
	WiimoteReal::Shutdown();
}


// ------------
// Talk to GUI

void Host_SysMessage(const char *fmt, ...) 
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg)-1] == '\n') msg[strlen(msg)-1] = 0;
	//wxMessageBox(wxString::FromAscii(msg));
	PanicAlert("%s", msg);
}

bool wxMsgAlert(const char* caption, const char* text, bool yes_no, int /*Style*/)
{
#ifdef __WXGTK__
	if (wxIsMainThread())
#endif
		return wxYES == wxMessageBox(wxString::FromUTF8(text), 
				wxString::FromUTF8(caption),
				(yes_no) ? wxYES_NO : wxOK, wxGetActiveWindow());
#ifdef __WXGTK__
	else
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_PANIC);
		event.SetString(wxString::FromUTF8(caption) + wxT(":") + wxString::FromUTF8(text));
		event.SetInt(yes_no);
		main_frame->GetEventHandler()->AddPendingEvent(event);
		main_frame->panic_event.Wait();
		return main_frame->bPanicResult;
	}
#endif
}

std::string wxStringTranslator(const char *text)
{
	return (const char *)wxString(wxGetTranslation(wxString::From8BitData(text))).ToUTF8();
}

// Accessor for the main window class
CFrame* DolphinApp::GetCFrame()
{
	return main_frame;
}

void Host_Message(int Id)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, Id);
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

#ifdef _WIN32
extern "C" HINSTANCE wxGetInstance();
void* Host_GetInstance()
{
	return (void*)wxGetInstance();
}
#else
void* Host_GetInstance()
{
	return NULL;
}
#endif

void* Host_GetRenderHandle()
{
    return main_frame->GetRenderHandle();
}

// OK, this thread boundary is DANGEROUS on linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_NOTIFYMAPLOADED);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}


void Host_UpdateLogDisplay()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATELOGDISPLAY);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}


void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEDISASMDIALOG);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}


void Host_ShowJitResults(unsigned int address)
{
	if (main_frame->g_pCodeWindow && main_frame->g_pCodeWindow->m_JitWindow)
		main_frame->g_pCodeWindow->m_JitWindow->ViewAddr(address);
}

void Host_UpdateMainFrame()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEGUI);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateTitle(const char* title)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATETITLE);
	event.SetString(wxString::FromAscii(title));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_UpdateBreakPointView()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEBREAKPOINTS);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

bool Host_GetKeyState(int keycode)
{
#ifdef _WIN32
	return (0 != GetAsyncKeyState(keycode));
#elif defined __WXGTK__
	std::unique_lock<std::recursive_mutex> lk(main_frame->keystate_lock, std::try_to_lock);
	if (!lk.owns_lock())
		return false;

	bool key_pressed;
	if (!wxIsMainThread()) wxMutexGuiEnter();
	key_pressed = wxGetKeyState(wxKeyCode(keycode));
	if (!wxIsMainThread()) wxMutexGuiLeave();
	return key_pressed;
#else
	return wxGetKeyState(wxKeyCode(keycode));
#endif
}

void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
	main_frame->GetRenderWindowSize(x, y, width, height);
}

void Host_RequestRenderWindowSize(int width, int height)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_WINDOWSIZEREQUEST);
	event.SetClientData(new std::pair<int, int>(width, height));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_SetStartupDebuggingParameters()
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;
	if (main_frame->g_pCodeWindow)
	{    
		StartUp.bBootToPause = main_frame->g_pCodeWindow->BootToPause();
		StartUp.bAutomaticStart = main_frame->g_pCodeWindow->AutomaticStart();
		StartUp.bJITNoBlockCache = main_frame->g_pCodeWindow->JITNoBlockCache();
		StartUp.bJITBlockLinking = main_frame->g_pCodeWindow->JITBlockLinking();
	}
	else
	{
		StartUp.bBootToPause = false;
	}
	StartUp.bEnableDebugging = main_frame->g_pCodeWindow ? true : false; // RUNNING_DEBUG
}

void Host_UpdateStatusBar(const char* _pText, int Field)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);
	// Set the event string
	event.SetString(wxString::FromAscii(_pText));
	// Update statusbar field
	event.SetInt(Field);
	// Post message
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_SetWiiMoteConnectionState(int _State)
{
	static int currentState = -1;
	if (_State == currentState)
		return;
	currentState = _State;

	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);

	switch(_State)
	{
	case 0: event.SetString(_("Not connected")); break;
	case 1: event.SetString(_("Connecting...")); break;
	case 2: event.SetString(_("Wiimote Connected")); break;
	}
	// Update field 1 or 2
	event.SetInt(1);

	main_frame->GetEventHandler()->AddPendingEvent(event);
}

bool Host_RendererHasFocus()
{
	return main_frame->RendererHasFocus();
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	CFrame::ConnectWiimote(wm_idx, connect);
}
