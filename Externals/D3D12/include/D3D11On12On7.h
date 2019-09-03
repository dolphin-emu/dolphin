#pragma once

// Just an identity interface, no functionality exposed, retrieved by ID3D11On12On7::GetThreadLastCreatedResource
interface ID3D11On12On7Resource { };

// Per-device, retrieved by ID3D11On12On7::GetThreadLastCreatedDevice
interface ID3D11On12On7Device
{
    // Equivalent to ID3D11On12Device::AcquireWrappedResources:
    // From this point on, the resource should be used by D3D11, not by D3D12 directly.
    STDMETHOD(AcquireResource)(ID3D11On12On7Resource* pResource, D3D12_RESOURCE_STATES state) = 0;
    // Equivalent to ID3D11On12Device::ReleaseWrappedResources:
    // Note that this cannot unbind the resource from D3D11, that is the app's responsibility.
    // After calling this, the app should call Flush() on the D3D11 device.
    STDMETHOD(ReleaseResource)(ID3D11On12On7Resource* pResource, D3D12_RESOURCE_STATES state) = 0;
};

// Global, retrieved by GetD3D11On12On7Interface
interface ID3D11On12On7 : public IUnknown
{
    // Enables usage similar to D3D11On12CreateDevice.
    STDMETHOD_(void, SetThreadDeviceCreationParams)(ID3D12Device* pDevice, ID3D12CommandQueue* pGraphicsQueue) = 0;
    // Enables usage similar to ID3D11On12Device::CreateWrappedResource.
    // Note that the D3D11 resource creation parameters should be similar to the D3D12 resource,
    // or else unexpected/undefined behavior may occur.
    STDMETHOD_(void, SetThreadResourceCreationParams)(ID3D12Resource* pResource) = 0;
    STDMETHOD_(ID3D11On12On7Device*, GetThreadLastCreatedDevice)() = 0;
    STDMETHOD_(ID3D11On12On7Resource*, GetThreadLastCreatedResource)() = 0;
};

extern "C" HRESULT APIENTRY GetD3D11On12On7Interface(ID3D11On12On7** ppIface);
typedef HRESULT(APIENTRY *PFNGetD3D11On12On7Interface)(ID3D11On12On7** ppIface);