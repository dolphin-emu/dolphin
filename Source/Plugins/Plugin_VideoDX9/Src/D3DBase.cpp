#include "D3DBase.h"
#include "Render.h"


namespace D3D
{
	bool fullScreen = false, nextFullScreen=false;
	LPDIRECT3D9             D3D = NULL; // Used to create the D3DDevice
	LPDIRECT3DDEVICE9       dev = NULL; // Our rendering device
	LPDIRECT3DSURFACE9      backBuffer;
	D3DCAPS9 caps;
	int multisample;
	int resolution;

#define VENDOR_NVIDIA 4318

	RECT client;
	HWND hWnd;
	int xres, yres;
	int cur_adapter;
	Shader Ps;
	Shader Vs;

	bool bFrameInProgress = false;

	//enum shit
	Adapter adapters[4];
	int numAdapters;

	void Enumerate();

	int GetNumAdapters()
	{
		return numAdapters;
	}

	const Adapter &GetAdapter(int i)
	{
		return adapters[i];
	}

	const Adapter &GetCurAdapter()
	{
		return adapters[cur_adapter];
	}

	HRESULT Init()
	{
        // Create the D3D object, which is needed to create the D3DDevice.
		if( NULL == ( D3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
			return E_FAIL;

		Enumerate();
		return S_OK;
	}

	void EnableAlphaToCoverage()
	{
		// dev->SetRenderState(D3DRS_ADAPTIVETESS_Y, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));
		Renderer::SetRenderState( D3DRS_ADAPTIVETESS_Y, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C') );
	}

	void InitPP(int adapter, int resolution, int aa_mode, D3DPRESENT_PARAMETERS *pp)
	{
		int FSResX = adapters[adapter].resolutions[resolution].xres; 
		int FSResY = adapters[adapter].resolutions[resolution].yres;

		ZeroMemory(pp, sizeof(D3DPRESENT_PARAMETERS));
		pp->hDeviceWindow = hWnd;
		pp->EnableAutoDepthStencil = TRUE;
		pp->AutoDepthStencilFormat = D3DFMT_D24S8;
		pp->BackBufferFormat = D3DFMT_A8R8G8B8;
		if (aa_mode >= (int)adapters[adapter].aa_levels.size())
			aa_mode = 0;

		pp->MultiSampleType = adapters[adapter].aa_levels[aa_mode].ms_setting;
		pp->MultiSampleQuality = adapters[adapter].aa_levels[aa_mode].qual_setting;
		pp->Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL; 
		
		//D3DPRESENTFLAG_LOCKABLE_BACKBUFFER

		if (fullScreen)
		{
			xres = pp->BackBufferWidth = FSResX;
			yres = pp->BackBufferHeight = FSResY;	
			pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
			pp->Windowed = FALSE;
		}
		else
		{
			GetClientRect(hWnd, &client);
			xres = pp->BackBufferWidth  = client.right - client.left;
			yres = pp->BackBufferHeight = client.bottom - client.top;
			pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
			pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
			pp->Windowed = TRUE;
		}
	}

	void Enumerate()
	{
		numAdapters = D3D::D3D->GetAdapterCount();
		
		for (int i=0; i<numAdapters; i++)
		{
			Adapter &a = adapters[i];
			D3D::D3D->GetAdapterIdentifier(i, 0, &a.ident);

			bool isNvidia = a.ident.VendorId == VENDOR_NVIDIA;

			// Add multisample modes
			a.aa_levels.push_back(AALevel("None", D3DMULTISAMPLE_NONE, 0));

			DWORD qlevels = 0;
			if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
				i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES, &qlevels))
				if (qlevels > 0)
					a.aa_levels.push_back(AALevel("2x MSAA", D3DMULTISAMPLE_2_SAMPLES, 0));
			
			if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
				i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES, &qlevels))
				if (qlevels > 0)
					a.aa_levels.push_back(AALevel("4x MSAA", D3DMULTISAMPLE_4_SAMPLES, 0));

			if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
				i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_8_SAMPLES, &qlevels))
				if (qlevels > 0)
					a.aa_levels.push_back(AALevel("8x MSAA", D3DMULTISAMPLE_8_SAMPLES, 0));

			if (isNvidia)
			{
				// CSAA support
				if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
					i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, &qlevels))
				{
					if (qlevels > 2)
					{
						// 8x, 8xQ are available
						// See http://developer.nvidia.com/object/coverage-sampled-aa.html
						a.aa_levels.push_back(AALevel("8x CSAA", D3DMULTISAMPLE_4_SAMPLES, 2));
						a.aa_levels.push_back(AALevel("8xQ CSAA", D3DMULTISAMPLE_8_SAMPLES, 0));
					}
				}
				if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
					i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_8_SAMPLES, &qlevels)) 
				{
					if (qlevels > 2)
					{
						// 8x, 8xQ are available
						// See http://developer.nvidia.com/object/coverage-sampled-aa.html
						a.aa_levels.push_back(AALevel("16x CSAA", D3DMULTISAMPLE_4_SAMPLES, 4));
						a.aa_levels.push_back(AALevel("16xQ CSAA", D3DMULTISAMPLE_8_SAMPLES, 2));
					}
				}
			}

			if (a.aa_levels.size() == 1)
			{
				strcpy(a.aa_levels[0].name, "(Not supported on this device)");
			}

			int numModes = D3D::D3D->GetAdapterModeCount(i, D3DFMT_X8R8G8B8);

			for (int m = 0; m < numModes; m++)
			{	
				D3DDISPLAYMODE mode;
				D3D::D3D->EnumAdapterModes(i, D3DFMT_X8R8G8B8, m, &mode);
				
				int found = -1;
				for (int x = 0; x < (int)a.resolutions.size(); x++)
				{
					if (a.resolutions[x].xres == mode.Width && a.resolutions[x].yres == mode.Height)
					{
						found = x;
						break;
					}
				}

				Resolution temp;
				Resolution &r = found==-1 ? temp : a.resolutions[found];
				
				sprintf(r.name, "%ix%i", mode.Width, mode.Height);
				r.bitdepths.insert(mode.Format);
				r.refreshes.insert(mode.RefreshRate);
				if (found == -1 && mode.Width >= 640 && mode.Height >= 480)
				{
					r.xres = mode.Width;
					r.yres = mode.Height;
					a.resolutions.push_back(r);
				}
			}
		}
	}

	HRESULT Create(int adapter, HWND wnd, bool _fullscreen, int _resolution, int aa_mode)
	{
		hWnd = wnd;
		fullScreen = _fullscreen;
		nextFullScreen = _fullscreen;
		multisample = aa_mode;
		resolution = _resolution;
		cur_adapter = adapter;
		D3DPRESENT_PARAMETERS d3dpp; 
		InitPP(adapter, resolution, aa_mode, &d3dpp);

		if( FAILED( D3D->CreateDevice( 
			adapter, 
			D3DDEVTYPE_HAL, 
			wnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
			// |D3DCREATE_MULTITHREADED /* | D3DCREATE_PUREDEVICE*/,
			//D3DCREATE_SOFTWARE_VERTEXPROCESSING ,
			&d3dpp, &dev ) ) )
		{
			MessageBox(wnd,
				"Sorry, but it looks like your 3D accelerator is too old,\n"
				"or doesn't support features that Dolphin requires.\n"
				"Falling back to software vertex processing.\n", 
				"Dolphin Direct3D plugin", MB_OK | MB_ICONERROR);
			if( FAILED( D3D->CreateDevice( 
				adapter, 
				D3DDEVTYPE_HAL, 
				wnd,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
				// |D3DCREATE_MULTITHREADED /* | D3DCREATE_PUREDEVICE*/,
				//D3DCREATE_SOFTWARE_VERTEXPROCESSING ,
				&d3dpp, &dev ) ) )
			{
				MessageBox(wnd,
					"Software VP failed too. Upgrade your graphics card.", 
					"Dolphin Direct3D plugin", MB_OK | MB_ICONERROR);
				return E_FAIL;
			}
		}
		dev->GetDeviceCaps(&caps);
		dev->GetRenderTarget(0,&backBuffer);

		Ps.Major = (D3D::caps.PixelShaderVersion >> 8) & 0xFF;
		Ps.Minor = (D3D::caps.PixelShaderVersion) & 0xFF;
		Vs.Major = (D3D::caps.VertexShaderVersion >>8) & 0xFF;
		Vs.Minor = (D3D::caps.VertexShaderVersion) & 0xFF;

		// Device state would normally be set here
		return S_OK;
	}

	ShaderVersion GetShaderVersion()
	{
		if (Ps.Major < 2)
		{
			return PSNONE;
		}
		
		//good enough estimate - we really only 
		//care about zero shader vs ps20
		return (ShaderVersion)Ps.Major;
	}

	void Close()
	{
		dev->Release(); 
		dev = 0;
	}

	void Shutdown()
	{
		D3D->Release();
		D3D = 0;
	}

	const D3DCAPS9 &GetCaps()
	{
		return caps;
	}

	LPDIRECT3DSURFACE9 GetBackBufferSurface()
	{
		return backBuffer;
	}

	void ShowD3DError(HRESULT err)
	{
		switch (err) 
		{
		case D3DERR_DEVICELOST:
			MessageBox(0, "Device Lost", "D3D ERROR", 0);
			break;
		case D3DERR_INVALIDCALL:
			MessageBox(0, "Invalid Call", "D3D ERROR", 0);
			break;
		case D3DERR_DRIVERINTERNALERROR:
			MessageBox(0, "Driver Internal Error", "D3D ERROR", 0);
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			MessageBox(0, "Out of vid mem", "D3D ERROR", 0);
			break;
		default:
			// MessageBox(0,"Other error or success","ERROR",0);
			break;
		}
	}

	void Reset()
	{
		if (dev)
		{
			D3DPRESENT_PARAMETERS d3dpp; 
			InitPP(cur_adapter, resolution, multisample, &d3dpp);
			HRESULT hr = dev->Reset(&d3dpp);
			ShowD3DError(hr);
		}
	}

	bool IsFullscreen()
	{
		return fullScreen;
	}

	int GetDisplayWidth()	
	{
		return xres;
	}
	int GetDisplayHeight()
	{
		return yres;
	}
	void SwitchFullscreen(bool fullscreen)
	{
		nextFullScreen = fullscreen;
	}

	bool BeginFrame(bool clear, u32 color, float z)
	{
		if (bFrameInProgress)
		{
			return false;
		}

		bFrameInProgress = true;

		if (dev)
		{
			if (clear)
				dev->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, (DWORD)color, z, 0 );
			dev->BeginScene();
			return true;
		}
		else 
			return false;
	}

	void EndFrame()
	{
		if (!bFrameInProgress)
			return;

		bFrameInProgress = false;

		if (dev)
		{
			dev->EndScene();
			dev->Present( NULL, NULL, NULL, NULL );
		}

		if (fullScreen != nextFullScreen)
		{
			fullScreen = nextFullScreen;
			Reset();
		}
	}

}
