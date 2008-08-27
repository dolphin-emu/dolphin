//-----------------------------------------------------------------------------
// File: MultiDI.h
//
// Desc: Multiple user DirectInput support using action mapping
//
// Copyright (C) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------------------------
#ifndef MULTIDI_H
#define MULTIDI_H

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>

//-----------------------------------------------------------------------------
// Miscellaneous helper functions
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

HRESULT DXUtil_WriteStringRegKey( HKEY hKey, LPCTSTR strRegName, LPCTSTR strValue );
HRESULT DXUtil_ReadStringRegKeyCch( HKEY hKey, LPCTSTR strRegName, TCHAR* strDest, DWORD cchDest, LPCTSTR strDefault );
HRESULT DXUtil_ConvertGUIDToStringCch( const GUID* pGuidSrc, TCHAR* strDest, int cchDestChar );
HRESULT DXUtil_ConvertWideStringToGenericCch( TCHAR* tstrDestination, const WCHAR* wstrSource, int cchDestChar );

//-----------------------------------------------------------------------------
// Name: DXUtil_Timer()
// Desc: Performs timer opertations. Use the following commands:
//          TIMER_RESET           - to reset the timer
//          TIMER_START           - to start the timer
//          TIMER_STOP            - to stop (or pause) the timer
//          TIMER_ADVANCE         - to advance the timer by 0.1 seconds
//          TIMER_GETABSOLUTETIME - to get the absolute system time
//          TIMER_GETAPPTIME      - to get the current time
//          TIMER_GETELAPSEDTIME  - to get the time that elapsed between 
//                                  TIMER_GETELAPSEDTIME calls
//-----------------------------------------------------------------------------
enum TIMER_COMMAND { TIMER_RESET, TIMER_START, TIMER_STOP, TIMER_ADVANCE,
                     TIMER_GETABSOLUTETIME, TIMER_GETAPPTIME, TIMER_GETELAPSEDTIME };
FLOAT __stdcall DXUtil_Timer( TIMER_COMMAND command );

// E_DIUTILERR_PLAYERWITHOUTDEVICE is returned by the manager class after configuring
// device, and there's a player that hasn't been assigned a device.
#define E_DIUTILERR_PLAYERWITHOUTDEVICE MAKE_HRESULT(SEVERITY_ERROR,FACILITY_ITF,997)

// E_DIUTILERR_DEVICESTAKEN is returned by the manager class when one player
// on the machine has enough RECENT devices to prevent other players from 
// playing. This return code is needed because this sample attempts to give 
// all RECENT devices to that player.
#define E_DIUTILERR_DEVICESTAKEN MAKE_HRESULT(SEVERITY_ERROR,FACILITY_ITF,998) 

// E_DIUTILERR_TOOMANYUSERS is returned by the manager class when the number of 
// players exceeds the number of devices present on the system. For example, 
// if you ask for 4 players on a machine that only has a keyboard and mouse,
// you're 2 short of what you need. 
#define E_DIUTILERR_TOOMANYUSERS MAKE_HRESULT(SEVERITY_ERROR,FACILITY_ITF,999)



//-----------------------------------------------------------------------------
// Name: class CMultiplayerInputDeviceManager
// Desc: Input device manager using DX8 action mapping
//-----------------------------------------------------------------------------
class CMultiplayerInputDeviceManager
{
public:
    struct PlayerInfo
    {
        DWORD                dwPlayerIndex;         // 0-based player number 
        TCHAR                strPlayerName[MAX_PATH]; // player name
        DWORD                dwMaxDevices;          // max number of elements in pDevices array
        DWORD                dwNumDevices;          // current number of elements in pDevices array
        BOOL                 bFoundDeviceForPlayer; // if a device has been found for this player yet
    };
    
    struct DeviceInfo
    {
        LPDIRECTINPUTDEVICE8  pdidDevice;           // dinput device pointer
        PlayerInfo*           pPlayerInfo;          // Player who owns this device
        BOOL                  bRelativeAxis;        // TRUE if device is using relative axis
        BOOL                  bMapsPri1Actions;     // TRUE if device maps pri 1 actions
        BOOL                  bMapsPri2Actions;     // TRUE if device maps pri 2 actions
        LPVOID                pParam;               // app defined pointer assoicated with this device
        DIDEVICEINSTANCE      didi;                 // device instance info
    };
    
    typedef HRESULT (CALLBACK *LPDIMANAGERCALLBACK)(CMultiplayerInputDeviceManager::PlayerInfo* pPlayerInfo, CMultiplayerInputDeviceManager::DeviceInfo* pDeviceInfo, const DIDEVICEINSTANCE* pdidi, LPVOID);
    
private:
    BOOL                    m_bCleanupCOM;
    HWND                    m_hWnd;
    
    LPDIRECTINPUT8          m_pDI;
    DIACTIONFORMAT*         m_pdiaf;
    
    PlayerInfo**            m_pUsers;
    DWORD                   m_dwNumUsers;    

    DeviceInfo*             m_pDeviceList;
    DWORD                   m_dwNumDevices;
    DWORD                   m_dwMaxDevices;    
    
    LPDIMANAGERCALLBACK     m_AddDeviceCallback;
    LPVOID                  m_AddDeviceCallbackParam;
    
    TCHAR*                  m_strKey;
    HKEY                    m_hKey;
    
public:
    static BOOL CALLBACK StaticEnumSuitableDevicesCB( LPCDIDEVICEINSTANCE pdidi, LPDIRECTINPUTDEVICE8 pdidDevice, DWORD dwFlags, DWORD dwRemainingDevices, VOID* pContext );
    static BOOL CALLBACK StaticBuildDeviceListCB( LPCDIDEVICEINSTANCE pdidi, VOID* pContext );
    
    // Device control
    BOOL EnumDevice( const DIDEVICEINSTANCE* pdidi, LPDIRECTINPUTDEVICE8 pdidDevice, DWORD dwFlags, DWORD dwDeviceRemaining );
    BOOL BuildDeviceListCB( LPCDIDEVICEINSTANCE pdidi ); 
    
    HRESULT AddDevice( DeviceInfo* pDeviceInfo, BOOL bForceReset );
    HRESULT GetDevices( DeviceInfo** ppDeviceInfo, DWORD* pdwNumDevices );
    HRESULT ConfigureDevices( HWND hWnd, IUnknown* pSurface, VOID* pCallback, DWORD dwFlags, LPVOID pvCBParam );
    DWORD   GetNumDevices() { return m_dwNumDevices; }
    VOID    UnacquireDevices();
    VOID    SetFocus( HWND hWnd );
    PlayerInfo* LookupPlayer( TCHAR* strPlayerName );
    HRESULT SaveDeviceOwnershipKeys();
    VOID    DeleteDeviceOwnershipKeys();
    HRESULT UpdateDeviceOwnership();
    HRESULT AssignDevices();
    HRESULT VerifyAssignment();
    HRESULT AddAssignedDevices( BOOL bResetMappings );
    
    HRESULT BuildDeviceList();
    VOID    CleanupDeviceList();
    
    // Construction
    HRESULT SetActionFormat( DIACTIONFORMAT* pdiaf, BOOL bReenumerate, BOOL bResetOwnership, BOOL bResetMappings );
    HRESULT Create( HWND hWnd, TCHAR* strUserNames[], DWORD dwNumUsers, DIACTIONFORMAT* pdiaf, LPDIMANAGERCALLBACK AddDeviceCallback, LPVOID pCallbackParam, BOOL bResetOwnership, BOOL bResetMappings );

    CMultiplayerInputDeviceManager( TCHAR* strRegKey );
    ~CMultiplayerInputDeviceManager();
    VOID Cleanup();
};

#endif