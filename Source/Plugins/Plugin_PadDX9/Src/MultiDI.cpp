//-----------------------------------------------------------------------------
// File: MultiDI.cpp
//
// Desc: DirectInput framework class using semantic mapping with multiplayer
//       device ownership.  Feel free to use this class as a starting point 
//       for adding extra functionality.
//
// Copyright (C) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------------------------
#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#include "stdafx.h"

#include <basetsd.h>
#include <tchar.h>
#include <stdio.h>
#include <windows.h>
#include <dxerr9.h>
#include <d3d9types.h>  // included to get the D3DCOLOR_RGBA macro.
#include "MultiDI.h"
#include <assert.h>




//-----------------------------------------------------------------------------
// Name: CMultiplayerInputDeviceManager
// Desc: Constructor
// Args: strRegKey - A location in the registry where device ownership 
//       information should be stored
//-----------------------------------------------------------------------------
CMultiplayerInputDeviceManager::CMultiplayerInputDeviceManager( TCHAR* strRegKey )
{
    HRESULT hr = CoInitialize(NULL);
    m_bCleanupCOM = SUCCEEDED(hr);
    LONG nResult;

    // Initialize members
    m_pDI                       = NULL;
    m_hWnd                      = NULL;
    m_pdiaf                     = NULL;
    m_pUsers                    = NULL;
    m_pDeviceList               = NULL;
    m_AddDeviceCallback         = NULL;
    m_AddDeviceCallbackParam    = NULL;
    m_hKey                      = NULL;
    m_dwNumDevices              = 0;
    m_dwMaxDevices              = 0; 
    

    // Duplicate the registry location string since we'll need this again later
    m_strKey = _tcsdup( strRegKey );
    if( m_strKey == NULL )
        return;

    // Create a reg key to store device ownership data 
    nResult = RegCreateKeyEx( HKEY_CURRENT_USER, strRegKey, 0, NULL,
                              REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, 
                              &m_hKey, NULL );
    if(nResult != ERROR_SUCCESS)
        m_hKey = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CMultiplayerInputDeviceManager
// Desc: Destructor
//-----------------------------------------------------------------------------
CMultiplayerInputDeviceManager::~CMultiplayerInputDeviceManager()
{
    Cleanup();
    
    if( m_bCleanupCOM )
        CoUninitialize();

    RegCloseKey( m_hKey );

    free( m_strKey );
}




//-----------------------------------------------------------------------------
// Name: Create
// Desc: Initializes the class, and enums the devices.  See MultiMapper sample
//       for how to use this class.
//       It might fail if there are too many players for the 
//       number of devices availible, or if one player owns too many
//       devices preventing others from having a device.  Its up the app
//       to prevent this or respond to this.
//       Note: strUserName should be a array of sz strings 
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::Create( HWND hWnd, 
                                     TCHAR* strUserNames[], 
                                     DWORD dwNumUsers,
                                     DIACTIONFORMAT* pdiaf,
                                     LPDIMANAGERCALLBACK AddDeviceCallback, 
                                     LPVOID pCallbackParam,
                                     BOOL bResetOwnership, 
                                     BOOL bResetMappings )
{
    HRESULT hr;
    
    if( strUserNames == NULL || dwNumUsers == 0 )
        return E_INVALIDARG;

    Cleanup();
    
    // Store data
    m_hWnd = hWnd;

    // Create and init the m_pUsers array 
    m_dwNumUsers = dwNumUsers;
    m_pUsers     = new PlayerInfo*[dwNumUsers];
    for( DWORD i=0; i<dwNumUsers; i++ )
    {
        m_pUsers[i] = new PlayerInfo;
        ZeroMemory( m_pUsers[i], sizeof(PlayerInfo) );        
        m_pUsers[i]->dwPlayerIndex = i; // set the 0-based player index (for easy referencing)
        lstrcpyn( m_pUsers[i]->strPlayerName, strUserNames[i], MAX_PATH-1 );            
    }
    
    m_AddDeviceCallback = AddDeviceCallback;
    m_AddDeviceCallbackParam = pCallbackParam;

    // Create the base DirectInput object
    if( FAILED( hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, 
                                         IID_IDirectInput8, (VOID**)&m_pDI, NULL ) ) )
        return DXTRACE_ERR( TEXT("DirectInput8Create"), hr );
    
    if( FAILED( hr = SetActionFormat( pdiaf, TRUE, bResetOwnership, bResetMappings ) ) )
        return DXTRACE_ERR( TEXT("SetActionFormat"), hr );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetActionFormat
// Desc: Sets a new action format.  
//       It re-enumerates the devices if bReenumerate
//       It resets the ownership of the devices if bResetOwnership
//       It resets the mapper actions of the devices if bResetMappings
//       This function may fail if there are too many players for the 
//       number of devices availible, or if one player owns too many
//       devices preventing others from having a device.  Its up the app
//       to prevent this or respond to this.
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::SetActionFormat( DIACTIONFORMAT* pdiaf, 
                                              BOOL bReenumerate, 
                                              BOOL bResetOwnership,
                                              BOOL bResetMappings )
{
    HRESULT hr;
    DWORD iPlayer;
    DWORD iDevice;

    // Store the new action format
    m_pdiaf = pdiaf;
    
    // Only destroy and re-enumerate devices if the caller explicitly wants to.
    // This isn't thread safe, so be sure not to have any other threads using
    // this data unless you redesign this class
    if( bReenumerate )
    {
        // Set all players to not have a device yet
        for( iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
            m_pUsers[iPlayer]->bFoundDeviceForPlayer = FALSE;

        if( bResetOwnership )
        {
            // Set all devices as not assigned to a player
            for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
                m_pDeviceList[iDevice].pPlayerInfo = NULL;

            // Delete the device ownership keys
            DeleteDeviceOwnershipKeys();
        }

        // Devices must be unacquired to have a new action map set.
        UnacquireDevices();
        
        // Enumerate all available devices that map the pri 1 actions,
        // and store them in the m_pDeviceList array
        if( FAILED( hr = BuildDeviceList() ) )
            return DXTRACE_ERR( TEXT("BuildDeviceList"), hr );
        
        // Assign devices to any players that don't have devices
        // Some games may want to change this functionality.
        if( FAILED( hr = AssignDevices() ) )
            return DXTRACE_ERR( TEXT("AssignDevices"), hr );

        // Report an error if there are too many players for the 
        // number of devices availible, or if one player owns too many
        // devices preventing others from having a device
        if( FAILED( hr = VerifyAssignment() ) )
            return hr;

        // Now that every player has at least one device, build and set 
        // the action map, and use callback into the app to tell the 
        // app of the device assignment.
        if( FAILED( hr = AddAssignedDevices( bResetMappings ) ) )
            return DXTRACE_ERR( TEXT("AddAssignedDevices"), hr );

        // For every device that's assigned to a player, save its device key 
        // and assigned player to registry, so the app remembers which devices
        // each player prefers
        if( FAILED( hr = SaveDeviceOwnershipKeys() ) )
            return DXTRACE_ERR( TEXT("SaveDeviceOwnershipKeys"), hr );
    }
    else
    {
        // Just apply the new maps for each device owned by each user
        
        // Devices must be unacquired to have a new action map set.
        UnacquireDevices();
        
        // Apply the new action map to the current devices.
        for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
        {
            LPDIRECTINPUTDEVICE8 pdidDevice = m_pDeviceList[iDevice].pdidDevice;
            PlayerInfo* pPlayerInfo = m_pDeviceList[iDevice].pPlayerInfo;

            if( FAILED( hr = pdidDevice->BuildActionMap( m_pdiaf, pPlayerInfo->strPlayerName, DIDBAM_DEFAULT ) ) )
                return DXTRACE_ERR( TEXT("BuildActionMap"), hr );
            if( FAILED( hr = pdidDevice->SetActionMap( m_pdiaf, pPlayerInfo->strPlayerName, DIDSAM_DEFAULT ) ) )
                return DXTRACE_ERR( TEXT("SetActionMap"), hr );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: BuildDeviceList
// Desc:
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::BuildDeviceList()
{
    // Cleanup any previously enumerated devices
    CleanupDeviceList();
    
    // Build a simple list of all devices currently attached to the machine. This
    // array will be used to reassign devices to each user.
    m_dwMaxDevices = 5;
    m_dwNumDevices = 0;
    m_pDeviceList = NULL;

    DeviceInfo* pListNew = NULL;
    pListNew = (DeviceInfo*) realloc( m_pDeviceList, m_dwMaxDevices*sizeof(DeviceInfo) );
    
    // Verify allocation
    if( NULL == pListNew )
        return DXTRACE_ERR( TEXT("BuildDeviceList"), E_OUTOFMEMORY );
    else
        m_pDeviceList = pListNew;

     ZeroMemory( m_pDeviceList, m_dwMaxDevices*sizeof(DeviceInfo) );        

    // Enumerate available devices for any user.
    HRESULT rs = m_pDI->EnumDevicesBySemantics( NULL, m_pdiaf, StaticEnumSuitableDevicesCB, 
                                   this, DIEDBSFL_ATTACHEDONLY );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: EnumSuitableDevicesCB
// Desc: DirectInput device enumeratation callback.  Calls AddDevice()
//       on each device enumerated.
//-----------------------------------------------------------------------------
BOOL CALLBACK CMultiplayerInputDeviceManager::StaticEnumSuitableDevicesCB( LPCDIDEVICEINSTANCE pdidi, 
                                                               LPDIRECTINPUTDEVICE8 pdidDevice, 
                                                               DWORD dwFlags, DWORD dwDeviceRemaining,
                                                               VOID* pContext )
{
    // Add the device to the device manager's internal list
    CMultiplayerInputDeviceManager* pInputDeviceManager = (CMultiplayerInputDeviceManager*)pContext;
    return pInputDeviceManager->EnumDevice( pdidi, pdidDevice, 
                                            dwFlags, dwDeviceRemaining );    
}




//-----------------------------------------------------------------------------
// Name: EnumDevice
// Desc: Enums each device to see if its suitable to add 
//-----------------------------------------------------------------------------
BOOL CMultiplayerInputDeviceManager::EnumDevice( const DIDEVICEINSTANCE* pdidi, 
                                     const LPDIRECTINPUTDEVICE8 pdidDevice,
                                     DWORD dwFlags, DWORD dwRemainingDevices )
{    
    TCHAR strPlayerName[MAX_PATH];
    TCHAR strDeviceGuid[40];
    
    // Devices of type DI8DEVTYPE_DEVICECTRL are specialized devices not generally
    // considered appropriate to control game actions. We just ignore these.
    if( GET_DIDEVICE_TYPE(pdidi->dwDevType) != DI8DEVTYPE_DEVICECTRL )
    {
        // We're only interested in devices that map the pri 1 actions
        if( dwFlags & DIEDBS_MAPPEDPRI1 )
        {
            // Add new pdidDevice struct to array, and resize array if needed
            m_dwNumDevices++;
            if( m_dwNumDevices > m_dwMaxDevices )
            {
                m_dwMaxDevices += 5;

                DeviceInfo* pListNew = NULL;
                pListNew = (DeviceInfo*) realloc( m_pDeviceList, m_dwMaxDevices*sizeof(DeviceInfo) );
    
                // Verify allocation
                if( NULL == pListNew )
                {
                    DXTRACE_ERR( TEXT("EnumDevice"), E_OUTOFMEMORY );
                    return DIENUM_STOP;
                }
                else
                    m_pDeviceList = pListNew;

                ZeroMemory( m_pDeviceList + m_dwMaxDevices - 5, 5*sizeof(DeviceInfo) );
            }
            
            DXUtil_ConvertGUIDToStringCch( &pdidi->guidInstance, strDeviceGuid, 40 );
            DXUtil_ReadStringRegKeyCch( m_hKey, strDeviceGuid, strPlayerName, MAX_PATH, TEXT("") );        
            
            // Add the device to the array m_pDeviceList
            DWORD dwCurrentDevice = m_dwNumDevices-1;
            ZeroMemory( &m_pDeviceList[dwCurrentDevice], sizeof(DeviceInfo) );

            m_pDeviceList[dwCurrentDevice].didi = *pdidi;            
            m_pDeviceList[dwCurrentDevice].pdidDevice = pdidDevice;        
            m_pDeviceList[dwCurrentDevice].pdidDevice->AddRef();
            m_pDeviceList[dwCurrentDevice].bMapsPri1Actions = ((dwFlags & DIEDBS_MAPPEDPRI1) == DIEDBS_MAPPEDPRI1);
            m_pDeviceList[dwCurrentDevice].bMapsPri2Actions = ((dwFlags & DIEDBS_MAPPEDPRI2) == DIEDBS_MAPPEDPRI2);
            
            if( lstrcmp( strPlayerName, TEXT("") ) != 0 )
            {
                m_pDeviceList[dwCurrentDevice].pPlayerInfo = LookupPlayer( strPlayerName );
                if( m_pDeviceList[dwCurrentDevice].pPlayerInfo )
                    m_pDeviceList[dwCurrentDevice].pPlayerInfo->bFoundDeviceForPlayer = TRUE;
            }
        }
    }
    
    // Continue enumerating 
    return DIENUM_CONTINUE;
}




//-----------------------------------------------------------------------------
// Name: AssignDevices
// Desc:
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::AssignDevices()
{    
    DWORD iDevice;
    DWORD iPlayer;

    // For any device that doesn't have a user assigned to it,
    // then assign it to the first user that needs a device
    for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
    {
        if( m_pDeviceList[iDevice].pPlayerInfo == NULL )
        {
            for( iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
            {
                if( !m_pUsers[iPlayer]->bFoundDeviceForPlayer )
                {
                    m_pDeviceList[iDevice].pPlayerInfo = m_pUsers[iPlayer];
                    m_pUsers[iPlayer]->bFoundDeviceForPlayer = TRUE;
                    break;
                }
            }                        
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: VerifyAssignment
// Desc:
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::VerifyAssignment()
{    
    DWORD iPlayer;
    DWORD iDevice;

    // For each player, make sure that a device was found for this 
    // player, otherwise return failure.
    for( iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
    {
        if( !m_pUsers[iPlayer]->bFoundDeviceForPlayer )
        {
            if( GetNumDevices() < m_dwNumUsers )
                return E_DIUTILERR_TOOMANYUSERS;
            else
            {
                // Check to see if there's a device that isn't already
                // assigned to a player. If there is return 
                // E_DIUTILERR_PLAYERWITHOUTDEVICE otherwise return 
                // E_DIUTILERR_DEVICESTAKEN
                for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
                {
                    if( m_pDeviceList[iDevice].pPlayerInfo == NULL )
                        return E_DIUTILERR_PLAYERWITHOUTDEVICE;
                }
                
                return E_DIUTILERR_DEVICESTAKEN;
            }
        }                       
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: AddAssignedDevices
// Desc: For every device that's assigned to a player, set it action map
//       and add it to the game
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::AddAssignedDevices( BOOL bResetMappings )
{    
    DWORD iDevice;
    DWORD iAction;
    HRESULT hr = S_OK;
    DWORD *pdwAppFixed = NULL;

    // If flagged, we'll be remapping the actions to hardware defaults, and in
    // the process DirectInput will clear the DIA_APPFIXED flag, so we need 
    // to make a copy in order to reapply the flag later.
    if( bResetMappings )
    {
        pdwAppFixed = new DWORD[m_pdiaf->dwNumActions];
        
        // Verify memory allocation and collect DIA_APPFIXED settings
        if( pdwAppFixed )
        {
            for( iAction=0; iAction < m_pdiaf->dwNumActions; iAction++ )
                pdwAppFixed[iAction] = m_pdiaf->rgoAction[iAction].dwFlags & DIA_APPFIXED;
        }
    }

    // For every device that's assigned to a player, 
    // set it action map, and add it to the game
    for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
    {                
        LPDIRECTINPUTDEVICE8 pdidDevice = m_pDeviceList[iDevice].pdidDevice;
        PlayerInfo*          pPlayerInfo = m_pDeviceList[iDevice].pPlayerInfo;           
        
        if( pPlayerInfo != NULL )
        {   
            // Set the device's coop level
            hr = pdidDevice->SetCooperativeLevel( m_hWnd, DISCL_NONEXCLUSIVE|DISCL_FOREGROUND );
            if( FAILED(hr) )
                break;
            
            // Build and set the action map on this device.  This will also remove 
            // it from DirectInput's internal list of available devices.
            DWORD dwBuildFlags = bResetMappings ? DIDBAM_HWDEFAULTS : DIDBAM_DEFAULT;   
          	DWORD dwSetFlags = bResetMappings ? DIDSAM_FORCESAVE : DIDSAM_DEFAULT;

            hr = pdidDevice->BuildActionMap( m_pdiaf, pPlayerInfo->strPlayerName, dwBuildFlags );
            if( FAILED( hr ) )
            {
                // just print out a debug message and keep going
                DXTRACE_ERR( TEXT("BuildActionMap"), hr );
                hr = S_OK;
                continue;
            }

            hr = pdidDevice->SetActionMap( m_pdiaf, pPlayerInfo->strPlayerName, dwSetFlags );
            if( FAILED( hr ) )
            {
                // just print out a debug message and keep going
                DXTRACE_ERR( TEXT("SetActionMap"), hr );   
                hr = S_OK;
                continue;
            }
            

            // Callback into the app so it can adjust the device and set
            // the m_pDeviceList[iDevice].pParam field with a device state struct
            if( m_AddDeviceCallback )
                m_AddDeviceCallback( pPlayerInfo, &m_pDeviceList[iDevice], 
                                     &m_pDeviceList[iDevice].didi, m_AddDeviceCallbackParam );            
            
            // Check to see if the device is using relative axis -- sometimes app code
            // might want to know this.
            DIPROPDWORD dipdw;
            dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
            dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            dipdw.diph.dwObj        = 0;
            dipdw.diph.dwHow        = DIPH_DEVICE;
            dipdw.dwData            = 0;
            pdidDevice->GetProperty( DIPROP_AXISMODE, &dipdw.diph );
            if( dipdw.dwData == DIPROPAXISMODE_REL )
                m_pDeviceList[iDevice].bRelativeAxis = TRUE;   
            
            // We made it through this iteration without breaking out do to errors
            hr = S_OK;
        }
        else
        {
            if( FAILED( hr = pdidDevice->BuildActionMap( m_pdiaf, NULL, DIDBAM_DEFAULT ) ) )
            {
                DXTRACE_ERR( TEXT("BuildActionMap"), hr );
                break;
            }
            
            if( FAILED( hr = pdidDevice->SetActionMap( m_pdiaf, NULL, DIDSAM_NOUSER ) ) )
            {
                DXTRACE_ERR( TEXT("SetActionMap"), hr );   
                break;
            }
        }
    }                              

    // If we stored DIA_APPFIXED flags earlier, we need to reapply those flags and
    // free the allocated memory
    if( bResetMappings && pdwAppFixed )
    {
        for( iAction=0; iAction < m_pdiaf->dwNumActions; iAction++ )
            m_pdiaf->rgoAction[iAction].dwFlags |= pdwAppFixed[iAction];
        
        delete [] pdwAppFixed;
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: ConfigureDevices
// Desc:
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::ConfigureDevices( HWND hWnd, IUnknown* pSurface,
                                               VOID* ConfigureDevicesCB,
                                               DWORD dwFlags, LPVOID pvCBParam )
{
    HRESULT hr;
    DWORD iPlayer;
    
    // Determine how large of a string we'll need to hold all user names
    DWORD dwNamesSize = 0;
    for( iPlayer=0; iPlayer < m_dwNumUsers; iPlayer++ )
        dwNamesSize += lstrlen( m_pUsers[iPlayer]->strPlayerName ) +1;

    // Build multi-sz list of user names
    TCHAR* strUserNames = new TCHAR[dwNamesSize+1];
    
    // Verify allocation and cycle through user names
    if( strUserNames ) 
    {
        TCHAR* strTemp = strUserNames;
        for( iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
        {
            lstrcpy( strTemp, m_pUsers[iPlayer]->strPlayerName );
            strTemp += lstrlen(strTemp) + 1;
        }

        lstrcpy( strTemp, TEXT("\0") );
    }

    
    // Fill in all the params
    DICONFIGUREDEVICESPARAMS dicdp;
    ZeroMemory(&dicdp, sizeof(dicdp));
    dicdp.dwSize = sizeof(dicdp);
    dicdp.dwcFormats     = 1;
    dicdp.lprgFormats    = m_pdiaf;
    dicdp.hwnd           = hWnd;
    dicdp.lpUnkDDSTarget = pSurface;
    dicdp.dwcUsers       = m_dwNumUsers;
    dicdp.lptszUserNames = strUserNames;

    // Initialize all the colors here
    DICOLORSET dics;
    ZeroMemory(&dics, sizeof(DICOLORSET));
    dics.dwSize = sizeof(DICOLORSET);

    // Set UI color scheme (if not specified it uses defaults)
    dicdp.dics.dwSize = sizeof(dics);
    dicdp.dics.cTextFore        = D3DCOLOR_RGBA(255,255,255,255);
    dicdp.dics.cTextHighlight   = D3DCOLOR_RGBA(204,204,255,255);
    dicdp.dics.cCalloutLine     = D3DCOLOR_RGBA(255,255,255,128); //
    dicdp.dics.cCalloutHighlight= D3DCOLOR_RGBA(204,204,255,255);
    dicdp.dics.cBorder          = D3DCOLOR_RGBA(153,153,204,128);
    dicdp.dics.cControlFill     = D3DCOLOR_RGBA( 51,  51, 102, 128); //
    dicdp.dics.cHighlightFill   = D3DCOLOR_RGBA(0,0,0,128);
    dicdp.dics.cAreaFill        = D3DCOLOR_RGBA(0,0,50,128);
        
    if( dwFlags & DICD_EDIT )
    {
        // Re-enum so we can catch any new devices that have been recently attached
        for( iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
            m_pUsers[iPlayer]->bFoundDeviceForPlayer = FALSE;
        if( FAILED( hr = BuildDeviceList() ) )
        {
            DXTRACE_ERR( TEXT("BuildDeviceList"), hr );
            goto LCleanup;
        }
    }
        
    // Unacquire the devices so that mouse doesn't 
    // control the game while in control panel
    UnacquireDevices();

    if( FAILED( hr = m_pDI->ConfigureDevices( (LPDICONFIGUREDEVICESCALLBACK)ConfigureDevicesCB, 
                                  &dicdp, dwFlags, pvCBParam ) ) )
    {
        DXTRACE_ERR( TEXT("ConfigureDevices"), hr );   
        goto LCleanup;
    }

    if( dwFlags & DICD_EDIT )
    {
        // Update the device ownership 
        if( FAILED( hr = UpdateDeviceOwnership() ) )
        {
            DXTRACE_ERR( TEXT("UpdateDeviceOwnership"), hr );    
            goto LCleanup;
        }

        // Now save the device keys that are assigned players to registry, 
        if( FAILED( hr = SaveDeviceOwnershipKeys() ) )
        {
            DXTRACE_ERR( TEXT("SaveDeviceOwnershipKeys"), hr );
            goto LCleanup;
        }

        // Report an error if there is a player that doesn't not
        // have a device assigned
        if( FAILED( hr = VerifyAssignment() ) )
            goto LCleanup;
        
        // Now that every player has at least one device, build and set 
        // the action map, and use callback into the app to tell the 
        // app of the device assignment.
        if( FAILED( hr = AddAssignedDevices( FALSE ) ) )
        {
            DXTRACE_ERR( TEXT("AddAssignedDevices"), hr );
            goto LCleanup;
        }
    }
    
    hr = S_OK;

LCleanup:

    if( strUserNames )
        delete [] strUserNames;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: UpdateDeviceOwnership
// Desc:
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::UpdateDeviceOwnership()
{
    DWORD   iPlayer;
    DWORD   iDevice;

    // Set all players to not have a device yet
    for( iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
        m_pUsers[iPlayer]->bFoundDeviceForPlayer = FALSE;

    // Set all devices as not assigned to a player
    for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
        m_pDeviceList[iDevice].pPlayerInfo = NULL;

    UnacquireDevices();
    
    // Update the device ownership by quering the DIPROP_USERNAME property
    for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
    {
        LPDIRECTINPUTDEVICE8 pdidDevice = m_pDeviceList[iDevice].pdidDevice;

        TCHAR strPlayerName[MAX_PATH];
        DIPROPSTRING dips;
        dips.diph.dwSize       = sizeof(DIPROPSTRING); 
        dips.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        dips.diph.dwObj        = 0; // device property 
        dips.diph.dwHow        = DIPH_DEVICE;                     
        pdidDevice->GetProperty( DIPROP_USERNAME, &dips.diph );                    
        DXUtil_ConvertWideStringToGenericCch( strPlayerName, dips.wsz, MAX_PATH );                    

        if( lstrcmp( strPlayerName, TEXT("") ) != 0 )
        {
            m_pDeviceList[iDevice].pPlayerInfo = LookupPlayer( strPlayerName );
            if( m_pDeviceList[iDevice].pPlayerInfo )
                m_pDeviceList[iDevice].pPlayerInfo->bFoundDeviceForPlayer = TRUE;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UnacquireDevices
// Desc:
//-----------------------------------------------------------------------------
VOID CMultiplayerInputDeviceManager::UnacquireDevices()
{
    // Unacquire every device

    if( m_pDeviceList )
    {
        // All devices have been assigned a to a user in 
        // the new array, so clean up the local array
        for( DWORD iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
        {
            LPDIRECTINPUTDEVICE8 pdidDevice = m_pDeviceList[iDevice].pdidDevice;
            
            // Set the device's coop level
            pdidDevice->Unacquire();
        }
    }
}




//-----------------------------------------------------------------------------
// Name: SetFocus
// Desc: Sets the DirectInput focus to a new HWND
//-----------------------------------------------------------------------------
VOID CMultiplayerInputDeviceManager::SetFocus( HWND hWnd ) 
{
    m_hWnd = hWnd;
    
    if( m_pDeviceList )
    {
        // All devices have been assigned a to a user in 
        // the new array, so clean up the local array
        for( DWORD iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
        {
            LPDIRECTINPUTDEVICE8 pdidDevice = m_pDeviceList[iDevice].pdidDevice;

            // Set the device's coop level
            pdidDevice->Unacquire();
            pdidDevice->SetCooperativeLevel( m_hWnd, DISCL_NONEXCLUSIVE|DISCL_FOREGROUND );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: GetDevices
// Desc: returns an array of DeviceInfo*'s
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::GetDevices( DeviceInfo** ppDeviceInfo, 
                                         DWORD* pdwCount )
{
    if( NULL==ppDeviceInfo || NULL==pdwCount )
        return E_INVALIDARG;
    
    (*ppDeviceInfo) = m_pDeviceList;
    (*pdwCount)     = m_dwNumDevices;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LookupPlayer
// Desc: searchs m_pUsers by player name
//-----------------------------------------------------------------------------
CMultiplayerInputDeviceManager::PlayerInfo* CMultiplayerInputDeviceManager::LookupPlayer( TCHAR* strPlayerName )
{
    for( DWORD iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
    {
        PlayerInfo* pCurPlayer = m_pUsers[iPlayer];
        if( lstrcmp( pCurPlayer->strPlayerName, strPlayerName ) == 0 )
            return pCurPlayer;
    }
    
    return NULL;
}




//-----------------------------------------------------------------------------
// Name: SaveDeviceOwnershipKeys
// Desc: For every device that's assigned to a player, save its device key 
//       and assigned player to registry.
//-----------------------------------------------------------------------------
HRESULT CMultiplayerInputDeviceManager::SaveDeviceOwnershipKeys()
{
    TCHAR strDeviceGuid[40];
    DWORD iDevice;

    for( iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
    {                
        PlayerInfo* pPlayerInfo = m_pDeviceList[iDevice].pPlayerInfo;           

        DXUtil_ConvertGUIDToStringCch( &m_pDeviceList[iDevice].didi.guidInstance, strDeviceGuid, 40 );

        if( pPlayerInfo != NULL )
            DXUtil_WriteStringRegKey( m_hKey, strDeviceGuid, pPlayerInfo->strPlayerName );        
        else
            RegDeleteValue( m_hKey, strDeviceGuid );
    }                              

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DeleteDeviceOwnershipKeys
// Desc: Delete all the ownership keys
//-----------------------------------------------------------------------------
VOID CMultiplayerInputDeviceManager::DeleteDeviceOwnershipKeys()
{
    HKEY hKey;
    TCHAR *strRegKey;

    // Prepare strings to delete the key
    strRegKey = _tcsdup( m_strKey );
    if( strRegKey == NULL )
        return;

    TCHAR* strTemp = _tcsrchr( strRegKey, TEXT('\\') );
    
    // Unless the registry path string was malformed, we're ready to delete
    // and recreate the key
    if( strTemp ) 
    {
        *strTemp = 0;
        strTemp++;

        RegCloseKey( m_hKey );

        // Delete the reg key
        RegOpenKey( HKEY_CURRENT_USER, strRegKey, &hKey );
        RegDeleteKey( hKey, strTemp );
        RegCloseKey( hKey );
    
        // Create the key again now that all the subkeys have been deleted
        RegCreateKeyEx( HKEY_CURRENT_USER, m_strKey, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, 
                        &m_hKey, NULL );
    }


    // Clean up memory allocation
    free( strRegKey );
}




//-----------------------------------------------------------------------------
// Name: Cleanup
// Desc:
//-----------------------------------------------------------------------------
VOID CMultiplayerInputDeviceManager::Cleanup()
{
    CleanupDeviceList();
    
    if( m_pUsers )
    {
        for( DWORD iPlayer=0; iPlayer<m_dwNumUsers; iPlayer++ )
            SAFE_DELETE( m_pUsers[iPlayer] );
        SAFE_DELETE( m_pUsers );
    }
    
    // Release() base object
    SAFE_RELEASE( m_pDI );
    
}
    



//-----------------------------------------------------------------------------
// Name: CleanupDeviceList
// Desc: Clean up the device array
//-----------------------------------------------------------------------------
VOID CMultiplayerInputDeviceManager::CleanupDeviceList()
{
    for( DWORD iDevice=0; iDevice<m_dwNumDevices; iDevice++ )
        SAFE_RELEASE( m_pDeviceList[iDevice].pdidDevice );
    free( m_pDeviceList );
    m_pDeviceList = NULL;
    m_dwMaxDevices = 0;
    m_dwNumDevices = 0;    
}







//-----------------------------------------------------------------------------
// Name: DXUtil_ConvertGUIDToStringCch()
// Desc: Converts a GUID to a string 
//       cchDestChar is the size in TCHARs of strDest.  Be careful not to 
//       pass in sizeof(strDest) on UNICODE builds
//-----------------------------------------------------------------------------
HRESULT DXUtil_ConvertGUIDToStringCch( const GUID* pGuidSrc, TCHAR* strDest, int cchDestChar )
{
	int nResult = _sntprintf( strDest, cchDestChar, TEXT("{%0.8X-%0.4X-%0.4X-%0.2X%0.2X-%0.2X%0.2X%0.2X%0.2X%0.2X%0.2X}"),
		pGuidSrc->Data1, pGuidSrc->Data2, pGuidSrc->Data3,
		pGuidSrc->Data4[0], pGuidSrc->Data4[1],
		pGuidSrc->Data4[2], pGuidSrc->Data4[3],
		pGuidSrc->Data4[4], pGuidSrc->Data4[5],
		pGuidSrc->Data4[6], pGuidSrc->Data4[7] );

	if( nResult < 0 )
		return E_FAIL;
	return S_OK;
}


//-----------------------------------------------------------------------------
// Name: DXUtil_ConvertWideStringToAnsi()
// Desc: This is a UNICODE conversion utility to convert a WCHAR string into a
//       char string.
//       cchDestChar is the size in bytes of strDestination
//-----------------------------------------------------------------------------
HRESULT DXUtil_ConvertWideStringToAnsiCch( char* strDestination, const WCHAR* wstrSource,
										  int cchDestChar )
{
	if( strDestination==NULL || wstrSource==NULL || cchDestChar < 1 )
		return E_INVALIDARG;

	int nResult = WideCharToMultiByte( CP_ACP, 0, wstrSource, -1, strDestination, 
		cchDestChar, NULL, NULL );
	strDestination[cchDestChar-1] = 0;

	if( nResult == 0 )
		return E_FAIL;
	return S_OK;
}


//-----------------------------------------------------------------------------
// Name: DXUtil_ConvertAnsiStringToGeneric()
// Desc: This is a UNICODE conversion utility to convert a WCHAR string into a
//       TCHAR string. 
//       cchDestChar is the size in TCHARs of tstrDestination.  Be careful not to 
//       pass in sizeof(strDest) on UNICODE builds
//-----------------------------------------------------------------------------
HRESULT DXUtil_ConvertWideStringToGenericCch( TCHAR* tstrDestination, const WCHAR* wstrSource, 
											 int cchDestChar )
{
	if( tstrDestination==NULL || wstrSource==NULL || cchDestChar < 1 )
		return E_INVALIDARG;

#ifdef _UNICODE
	wcsncpy( tstrDestination, wstrSource, cchDestChar );
	tstrDestination[cchDestChar-1] = L'\0';    
	return S_OK;
#else
	return DXUtil_ConvertWideStringToAnsiCch( tstrDestination, wstrSource, cchDestChar );
#endif
}



//-----------------------------------------------------------------------------
// Name: DXUtil_ReadStringRegKeyCch()
// Desc: Helper function to read a registry key string
//       cchDest is the size in TCHARs of strDest.  Be careful not to 
//       pass in sizeof(strDest) on UNICODE builds.
//-----------------------------------------------------------------------------
HRESULT DXUtil_ReadStringRegKeyCch( HKEY hKey, LPCTSTR strRegName, TCHAR* strDest, 
								   DWORD cchDest, LPCTSTR strDefault )
{
	DWORD dwType;
	DWORD cbDest = cchDest * sizeof(TCHAR);

	if( ERROR_SUCCESS != RegQueryValueEx( hKey, strRegName, 0, &dwType, 
		(BYTE*)strDest, &cbDest ) )
	{
		_tcsncpy( strDest, strDefault, cchDest );
		strDest[cchDest-1] = 0;
		return S_FALSE;
	}
	else
	{     
		if( dwType != REG_SZ )
		{
			_tcsncpy( strDest, strDefault, cchDest );
			strDest[cchDest-1] = 0;
			return S_FALSE;
		}   
	}

	return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DXUtil_WriteStringRegKey()
// Desc: Helper function to write a registry key string
//-----------------------------------------------------------------------------
HRESULT DXUtil_WriteStringRegKey( HKEY hKey, LPCTSTR strRegName,
								 LPCTSTR strValue )
{
	if( NULL == strValue )
		return E_INVALIDARG;

	DWORD cbValue = ((DWORD)_tcslen(strValue)+1) * sizeof(TCHAR);

	if( ERROR_SUCCESS != RegSetValueEx( hKey, strRegName, 0, REG_SZ, 
		(BYTE*)strValue, cbValue ) )
		return E_FAIL;

	return S_OK;
}



