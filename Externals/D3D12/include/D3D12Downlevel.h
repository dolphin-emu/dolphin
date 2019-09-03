/*-------------------------------------------------------------------------------------
 *
 * Copyright (c) Microsoft Corporation
 *
 *-------------------------------------------------------------------------------------*/


/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __d3d12downlevel_h__
#define __d3d12downlevel_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ID3D12CommandQueueDownlevel_FWD_DEFINED__
#define __ID3D12CommandQueueDownlevel_FWD_DEFINED__
typedef interface ID3D12CommandQueueDownlevel ID3D12CommandQueueDownlevel;

#endif 	/* __ID3D12CommandQueueDownlevel_FWD_DEFINED__ */


#ifndef __ID3D12DeviceDownlevel_FWD_DEFINED__
#define __ID3D12DeviceDownlevel_FWD_DEFINED__
typedef interface ID3D12DeviceDownlevel ID3D12DeviceDownlevel;

#endif 	/* __ID3D12DeviceDownlevel_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "d3d12.h"
#include "dxgi1_4.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_d3d12downlevel_0000_0000 */
/* [local] */ 

#include <winapifamily.h>
#pragma region Desktop Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
typedef 
enum D3D12_DOWNLEVEL_PRESENT_FLAGS
    {
        D3D12_DOWNLEVEL_PRESENT_FLAG_NONE	= 0,
        D3D12_DOWNLEVEL_PRESENT_FLAG_WAIT_FOR_VBLANK	= ( D3D12_DOWNLEVEL_PRESENT_FLAG_NONE + 1 ) 
    } 	D3D12_DOWNLEVEL_PRESENT_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS( D3D12_DOWNLEVEL_PRESENT_FLAGS );


extern RPC_IF_HANDLE __MIDL_itf_d3d12downlevel_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_d3d12downlevel_0000_0000_v0_0_s_ifspec;

#ifndef __ID3D12CommandQueueDownlevel_INTERFACE_DEFINED__
#define __ID3D12CommandQueueDownlevel_INTERFACE_DEFINED__

/* interface ID3D12CommandQueueDownlevel */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_ID3D12CommandQueueDownlevel;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("38a8c5ef-7ccb-4e81-914f-a6e9d072c494")
    ID3D12CommandQueueDownlevel : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Present( 
            _In_  ID3D12GraphicsCommandList *pOpenCommandList,
            _In_  ID3D12Resource *pSourceTex2D,
            _In_  HWND hWindow,
            D3D12_DOWNLEVEL_PRESENT_FLAGS Flags) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ID3D12CommandQueueDownlevelVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ID3D12CommandQueueDownlevel * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ID3D12CommandQueueDownlevel * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ID3D12CommandQueueDownlevel * This);
        
        HRESULT ( STDMETHODCALLTYPE *Present )( 
            ID3D12CommandQueueDownlevel * This,
            _In_  ID3D12GraphicsCommandList *pOpenCommandList,
            _In_  ID3D12Resource *pSourceTex2D,
            _In_  HWND hWindow,
            D3D12_DOWNLEVEL_PRESENT_FLAGS Flags);
        
        END_INTERFACE
    } ID3D12CommandQueueDownlevelVtbl;

    interface ID3D12CommandQueueDownlevel
    {
        CONST_VTBL struct ID3D12CommandQueueDownlevelVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ID3D12CommandQueueDownlevel_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ID3D12CommandQueueDownlevel_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ID3D12CommandQueueDownlevel_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ID3D12CommandQueueDownlevel_Present(This,pOpenCommandList,pSourceTex2D,hWindow,Flags)	\
    ( (This)->lpVtbl -> Present(This,pOpenCommandList,pSourceTex2D,hWindow,Flags) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ID3D12CommandQueueDownlevel_INTERFACE_DEFINED__ */


#ifndef __ID3D12DeviceDownlevel_INTERFACE_DEFINED__
#define __ID3D12DeviceDownlevel_INTERFACE_DEFINED__

/* interface ID3D12DeviceDownlevel */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_ID3D12DeviceDownlevel;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("74eaee3f-2f4b-476d-82ba-2b85cb49e310")
    ID3D12DeviceDownlevel : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo( 
            UINT NodeIndex,
            DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
            _Out_  DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ID3D12DeviceDownlevelVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ID3D12DeviceDownlevel * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ID3D12DeviceDownlevel * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ID3D12DeviceDownlevel * This);
        
        HRESULT ( STDMETHODCALLTYPE *QueryVideoMemoryInfo )( 
            ID3D12DeviceDownlevel * This,
            UINT NodeIndex,
            DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
            _Out_  DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo);
        
        END_INTERFACE
    } ID3D12DeviceDownlevelVtbl;

    interface ID3D12DeviceDownlevel
    {
        CONST_VTBL struct ID3D12DeviceDownlevelVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ID3D12DeviceDownlevel_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ID3D12DeviceDownlevel_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ID3D12DeviceDownlevel_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ID3D12DeviceDownlevel_QueryVideoMemoryInfo(This,NodeIndex,MemorySegmentGroup,pVideoMemoryInfo)	\
    ( (This)->lpVtbl -> QueryVideoMemoryInfo(This,NodeIndex,MemorySegmentGroup,pVideoMemoryInfo) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ID3D12DeviceDownlevel_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_d3d12downlevel_0000_0002 */
/* [local] */ 

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
#pragma endregion
DEFINE_GUID(IID_ID3D12CommandQueueDownlevel,0x38a8c5ef,0x7ccb,0x4e81,0x91,0x4f,0xa6,0xe9,0xd0,0x72,0xc4,0x94);
DEFINE_GUID(IID_ID3D12DeviceDownlevel,0x74eaee3f,0x2f4b,0x476d,0x82,0xba,0x2b,0x85,0xcb,0x49,0xe3,0x10);


extern RPC_IF_HANDLE __MIDL_itf_d3d12downlevel_0000_0002_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_d3d12downlevel_0000_0002_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


