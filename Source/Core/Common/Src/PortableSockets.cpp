// Under MIT licence from http://www.mindcontrol.org/~hplus/http-get.html

#include "PortableSockets.h"

#if NEED_GETTIMEOFDAY
#if defined( _WIN32 )

#include <sys/types.h>
#include <sys/timeb.h>
#include <math.h>
#include <assert.h>
#include <mmsystem.h>

#pragma comment( lib, "winmm.lib" )
//#pragma comment( lib, "ws2_32.lib" )


namespace {

	// This class could be made cheaper using RDTSC for short-term 
	// measurement. But, whatever.
	class init_gettimeofday
	{
	public:
		init_gettimeofday()
		{
			timeBeginPeriod( 2 );
			__int64 rr;
			QueryPerformanceFrequency( (LARGE_INTEGER *)&rr );
			ticksPerSecInv_ = 1.0 / (double)((DWORD)rr & 0xffffffff);
			int watchdog = 0;
again:
			lastTicks_ = timeGetTime();
			QueryPerformanceCounter( (LARGE_INTEGER *)&lastRead_ );
			timeb tb;
			ftime( &tb );
			timeOffset_ = tb.time + tb.millitm * 0.001 - lastRead_ * ticksPerSecInv_;
			lastTime_ = timeOffset_;
			//  make sure it didn't take too long
			if( watchdog++ < 10 && (timeGetTime() != lastTicks_) ) {
				goto again;
			}
		}
		~init_gettimeofday()
		{
			timeEndPeriod( 2 );
		}

		void get( timeval * tv )
		{
			__int64 nu;
			int watchdog = 0;
again:
			DWORD m = timeGetTime();
			QueryPerformanceCounter( (LARGE_INTEGER *)&nu );
			DWORD n = timeGetTime();
			// guard against pre-emption
			if( (watchdog++ < 10) && (n != m) ) {
				goto again;
			}
			double nuTime = nu * ticksPerSecInv_ + timeOffset_;
			if( (nu - lastRead_) & 0x7fffffff80000000ULL ) {
				//  there's a chance that we're seeing a jump-ahead
				double adjust = (nuTime - lastTime_ - (n - lastTicks_) * 0.001);
				if( adjust > 0.1f ) {
					timeOffset_ -= adjust;
					nuTime -= adjust;
					assert( nuTime >= lastTime_ );
				}
			}
			lastRead_ = nu;
			lastTicks_ = n;
			lastTime_ = nuTime;
			tv->tv_sec = (ulong)floor( nuTime );
			tv->tv_usec = (ulong)(1000000 * (nuTime - tv->tv_sec));
		}

		double ticksPerSecInv_;
		double timeOffset_;
		double lastTime_;
		__int64 lastRead_;
		DWORD lastTicks_;
	};

}

void gettimeofday( timeval * tv, int )
{
	static init_gettimeofday data;
	data.get( tv );
}

#else
#error "don't know how to do this"
#endif
#endif

#if NEED_WINDOWS_POLL
#if defined( WIN32 )

#include <assert.h>
#include <winsock2.h>
#include <windows.h>

//  This is somewhat less than ideal -- better would be if we could 
//  abstract pollfd enough that it's non-copying on Windows.
int poll( pollfd * iofds, size_t count, int ms )
{
	FD_SET rd, wr, ex;
	FD_ZERO( &rd );
	FD_ZERO( &wr );
	FD_ZERO( &ex );
	SOCKET m = 0;
	for( size_t ix = 0; ix < count; ++ix ) {
		iofds[ix].revents = 0;
		if( iofds[ix].fd >= m ) {
			m = iofds[ix].fd + 1;
		}
		if( iofds[ix].events & (POLLIN | POLLPRI) ) {
			assert( rd.fd_count < FD_SETSIZE );
			rd.fd_array[ rd.fd_count++ ] = iofds[ix].fd;
		}
		if( iofds[ix].events & (POLLOUT) ) {
			assert( wr.fd_count < FD_SETSIZE );
			wr.fd_array[ wr.fd_count++ ] = iofds[ix].fd;
		}
		assert( ex.fd_count < FD_SETSIZE );
		ex.fd_array[ ex.fd_count++ ] = iofds[ix].fd;
	}
	timeval tv;
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms - (tv.tv_sec * 1000)) * 1000;
	int r = 0;
	if( m == 0 ) {
		::Sleep( ms );
	}
	else {
		r = ::select( (int)m, (rd.fd_count ? &rd : 0), (wr.fd_count ? &wr : 0), (ex.fd_count ? &ex : 0), &tv );
	}
	if( r < 0 ) {
		int err = WSAGetLastError();
		errno = err;
		return r;
	}
	r = 0;
	for( size_t ix = 0; ix < count; ++ix ) {
		for( size_t iy = 0; iy < rd.fd_count; ++iy ) {
			if( rd.fd_array[ iy ] == iofds[ix].fd ) {
				iofds[ix].revents |= POLLIN;
				++r;
				break;
			}
		}
		for( size_t iy = 0; iy < wr.fd_count; ++iy ) {
			if( wr.fd_array[ iy ] == iofds[ix].fd ) {
				iofds[ix].revents |= POLLOUT;
				++r;
				break;
			}
		}
		for( size_t iy = 0; iy < ex.fd_count; ++iy ) {
			if( ex.fd_array[ iy ] == iofds[ix].fd ) {
				iofds[ix].revents |= POLLERR;
				++r;
				break;
			}
		}
	}
	return r;
}

#else
#error "don't know how to do this"
#endif
#endif


#if NEED_FIREWALL_ENABLE
#if defined( WIN32 )

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500

#include <objbase.h>
#include <oleauto.h>
//#include <netfw.h>

/*
#define _ASSERT assert

namespace {

	HRESULT WindowsFirewallInitialize(OUT INetFwProfile** fwProfile)
	{
		HRESULT hr = S_OK;
		INetFwMgr* fwMgr = NULL;
		INetFwPolicy* fwPolicy = NULL;

		_ASSERT(fwProfile != NULL);

		*fwProfile = NULL;

		// Create an instance of the firewall settings manager.
		hr = CoCreateInstance(
			__uuidof(NetFwMgr),
			NULL,
			CLSCTX_INPROC_SERVER,
			__uuidof(INetFwMgr),
			(void**)&fwMgr
			);
		if (FAILED(hr))
		{
			printf("CoCreateInstance failed: 0x%08lx\n", hr);
			goto error;
		}

		// Retrieve the local firewall policy.
		hr = fwMgr->get_LocalPolicy(&fwPolicy);
		if (FAILED(hr))
		{
			printf("get_LocalPolicy failed: 0x%08lx\n", hr);
			goto error;
		}

		// Retrieve the firewall profile currently in effect.
		hr = fwPolicy->get_CurrentProfile(fwProfile);
		if (FAILED(hr))
		{
			printf("get_CurrentProfile failed: 0x%08lx\n", hr);
			goto error;
		}

error:

		// Release the local firewall policy.
		if (fwPolicy != NULL)
		{
			fwPolicy->Release();
		}

		// Release the firewall settings manager.
		if (fwMgr != NULL)
		{
			fwMgr->Release();
		}

		return hr;
	}

	void WindowsFirewallCleanup(IN INetFwProfile* fwProfile)
	{
		// Release the firewall profile.
		if (fwProfile != NULL)
		{
			fwProfile->Release();
		}
	}

	HRESULT WindowsFirewallAppIsEnabled(
		IN INetFwProfile* fwProfile,
		IN const wchar_t* fwProcessImageFileName,
		OUT BOOL* fwAppEnabled
		)
	{
		HRESULT hr = S_OK;
		BSTR fwBstrProcessImageFileName = NULL;
		VARIANT_BOOL fwEnabled;
		INetFwAuthorizedApplication* fwApp = NULL;
		INetFwAuthorizedApplications* fwApps = NULL;

		_ASSERT(fwProfile != NULL);
		_ASSERT(fwProcessImageFileName != NULL);
		_ASSERT(fwAppEnabled != NULL);

		*fwAppEnabled = FALSE;

		// Retrieve the authorized application collection.
		hr = fwProfile->get_AuthorizedApplications(&fwApps);
		if (FAILED(hr))
		{
			printf("get_AuthorizedApplications failed: 0x%08lx\n", hr);
			goto error;
		}

		// Allocate a BSTR for the process image file name.
		fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
		if (SysStringLen(fwBstrProcessImageFileName) == 0)
		{
			hr = E_OUTOFMEMORY;
			printf("SysAllocString failed: 0x%08lx\n", hr);
			goto error;
		}

		// Attempt to retrieve the authorized application.
		hr = fwApps->Item(fwBstrProcessImageFileName, &fwApp);
		if (SUCCEEDED(hr))
		{
			// Find out if the authorized application is enabled.
			hr = fwApp->get_Enabled(&fwEnabled);
			if (FAILED(hr))
			{
				printf("get_Enabled failed: 0x%08lx\n", hr);
				goto error;
			}

			if (fwEnabled != VARIANT_FALSE)
			{
				// The authorized application is enabled.
				*fwAppEnabled = TRUE;

				printf(
					"Authorized application %lS is enabled in the firewall.\n",
					fwProcessImageFileName
					);
			}
			else
			{
				printf(
					"Authorized application %lS is disabled in the firewall.\n",
					fwProcessImageFileName
					);
			}
		}
		else
		{
			// The authorized application was not in the collection.
			hr = S_OK;

			printf(
				"Authorized application %lS is disabled in the firewall.\n",
				fwProcessImageFileName
				);
		}

error:

		// Free the BSTR.
		SysFreeString(fwBstrProcessImageFileName);

		// Release the authorized application instance.
		if (fwApp != NULL)
		{
			fwApp->Release();
		}

		// Release the authorized application collection.
		if (fwApps != NULL)
		{
			fwApps->Release();
		}

		return hr;
	}


	HRESULT WindowsFirewallAddApp(
		IN INetFwProfile* fwProfile,
		IN const wchar_t* fwProcessImageFileName,
		IN const wchar_t* fwName
		)
	{
		HRESULT hr = S_OK;
		BOOL fwAppEnabled;
		BSTR fwBstrName = NULL;
		BSTR fwBstrProcessImageFileName = NULL;
		INetFwAuthorizedApplication* fwApp = NULL;
		INetFwAuthorizedApplications* fwApps = NULL;

		_ASSERT(fwProfile != NULL);
		_ASSERT(fwProcessImageFileName != NULL);
		_ASSERT(fwName != NULL);

		// First check to see if the application is already authorized.
		hr = WindowsFirewallAppIsEnabled(
			fwProfile,
			fwProcessImageFileName,
			&fwAppEnabled
			);
		if (FAILED(hr))
		{
			printf("WindowsFirewallAppIsEnabled failed: 0x%08lx\n", hr);
			goto error;
		}

		// Only add the application if it isn't already authorized.
		if (!fwAppEnabled)
		{
			// Retrieve the authorized application collection.
			hr = fwProfile->get_AuthorizedApplications(&fwApps);
			if (FAILED(hr))
			{
				printf("get_AuthorizedApplications failed: 0x%08lx\n", hr);
				goto error;
			}

			// Create an instance of an authorized application.
			hr = CoCreateInstance(
				__uuidof(NetFwAuthorizedApplication),
				NULL,
				CLSCTX_INPROC_SERVER,
				__uuidof(INetFwAuthorizedApplication),
				(void**)&fwApp
				);
			if (FAILED(hr))
			{
				printf("CoCreateInstance failed: 0x%08lx\n", hr);
				goto error;
			}

			// Allocate a BSTR for the process image file name.
			fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
			if (SysStringLen(fwBstrProcessImageFileName) == 0)
			{
				hr = E_OUTOFMEMORY;
				printf("SysAllocString failed: 0x%08lx\n", hr);
				goto error;
			}

			// Set the process image file name.
			hr = fwApp->put_ProcessImageFileName(fwBstrProcessImageFileName);
			if (FAILED(hr))
			{
				printf("put_ProcessImageFileName failed: 0x%08lx\n", hr);
				goto error;
			}

			// Allocate a BSTR for the application friendly name.
			fwBstrName = SysAllocString(fwName);
			if (SysStringLen(fwBstrName) == 0)
			{
				hr = E_OUTOFMEMORY;
				printf("SysAllocString failed: 0x%08lx\n", hr);
				goto error;
			}

			// Set the application friendly name.
			hr = fwApp->put_Name(fwBstrName);
			if (FAILED(hr))
			{
				printf("put_Name failed: 0x%08lx\n", hr);
				goto error;
			}

			// Add the application to the collection.
			hr = fwApps->Add(fwApp);
			if (FAILED(hr))
			{
				printf("Add failed: 0x%08lx\n", hr);
				goto error;
			}

			printf(
				"Authorized application %lS is now enabled in the firewall.\n",
				fwProcessImageFileName
				);
		}

error:

		// Free the BSTRs.
		SysFreeString(fwBstrName);
		SysFreeString(fwBstrProcessImageFileName);

		// Release the authorized application instance.
		if (fwApp != NULL)
		{
			fwApp->Release();
		}

		// Release the authorized application collection.
		if (fwApps != NULL)
		{
			fwApps->Release();
		}

		return hr;
	}

}

bool ENABLE_FIREWALL()
{
	BOOL couldEnable = false;
	HRESULT hr = S_OK;
	HRESULT comInit = E_FAIL;
	INetFwProfile* fwProfile = NULL;

	// Initialize COM.
#if 1
	comInit = CoInitialize( 0 );
#else
	comInit = CoInitializeEx(
		0,
		COINIT_APARTMENTTHREADED //| COINIT_DISABLE_OLE1DDE
		);
#endif

	// Ignore RPC_E_CHANGED_MODE; this just means that COM has already been
	// initialized with a different mode. Since we don't care what the mode is,
	// we'll just use the existing mode.
	if (comInit != RPC_E_CHANGED_MODE) {
		hr = comInit;
		if (FAILED(hr)) {
			fprintf( stderr, "CoInitializeEx failed: 0x%08lx\n", hr );
			goto error;
		}
	}

	// Retrieve the firewall profile currently in effect.
	hr = WindowsFirewallInitialize(&fwProfile);
	if (FAILED(hr)) {
		fprintf( stderr, "WindowsFirewallInitialize failed: 0x%08lx\n", hr );
		goto error;
	}

	HMODULE hm = GetModuleHandle( 0 );
	wchar_t path[512];
	if( !GetModuleFileNameW( hm, path, sizeof(path)/sizeof(wchar_t) ) ) {
		fprintf( stderr, "GetModuleFileName() failed: 0x%lx\n", GetLastError() );
		goto error;
	}

	// Add the application to the authorized application collection.
	hr = WindowsFirewallAddApp(
		fwProfile,
		path,
		L"Introduction Library User"
		);
	if (FAILED(hr)) {
		fprintf( stderr, "WindowsFirewallAddApp failed: 0x%08lx\n", hr );
		goto error;
	}

error:

	WindowsFirewallCleanup(fwProfile);

	if (SUCCEEDED(comInit)) {
		CoUninitialize();
	}

	return couldEnable != FALSE;
}
*/

#else
#error "don't know how to do this"
#endif

#endif
