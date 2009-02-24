#ifndef _GLWINDOW_H
#define _GLWINDOW_H

#include <vector>
#include "Common.h"
#include "EventHandler.h"
#include "Globals.h"
#include "Config.h"
#include "pluginspecs_video.h"

#ifdef _WIN32
#define GLEW_STATIC

#include <GLew/glew.h>
#include <GLew/wglew.h>
#include <GLew/gl.h>
#include <GLew/glext.h>
#else
#include <GL/glew.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
enum OGL_Props {
	OGL_FULLSCREEN,
	OGL_KEEPRATIO,
	OGL_HIDECURSOR,
	OGL_PROP_COUNT
};

struct res {
	u32 x;
	u32 y;
};

class GLWindow {
private:

	// TODO: what is xmax and ymax? do we need [xy]render?
	u32 xWin, yWin;			// Windows' size
	int xOffset, yOffset;	// Offset in window
	float xMax, yMax;		// ???
	u32 xRender, yRender;	// Render area

	bool properties[OGL_PROP_COUNT];

protected:

	EventHandler* eventHandler;
	res origRes, currFullRes, currWinRes;
	static std::vector<res> fullResolutions;
	virtual void SetRender(u32 x, u32 y) {
		xRender = x;
		yRender = y;
	}

	static const std::vector<res>& getFsResolutions() {
		return fullResolutions;
	}

	static void addFSResolution(res fsr) {
		fullResolutions.push_back(fsr);
	}
public:

	virtual void SwapBuffers() {};
	virtual void SetWindowText(const char *text) {};
	virtual bool PeekMessages() {return false;};
	virtual void Update() {};
	virtual bool MakeCurrent() {return false;};

	virtual void updateDim()
	{
		if (GetProperty(OGL_FULLSCREEN))
			SetWinSize(currFullRes.x, currFullRes.y);
		else
			// Set the windowed resolution
			SetWinSize(currWinRes.x, currWinRes.y);

		float FactorX = 640.0f / (float)GetXwin();
		float FactorY = 480.0f / (float)GetYwin();
		//float Max = (FactorX < FactorY) ? FactorX : FactorY;

		SetMax(1.0f / FactorX, 1.0f / FactorY);
		SetOffset(0,0);
	}

	void SetEventHandler(EventHandler *eh) { eventHandler = eh;}
	bool GetProperty(OGL_Props prop) {return properties[prop];}
	virtual bool SetProperty(OGL_Props prop, bool value)
	{return properties[prop] = value;}

	u32 GetXrender() {return xRender;}
	u32 GetYrender() {return yRender;}

	u32 GetXwin() {return xWin;}
	u32 GetYwin() {return yWin;}
	void SetWinSize(u32 x, u32 y) {
		xWin = x;
		yWin = y;
	}

	int GetYoff() {return yOffset;}
	int GetXoff() {return xOffset;}
	void SetOffset(int x, int y) {
		yOffset = y;
		xOffset = x;
	}

	void SetMax(float x, float y) {
		yMax = y;
		xMax = x;
	}

	float GetXmax() {return xMax;}
	float GetYmax() {return yMax;}

	static bool valid() { return false;}

	GLWindow()
	{
		// Load defaults
		sscanf(g_Config.iFSResolution, "%dx%d",
			&currFullRes.x, &currFullRes.y);  

		sscanf(g_Config.iWindowedRes, "%dx%d",
			&currWinRes.x, &currWinRes.y);

		SetProperty(OGL_FULLSCREEN, g_Config.bFullscreen);
		SetProperty(OGL_KEEPRATIO, g_Config.bKeepAR43);
		SetProperty(OGL_HIDECURSOR, g_Config.bHideCursor);

		updateDim();
	}


	// setResolution
	// resolution iter
};

#endif
