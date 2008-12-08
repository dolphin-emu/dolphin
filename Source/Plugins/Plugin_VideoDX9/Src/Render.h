// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef __H_RENDER__
#define __H_RENDER__

#include "PluginSpecs_Video.h"
#include <vector>
#include <string>
#include "D3DBase.h"


class Renderer
{
	// screen offset
	static float m_x,m_y,m_width, m_height,xScale,yScale;
	const static int MaxTextureStages = 9;
	const static int MaxRenderStates = 210;
	const static DWORD MaxTextureTypes = 33;
	const static DWORD MaxSamplerSize = 13;
	const static DWORD MaxSamplerTypes = 15;
	static std::vector<LPDIRECT3DBASETEXTURE9> m_Textures;
	static DWORD m_RenderStates[MaxRenderStates];
	static DWORD m_TextureStageStates[MaxTextureStages][MaxTextureTypes];
	static DWORD m_SamplerStates[MaxSamplerSize][MaxSamplerTypes];
	static DWORD m_FVF;

public:

	static void Init(SVideoInitialize &_VideoInitialize);
	static void Shutdown();

	// initialize opengl standard values (like view port)
	static void Initialize(void);
	// must be called if the window size has changed
	static void ReinitView(void);
	//
	// --- Render Functions ---
	//
	static void SwapBuffers(void);
	static void Flush(void);
	static float GetXScale(){return xScale;}
	static float GetYScale(){return yScale;}

	static void SetScissorBox(RECT &rc);
	static void SetViewport(float* _Viewport);
	static void SetProjection(float* _pProjection, int constantIndex = -1);


	static void AddMessage(const std::string &message, unsigned int ms);
	static void ProcessMessages();
	static void RenderText(const std::string &text, int left, int top, unsigned int color);

	/**
	 * Assigns a texture to a device stage.
	 * @param Stage Stage to assign to.
	 * @param pTexture Texture to be assigned.
	 */
	static void SetTexture( DWORD Stage, IDirect3DBaseTexture9 *pTexture );

	/**
	 * Sets the current vertex stream declaration.
	 * @param FVF Fixed function vertex type
	 */
	static void SetFVF( DWORD FVF );

	/**
	 * Sets a single device render-state parameter.
	 * @param State Device state variable that is being modified.
	 * @param Value New value for the device render state to be set.
	 */
	static void SetRenderState( D3DRENDERSTATETYPE State, DWORD Value );

	/**
	 * Sets the state value for the currently assigned texture.
	 * @param Stage Stage identifier of the texture for which the state value is set.
	 * @param Type Texture state to set.
	 * @param Value State value to set.
	 */
	static void SetTextureStageState( DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,DWORD Value );

	/**
	 * Sets the sampler state value.
	 * @param Sampler The sampler stage index.
	 * @param Type Type of the sampler.
	 * @param Value State value to set. 
	 */
	static void SetSamplerState( DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value );

	/**
	 * Renders data specified by a user memory pointer as a sequence of geometric primitives of the specified type.
	 * @param PrimitiveType Type of primitive to render.
	 * @param PrimitiveCount Number of primitives to render.
	 * @param pVertexStreamZeroData User memory pointer to the vertex data.
	 * @param VertexStreamZeroStride The number of bytes of data for each vertex.
	 */
	static void DrawPrimitiveUP( D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount,
		const void* pVertexStreamZeroData, UINT VertexStreamZeroStride );

	/**
	 * Renders a sequence of non indexed, geometric primitives of the specified type from the current set of data input streams.
	 * @param PrimitiveType Type of primitive to render. 
	 * @param StartVertex Index of the first vertex to load.
	 * @param PrimitiveCount Number of primitives to render.
	 */
	static void DrawPrimitive( D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount );
};

#endif	// __H_RENDER__
