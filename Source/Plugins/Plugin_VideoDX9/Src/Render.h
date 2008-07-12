#pragma	once

#include "PluginSpecs_Video.h"

class Renderer
{
	// screen offset
	static float m_x,m_y,m_width, m_height,xScale,yScale;

public:

	static void Init(SVideoInitialize &_VideoInitialize);
	static void Shutdown();

	// initialize opengl standard values (like viewport)
	static void Initialize(void);
	// must be called if the windowsize has changed
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
};
